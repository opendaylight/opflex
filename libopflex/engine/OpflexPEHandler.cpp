/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexPEHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <vector>
#include <utility>

#include <rapidjson/document.h>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "opflex/engine/Processor.h"
#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexPEHandler.h"
#include "opflex/logging/internal/logging.hpp"
#include "opflex/engine/internal/MOSerializer.h"
#include "opflex/engine/internal/OpflexMessage.h"

namespace opflex {
namespace engine {
namespace internal {

using std::vector;
using std::string;
using std::pair;
using modb::class_id_t;
using modb::reference_t;
using modb::URI;
using internal::MOSerializer;
using modb::mointernal::StoreClient;
using rapidjson::Value;
using rapidjson::Writer;

class SendIdentityReq : public OpflexMessage {
public:
    SendIdentityReq(const std::string& name_,
                    const std::string& domain_,
                    const uint8_t roles_)
        : OpflexMessage("send_identity", REQUEST),
          name(name_), domain(domain_), roles(roles_) {}

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(Writer<T> & writer) {
        writer.StartArray();
        writer.StartObject();
        writer.String("proto_version");
        writer.String("1.0");
        writer.String("name");
        writer.String(name.c_str());
        writer.String("domain");
        writer.String(domain.c_str());
        writer.String("my_role");
        writer.StartArray();
        if (roles & OpflexHandler::POLICY_ELEMENT)
            writer.String("policy_element");
        if (roles & OpflexHandler::POLICY_REPOSITORY)
            writer.String("policy_repository");
        if (roles & OpflexHandler::ENDPOINT_REGISTRY)
            writer.String("endpoint_registry");
        if (roles & OpflexHandler::OBSERVER)
            writer.String("observer");
        writer.EndArray();
        writer.EndObject();
        writer.EndArray();

        return true;
    }

private:
    std::string name;
    std::string domain;
    uint8_t roles;
};

void OpflexPEHandler::connected() {
    setState(CONNECTED);

    OpflexPool& pool = getProcessor()->getPool();
    SendIdentityReq req(pool.getName(),
                        pool.getDomain(),
                        OpflexHandler::POLICY_ELEMENT);
    getConnection()->write(req.serialize());
}

void OpflexPEHandler::disconnected() {
    setState(DISCONNECTED);
    OpflexPool& pool = getProcessor()->getPool();
    OpflexClientConnection* conn = (OpflexClientConnection*)getConnection();
    pool.setRoles(conn, 0);
}

void OpflexPEHandler::ready() {
    LOG(INFO) << "[" << getConnection()->getRemotePeer() << "] " 
              << "Handshake succeeded";

    setState(READY);
    getProcessor()->connectionReady(getConnection());
}

void OpflexPEHandler::handleSendIdentityRes(const rapidjson::Value& id,
                                            const Value& payload) {
    OpflexPool& pool = getProcessor()->getPool();
    OpflexClientConnection* conn = (OpflexClientConnection*)getConnection();

    bool foundSelf = false;

    if (payload.HasMember("peers")) {
        const Value& peers = payload["peers"];
        if (!peers.IsArray()) {
            LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] " 
                       << "Malformed peers: must be array";
            conn->disconnect();
            return;
        }

        Value::ConstValueIterator it;
        for (it = peers.Begin(); it != peers.End(); ++it) {
            if (!it->IsObject()) {
                LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] " 
                           << "Malformed peers: must contain objects";
                conn->disconnect();
                return;
            }
            if (!it->HasMember("connectivity_info"))
                continue;
            const Value& civ = (*it)["connectivity_info"];
            if (!civ.IsString())
                continue;
                
            string ci = civ.GetString();
            size_t p = ci.find_last_of(':');
            if (p != string::npos) {
                string host = ci.substr(0, p);
                string portstr = ci.substr(p + 1);
                int port;
                try {
                    port = boost::lexical_cast<int>( portstr );
                    if (pool.getPeer(host, port) == NULL)
                        pool.addPeer(host, port);
                        
                    if (host == conn->getHostname() &&
                        port == conn->getPort())
                        foundSelf = true;
                } catch( boost::bad_lexical_cast const& ) {
                    LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] " 
                               << "Invalid port in connectivity_info: " 
                               << portstr;
                    conn->disconnect();
                    return;
                }
            } else {
                LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] " 
                           << "Connectivity info could not be parsed: " 
                           << ci;
                conn->disconnect();
                return;
            }
        }
    }

    if (foundSelf) {
        uint8_t peerRoles = 0;
        if (payload.HasMember("my_role")) {
            const Value& peerRolev = payload["my_role"];
            if (peerRolev.IsArray()) {
                Value::ConstValueIterator it;
                for (it = peerRolev.Begin(); it != peerRolev.End(); ++it) {
                    if (!it->IsString())
                        continue;

                    string rolestr = it->GetString();
                    if (rolestr == "policy_element") {
                        peerRoles |= OpflexHandler::POLICY_ELEMENT;
                    } else if (rolestr == "policy_repository") {
                        peerRoles |= OpflexHandler::POLICY_REPOSITORY;
                    } else if (rolestr == "endpoint_registry") {
                        peerRoles |= OpflexHandler::ENDPOINT_REGISTRY;
                    } else if (rolestr == "observer") {
                        peerRoles |= OpflexHandler::OBSERVER;
                    }
                }
            }
        }
        pool.setRoles(conn, peerRoles);
        ready();
    } else {
        LOG(INFO) << "[" << getConnection()->getRemotePeer() << "] " 
                  << "Current peer not found in peer list; disconnecting";
        conn->disconnect();
    }
}

