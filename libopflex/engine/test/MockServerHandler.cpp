/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MockServerHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>

#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/logging/internal/logging.hpp"

#include "MockServerHandler.h"
#include "MockOpflexServer.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using rapidjson::Writer;
using modb::mointernal::StoreClient;

void MockServerHandler::connected() {

}

void MockServerHandler::disconnected() {

}

void MockServerHandler::ready() {

}

class SendIdentityRes : public OpflexMessage {
public:
    SendIdentityRes(const rapidjson::Value& id,
                    const std::string& name_,
                    const std::string& domain_,
                    const uint8_t roles_,
                    MockOpflexServer::peer_vec_t peers_)
        : OpflexMessage("send_identity", RESPONSE, &id),
          name(name_), domain(domain_), roles(roles_), 
          peers(peers_) {}

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(Writer<T> & writer) {
        writer.StartObject();
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
        writer.String("peers");
        writer.StartArray();
        BOOST_FOREACH(const MockOpflexServer::peer_t& peer, peers) {
            writer.StartObject();
            writer.String("role");
            writer.StartArray();
            if (peer.first & OpflexHandler::POLICY_ELEMENT)
                writer.String("policy_element");
            if (peer.first & OpflexHandler::POLICY_REPOSITORY)
                writer.String("policy_repository");
            if (peer.first & OpflexHandler::ENDPOINT_REGISTRY)
                writer.String("endpoint_registry");
            if (peer.first & OpflexHandler::OBSERVER)
                writer.String("observer");
            writer.EndArray();
            writer.String("connectivity_info");
            writer.String(peer.second.c_str());
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
        return true;
    }

private:
    std::string name;
    std::string domain;
    uint8_t roles;
    MockOpflexServer::peer_vec_t peers;
};

class PolicyResolveRes : public OpflexMessage {
public:
    PolicyResolveRes(const rapidjson::Value& id,
                     MockOpflexServer& server_,
                     const std::vector<modb::reference_t>& mos_)
        : OpflexMessage("policy_update", RESPONSE, &id),
          server(server_),
          mos(mos_) {}

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(Writer<T> & writer) {
        MOSerializer& serializer = server.getSerializer();
        modb::mointernal::StoreClient* client = server.getSystemClient();

        writer.StartObject();
        writer.String("policy");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, mos) {
            try {
                serializer.serialize(p.first, p.second, 
                                     *client, writer,
                                     true);
            } catch (std::out_of_range e) {
                // policy doesn't exist locally
            }
        }
        writer.EndArray();
        writer.EndObject();
        return true;
    }

protected:
    MockOpflexServer& server;
    std::vector<modb::reference_t> mos;
};

void MockServerHandler::handleSendIdentityReq(const rapidjson::Value& id,
                                              const Value& payload) {
    LOG(DEBUG) << "Got send_identity req";
    std::stringstream sb;
    sb << "127.0.0.1:" << server->getPort();
    SendIdentityRes res(id, sb.str(), "testdomain", 
                        server->getRoles(), 
                        server->getPeers());
    getConnection()->write(res.serialize());
}

void MockServerHandler::handlePolicyResolveReq(const rapidjson::Value& id,
                                               const Value& payload) {
    LOG(DEBUG) << "Got policy_resolve req";
    StoreClient& client = *server->getSystemClient();
    MOSerializer& serializer = server->getSerializer();

    Value::ConstValueIterator it;
    std::vector<modb::reference_t> mos;
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", "Malformed message: not an object");
            return;
        }
        if (!it->HasMember("subject")) {
            sendErrorRes(id, "ERROR", "Malformed message: no endpoint");
            return;
        }
        if (it->HasMember("policy_ident")) {
            sendErrorRes(id, "EUNSUPPORTED", 
                         "Policy resolution by ident is not supported");
            return;
        }
        if (!it->HasMember("policy_uri")) {
            sendErrorRes(id, "ERROR", "Malformed message: no policy_uri");
            return;
        }

        const Value& subjectv = (*it)["subject"];
        const Value& puriv = (*it)["policy_uri"];
        if (!subjectv.IsString()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: subject is not a string");
            return;
        }
        if (!puriv.IsString()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: policy_uri is not a string");
            return;
        }

        try {
            const modb::ClassInfo& ci = 
                server->getStore().getClassInfo(subjectv.GetString());
            modb::URI puri(puriv.GetString());
            modb::reference_t mo(ci.getId(), puri);
            resolutions.insert(mo);
            mos.push_back(mo);
        } catch (std::out_of_range e) {
            sendErrorRes(id, "ERROR", 
                         std::string("Unknown subject: ") + 
                         subjectv.GetString());
            return;
        }
    }

    PolicyResolveRes res(id, *server, mos);
    getConnection()->write(res.serialize());
}

