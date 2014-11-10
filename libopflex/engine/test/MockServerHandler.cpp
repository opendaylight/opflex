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

void MockServerHandler::connected() {
    // XXX - TODO
}

void MockServerHandler::disconnected() {
    // XXX - TODO
}

void MockServerHandler::ready() {
    // XXX - TODO
}

class SendIdentityRes : public OpflexMessage {
public:
    SendIdentityRes(const std::string& name_,
                    const std::string& domain_,
                    const uint8_t roles_,
                    MockOpflexServer::peer_vec_t peers_)
        : OpflexMessage("send_identity", RESPONSE),
          name(name_), domain(domain_), roles(roles_), 
          peers(peers_) {}

    virtual void serializePayload(StrWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(Writer<T> & handler) {
        handler.StartObject();
        handler.String("name");
        handler.String(name.c_str());
        handler.String("domain");
        handler.String(domain.c_str());
        handler.String("my_role");
        handler.StartArray();
        if (roles & OpflexHandler::POLICY_ELEMENT)
            handler.String("policy_element");
        if (roles & OpflexHandler::POLICY_REPOSITORY)
            handler.String("policy_repository");
        if (roles & OpflexHandler::ENDPOINT_REGISTRY)
            handler.String("endpoint_registry");
        if (roles & OpflexHandler::OBSERVER)
            handler.String("observer");
        handler.EndArray();
        handler.String("peers");
        handler.StartArray();
        BOOST_FOREACH(const MockOpflexServer::peer_t& peer, peers) {
            handler.StartObject();
            handler.String("role");
            handler.StartArray();
            if (peer.first & OpflexHandler::POLICY_ELEMENT)
                handler.String("policy_element");
            if (peer.first & OpflexHandler::POLICY_REPOSITORY)
                handler.String("policy_repository");
            if (peer.first & OpflexHandler::ENDPOINT_REGISTRY)
                handler.String("endpoint_registry");
            if (peer.first & OpflexHandler::OBSERVER)
                handler.String("observer");
            handler.EndArray();
            handler.String("connectivity_info");
            handler.String(peer.second.c_str());
            handler.EndObject();
        }
        handler.EndArray();
        handler.EndObject();
        return true;
    }

private:
    std::string name;
    std::string domain;
    uint8_t roles;
    MockOpflexServer::peer_vec_t peers;
};


void MockServerHandler::handleSendIdentityReq(const rapidjson::Value& id,
                                              const Value& payload) {
    LOG(INFO) << "Got send_identity req";
    std::stringstream sb;
    sb << "127.0.0.1:" << server->getPort();
    SendIdentityRes res(sb.str(), "testdomain", 
                        server->getRoles(), 
                        server->getPeers());
    getConnection()->write(res.serialize());
}

void MockServerHandler::handlePolicyResolveReq(const rapidjson::Value& id,
                                               const Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handlePolicyUnresolveReq(const rapidjson::Value& id,
                                                 const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPDeclareReq(const rapidjson::Value& id,
                                           const rapidjson::Value& payload) {
    // XXX - TODO
}

void MockServerHandler::handleEPUndeclareReq(const rapidjson::Value& id,
                                             const rapidjson::Value& payload) {
    // XXX - TODO
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

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