void OpflexPEHandler::handlePolicyResolveRes(const rapidjson::Value& id,
                                             const Value& payload) {
    StoreClient* client = getProcessor()->getSystemClient();
    MOSerializer& serializer = getProcessor()->getSerializer();
    StoreClient::notif_t notifs;
    if (payload.HasMember("policy")) {
        const Value& policy = payload["policy"];
        if (!policy.IsArray()) {
            LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] " 
                       << "Malformed policy resolve response: policy must be array";
            conn->disconnect();
        }

        Value::ConstValueIterator it;
        for (it = policy.Begin(); it != policy.End(); ++it) {
            const Value& mo = *it;
            serializer.deserialize(mo, *client, true, &notifs);
        }
    }
    client->deliverNotifications(notifs);
}

void OpflexPEHandler::handlePolicyUpdateReq(const rapidjson::Value& id,
                                            const rapidjson::Value& payload) {
    StoreClient* client = getProcessor()->getSystemClient();
    MOSerializer& serializer = getProcessor()->getSerializer();
    StoreClient::notif_t notifs;

    Value::ConstValueIterator it;
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: payload array contains a nonobject");
            return;
        }

        if (it->HasMember("replace")) {
            const Value& replace = (*it)["replace"];
            if (!replace.IsArray()) {
                sendErrorRes(id, "ERROR", 
                             "Malformed message: replace is not an array");
                return;
            }
            Value::ConstValueIterator it;
            for (it = replace.Begin(); it != replace.End(); ++it) {
                const Value& mo = *it;
                serializer.deserialize(mo, *client, true, &notifs);
            }
        }
        if (it->HasMember("merge_children")) {
            const Value& merge = (*it)["merge_children"];
            if (!merge.IsArray()) {
                sendErrorRes(id, "ERROR", 
                             "Malformed message: merge_children is not an array");
                return;
            }
            Value::ConstValueIterator it;
            for (it = merge.Begin(); it != merge.End(); ++it) {
                const Value& mo = *it;
                serializer.deserialize(mo, *client, false, &notifs);
            }
        }
        if (it->HasMember("delete")) {
            const Value& del = (*it)["delete"];
            if (!del.IsArray()) {
                sendErrorRes(id, "ERROR", 
                             "Malformed message: delete is not an array");
                return;
            }
            Value::ConstValueIterator dit;
            for (dit = del.Begin(); dit != del.End(); ++dit) {
                if (!dit->IsObject()) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: delete contains a non-object");
                    return;
                }
                if (!dit->HasMember("subject")) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: subject missing from delete");
                    return;
                }
                if (!dit->HasMember("uri")) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: uri missing from delete");
                    return;
                }

                const Value& subjectv = (*dit)["subject"];
                const Value& puriv = (*dit)["uri"];
                if (!subjectv.IsString()) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: subject is not a string");
                    return;
                }
                if (!puriv.IsString()) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: uri is not a string");
                    return;
                }

                try {
                    const modb::ClassInfo& ci = 
                        getProcessor()->getStore()->getClassInfo(subjectv.GetString());
                    modb::URI puri(puriv.GetString());
                    client->remove(ci.getId(), puri, false, &notifs);
                    client->queueNotification(ci.getId(), puri, notifs);
                } catch (std::out_of_range e) {
                    sendErrorRes(id, "ERROR", 
                                 std::string("Unknown subject: ") + 
                                 subjectv.GetString());
                    return;
                }
            }
        }
    }

    client->deliverNotifications(notifs);
}