void MockServerHandler::handlePolicyUnresolveReq(const rapidjson::Value& id,
                                                 const rapidjson::Value& payload) {
    LOG(DEBUG) << "Got policy_unresolve req";
    Value::ConstValueIterator it;
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", "Malformed message: not an object");
            return;
        }
        if (!it->HasMember("subject")) {
            sendErrorRes(id, "ERROR", "Malformed message: no endpoint");
            return;
        }
        if (it->HasMember("policy_ident")) {
            sendErrorRes(id, "EUNSUPPORTED", 
                         "Policy resolution by ident is not supported");
            return;
        }
        if (!it->HasMember("policy_uri")) {
            sendErrorRes(id, "ERROR", "Malformed message: no policy_uri");
            return;
        }

        const Value& subjectv = (*it)["subject"];
        const Value& puriv = (*it)["policy_uri"];
        if (!subjectv.IsString()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: subject is not a string");
            return;
        }
        if (!puriv.IsString()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: policy_uri is not a string");
            return;
        }

        try {
            const modb::ClassInfo& ci = 
                server->getStore().getClassInfo(subjectv.GetString());
            modb::URI puri(puriv.GetString());
            resolutions.erase(std::make_pair(ci.getId(), puri));
        } catch (std::out_of_range e) {
            sendErrorRes(id, "ERROR", 
                         std::string("Unknown subject: ") + 
                         subjectv.GetString());
            return;
        }
    }

    OpflexMessage res("policy_unresolve", OpflexMessage::RESPONSE, &id);
    getConnection()->write(res.serialize());
}

void MockServerHandler::handleEPDeclareReq(const rapidjson::Value& id,
                                           const rapidjson::Value& payload) {
    LOG(DEBUG) << "Got endpoint_declare req";
    StoreClient::notif_t notifs;
    StoreClient& client = *server->getSystemClient();
    MOSerializer& serializer = server->getSerializer();

    Value::ConstValueIterator it;
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", "Malformed message: not an object");
            return;
        }
        if (!it->HasMember("endpoint")) {
            sendErrorRes(id, "ERROR", "Malformed message: no endpoint");
            return;
        }
        if (!it->HasMember("prr")) {
            sendErrorRes(id, "ERROR", "Malformed message: no prr");
            return;
        }

        const Value& endpoint = (*it)["endpoint"];
        const Value& prr = (*it)["prr"];

        if (!endpoint.IsArray()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: endpoint is not an array");
            return;
        }
        if (!prr.IsInt()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: prr is not an integer");
            return;
        }

        Value::ConstValueIterator ep_it;
        for (ep_it = endpoint.Begin(); ep_it != endpoint.End(); ++ep_it) {
            const Value& mo = *ep_it;
            serializer.deserialize(mo, client, true, &notifs);
        }
    }
    client.deliverNotifications(notifs);

    OpflexMessage res("endpoint_declare", OpflexMessage::RESPONSE, &id);
    getConnection()->write(res.serialize());
}

void MockServerHandler::handleEPUndeclareReq(const rapidjson::Value& id,
                                             const rapidjson::Value& payload) {
    StoreClient::notif_t notifs;
    StoreClient& client = *server->getSystemClient();

    Value::ConstValueIterator it;
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", "Malformed message: not an object");
            return;
        }
        if (!it->HasMember("subject")) {
            sendErrorRes(id, "ERROR", "Malformed message: no subject");
            return;
        }
        if (!it->HasMember("endpoint_uri")) {
            sendErrorRes(id, "ERROR", "Malformed message: no endpoint_uri");
            return;
        }

        const Value& subjectv = (*it)["subject"];
        const Value& euriv = (*it)["endpoint_uri"];
        if (!subjectv.IsString()) {
            sendErrorRes(id, "ERROR", 
                         "Malformed message: subject is not a string");
            return;
        }
        if (!euriv.IsString()) {
            sendErrorRes(id, "ERROR",
                         "Malformed message: endpoint_uri is not a string");
            return;
        }
        try {
            const modb::ClassInfo& ci = 
                server->getStore().getClassInfo(subjectv.GetString());
            modb::URI euri(euriv.GetString());
            client.remove(ci.getId(), euri, false, &notifs);
            client.queueNotification(ci.getId(), euri, notifs);
        } catch (std::out_of_range e) {
            sendErrorRes(id, "ERROR", 
                         std::string("Unknown subject: ") + 
                         subjectv.GetString());
            return;
        }

    }
    client.deliverNotifications(notifs);

    OpflexMessage res("endpoint_undeclare", OpflexMessage::RESPONSE, &id);
    getConnection()->write(res.serialize());
}

void MockServerHandler::handleEPResolveReq(const rapidjson::Value& id,
                                           const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPUnresolveReq(const rapidjson::Value& id,
                                             const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPUpdateRes(const rapidjson::Value& id,
                                          const rapidjson::Value& payload) {
    // nothing to do
}

bool MockServerHandler::hasResolution(modb::class_id_t class_id, 
                                      const modb::URI& uri) {
    return resolutions.find(std::make_pair(class_id, uri)) != resolutions.end();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
