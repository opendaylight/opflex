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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <string>
#include <vector>
#include <utility>

#include <rapidjson/document.h>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/address_v4.hpp>

#include "opflex/engine/Processor.h"
#include "opflex/engine/internal/OpflexPool.h"
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
using boost::optional;
using modb::class_id_t;
using modb::reference_t;
using modb::URI;
using internal::MOSerializer;
using modb::mointernal::StoreClient;
using rapidjson::Value;
using rapidjson::Writer;
using ofcore::OFConstants;
using boost::asio::ip::address_v4;

typedef opflex::ofcore::OFConstants::OpflexElementMode AgentMode;
typedef opflex::ofcore::OFConstants::OpflexTransportModeState AgentTransportState;

class SendIdentityReq : public OpflexMessage {
public:
    SendIdentityReq(const string& name_,
                    const string& domain_,
                    const optional<string>& location_,
                    const uint8_t roles_,
                    const string& mac_)
        : OpflexMessage("send_identity", REQUEST),
          name(name_), domain(domain_), location(location_), roles(roles_),
          mac(mac_) {}

    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual SendIdentityReq* clone() {
        return new SendIdentityReq(*this);
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
        if (location) {
            writer.String("my_location");
            writer.String(location.get().c_str());
        }
        writer.String("my_role");
        writer.StartArray();
        if (roles & OFConstants::POLICY_ELEMENT)
            writer.String("policy_element");
        if (roles & OFConstants::POLICY_REPOSITORY)
            writer.String("policy_repository");
        if (roles & OFConstants::ENDPOINT_REGISTRY)
            writer.String("endpoint_registry");
        if (roles & OFConstants::OBSERVER)
            writer.String("observer");
        writer.EndArray();

        if (roles & OFConstants::POLICY_ELEMENT) {
            writer.String("data");
            writer.StartObject();
            if (!mac.empty()) {
                writer.String("mac");
                writer.String(mac.c_str());
            }
            writer.String("features");
            writer.StartArray();
            writer.String("anycastFallback");
            writer.EndArray();
            writer.EndObject();
        }
        writer.EndObject();
        writer.EndArray();

        return true;
    }

private:
    string name;
    string domain;
    optional<string> location;
    uint8_t roles;
    string mac;
};