void OpflexPEHandler::handlePolicyUnresolveRes(const rapidjson::Value& id,
                                               const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPDeclareRes(const rapidjson::Value& id,
                                         const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPUndeclareRes(const rapidjson::Value& id,
                                           const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPResolveRes(const rapidjson::Value& id,
                                         const rapidjson::Value& payload) {
    StoreClient* client = getProcessor()->getSystemClient();
    MOSerializer& serializer = getProcessor()->getSerializer();
    StoreClient::notif_t notifs;
    if (payload.HasMember("endpoint")) {
        const Value& endpoint = payload["endpoint"];
        if (!endpoint.IsArray()) {
            LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] " 
                       << "Malformed endpoint resolve response: endpoint must be array";
            conn->disconnect();
        }

        Value::ConstValueIterator it;
        for (it = endpoint.Begin(); it != endpoint.End(); ++it) {
            const Value& mo = *it;
            serializer.deserialize(mo, *client, true, &notifs);
        }
    }
    client->deliverNotifications(notifs);
}

void OpflexPEHandler::handleEPUnresolveRes(const rapidjson::Value& id,
                                           const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPUpdateReq(const rapidjson::Value& id,
                                        const rapidjson::Value& payload) {
    StoreClient* client = getProcessor()->getSystemClient();
    MOSerializer& serializer = getProcessor()->getSerializer();
    StoreClient::notif_t notifs;

    Value::ConstValueIterator it;
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: payload array contains a nonobject");
            return;
        }

        if (it->HasMember("replace")) {
            const Value& replace = (*it)["replace"];
            if (!replace.IsArray()) {
                sendErrorRes(id, "ERROR", 
                             "Malformed message: replace is not an array");
                return;
            }
            Value::ConstValueIterator it;
            for (it = replace.Begin(); it != replace.End(); ++it) {
                const Value& mo = *it;
                serializer.deserialize(mo, *client, true, &notifs);
            }
        }
        if (it->HasMember("delete")) {
            const Value& del = (*it)["delete"];
            if (!del.IsArray()) {
                sendErrorRes(id, "ERROR", 
                             "Malformed message: delete is not an array");
                return;
            }
            Value::ConstValueIterator dit;
            for (dit = del.Begin(); dit != del.End(); ++dit) {
                if (!dit->IsObject()) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: delete contains a non-object");
                    return;
                }
                if (!dit->HasMember("subject")) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: subject missing from delete");
                    return;
                }
                if (!dit->HasMember("uri")) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: uri missing from delete");
                    return;
                }

                const Value& subjectv = (*dit)["subject"];
                const Value& puriv = (*dit)["uri"];
                if (!subjectv.IsString()) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: subject is not a string");
                    return;
                }
                if (!puriv.IsString()) {
                    sendErrorRes(id, "ERROR", 
                                 "Malformed message: uri is not a string");
                    return;
                }

                try {
                    const modb::ClassInfo& ci = 
                        getProcessor()->getStore()->getClassInfo(subjectv.GetString());
                    modb::URI puri(puriv.GetString());
                    client->remove(ci.getId(), puri, false, &notifs);
                    client->queueNotification(ci.getId(), puri, notifs);
                } catch (std::out_of_range e) {
                    sendErrorRes(id, "ERROR", 
                                 std::string("Unknown subject: ") + 
                                 subjectv.GetString());
                    return;
                }
            }
        }
    }

    client->deliverNotifications(notifs);
}

void OpflexPEHandler::handleStateReportRes(const rapidjson::Value& id,
                                           const rapidjson::Value& payload) {
    // nothing to do
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