void OpflexPEHandler::connected() {
    setState(CONNECTED);

    OpflexPool& pool = getProcessor()->getPool();
    SendIdentityReq* req =
        new SendIdentityReq(pool.getName(),
                            pool.getDomain(),
                            pool.getLocation(),
                            OFConstants::POLICY_ELEMENT,
                            pool.getTunnelMac().toString());
    getConnection()->sendMessage(req, true);
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

static bool validateProxyAddress(const Value &val,
                                 OpflexClientConnection *conn,
                                 const std::string &peer,
                                 address_v4 &parsedAddr) {
    std::string prtStr;
    if(val.IsString()){
        boost::system::error_code ec;
        parsedAddr = address_v4::from_string(val.GetString(), ec);
        if(!ec && !parsedAddr.is_multicast()) {
            return true;
        }
        prtStr = val.GetString();
    }
    if(prtStr.empty()) {
        prtStr = "not a string";
    }
    LOG(ERROR) << "[" << peer << "] "
               << "transport mode proxy address "
               << prtStr
               << " is not a valid unicast IPv4 address";
    conn->disconnect();
    return false;
}

void OpflexPEHandler::handleSendIdentityRes(uint64_t reqId,
                                            const Value& payload) {
    getProcessor()->responseReceived(reqId);
    OpflexPool& pool = getProcessor()->getPool();
    OpflexClientConnection* conn = (OpflexClientConnection*)getConnection();

    bool foundSelf = false;

    OpflexPool::peer_name_set_t peer_set;
    const std::string &remotePeer = getConnection()->getRemotePeer();
    bool isTransportMode = (pool.getClientMode() == AgentMode::TRANSPORT_MODE);
    bool seekingProxies = (pool.getTransportModeState() ==
                           AgentTransportState::SEEKING_PROXIES);
    int proxy_count = 0;

    if (payload.HasMember("your_location")) {
        const Value& ylocation = payload["your_location"];
        if (ylocation.IsString())
            pool.setLocation(ylocation.GetString());
    }
    if (payload.HasMember("data")) {
        const Value& data = payload["data"];
        if (data.IsObject()) {
            if(isTransportMode && seekingProxies) {
                address_v4 addr;
                Value::ConstMemberIterator itr = data.FindMember("proxy_v4");
                if (itr != data.MemberEnd()) {
                    if(!validateProxyAddress(itr->value, conn,
                                             remotePeer, addr)) {
                        return;
                    }
                    pool.setV4Proxy(addr);
                    LOG(INFO) << "[ V4Proxy set to " << addr << " ]";
                    proxy_count++;
                }
                if ((itr = data.FindMember("proxy_v6"))
                    != data.MemberEnd()) {
                    if(!validateProxyAddress(itr->value, conn,
                                             remotePeer, addr)) {
                        return;
                    }
                    pool.setV6Proxy(addr);
                    LOG(INFO) << "[ V6Proxy set to " << addr << " ]";
                    proxy_count++;
                }
                if ((itr = data.FindMember("proxy_mac"))
                    != data.MemberEnd()) {
                    if(!validateProxyAddress(itr->value, conn,
                                             remotePeer, addr)) {
                        return;
                    }
                    pool.setMacProxy(addr);
                    LOG(INFO) << "[ MacProxy set to " << addr << " ]";
                    proxy_count++;
                }
            }
        }
    }
    if(isTransportMode && seekingProxies && (proxy_count != 3)) {
        LOG(ERROR) << "[" << remotePeer << "] "
                   << "Three proxy addresses not returned in "
                   << "Identity response for transport mode agent";
        conn->disconnect();
        return;
    } else if(isTransportMode && seekingProxies) {
        pool.setTransportModeState(AgentTransportState::POLICY_CLIENT);
    }
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
                        pool.addPeer(host, port, false);

                    if (host == conn->getHostname() &&
                        port == conn->getPort())
                        foundSelf = true;

                    peer_set.insert(make_pair(host, port));
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
                        peerRoles |= OFConstants::POLICY_ELEMENT;
                    } else if (rolestr == "policy_repository") {
                        peerRoles |= OFConstants::POLICY_REPOSITORY;
                    } else if (rolestr == "endpoint_registry") {
                        peerRoles |= OFConstants::ENDPOINT_REGISTRY;
                    } else if (rolestr == "observer") {
                        peerRoles |= OFConstants::OBSERVER;
                    }
                }
            }
        }
        pool.setRoles(conn, peerRoles);
        pool.validatePeerSet(conn,peer_set);
        ready();
    } else {
        pool.validatePeerSet(conn,peer_set);
        LOG(INFO) << "[" << getConnection()->getRemotePeer() << "] "
                  << "Current peer not found in peer list; closing";
        conn->close();
    }

}

void OpflexPEHandler::handleSendIdentityErr(uint64_t reqId,
                                            const Value& payload) {
    handleError(reqId, payload, "Send Identity");
    LOG(ERROR) << "Handshake failed; terminating connection";
    setState(FAILED);
}

void OpflexPEHandler::handlePolicyResolveRes(uint64_t reqId,
                                             const Value& payload) {
    getProcessor()->responseReceived(reqId);
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
                } catch (const std::out_of_range& e) {
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

void OpflexPEHandler::handlePolicyUnresolveRes(uint64_t reqId,
                                               const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPDeclareRes(uint64_t reqId,
                                         const rapidjson::Value& payload) {
    getProcessor()->responseReceived(reqId);
}

void OpflexPEHandler::handleEPUndeclareRes(uint64_t reqId,
                                           const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPResolveRes(uint64_t reqId,
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

void OpflexPEHandler::handleEPUnresolveRes(uint64_t reqId,
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
                } catch (const std::out_of_range& e) {
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

void OpflexPEHandler::handleStateReportRes(uint64_t reqId,
                                           const rapidjson::Value& payload) {
    getProcessor()->responseReceived(reqId);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
