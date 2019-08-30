/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for InspectorClientHandler
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

#include <sstream>

#include "opflex/logging/internal/logging.hpp"
#include "opflex/engine/internal/InspectorClientHandler.h"
#include "opflex/engine/internal/InspectorServerHandler.h"
#include "opflex/engine/InspectorClientImpl.h"
#include "opflex/engine/Inspector.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;
using rapidjson::Value;
using rapidjson::Writer;
using ofcore::OFConstants;
using modb::mointernal::StoreClient;

InspectorClientHandler::InspectorClientHandler(OpflexConnection* conn,
                                               InspectorClientImpl* client_)
    : OpflexHandler(conn), client(client_) {}

void InspectorClientHandler::connected() {
    client->executeCommands();
    checkDone();
}

void InspectorClientHandler::disconnected() {

}

void InspectorClientHandler::ready() {

}

void InspectorClientHandler::checkDone() {
    if (client->pendingRequests == 0)
        client->conn.close();
}

void InspectorClientHandler::handlePolicyQueryRes(const Value& payload) {
    StoreClient* storeClient = client->storeClient;
    MOSerializer& serializer = client->serializer;
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
            serializer.deserialize(mo, *storeClient, true, &notifs);
        }
    }

    client->pendingRequests -= 1;
    checkDone();
}

void InspectorClientHandler::handleCustomRes(uint64_t reqId,
                                             const Value& payload) {
    if (!payload.HasMember("method") || !payload.HasMember("result"))
        return;
    
    const Value& method = payload["method"];
    const Value& result = payload["result"];

    if (!method.IsString() || !result.IsObject()) return;

    if (InspectorServerHandler::POLICY_QUERY == method.GetString())
        handlePolicyQueryRes(result);
}

void InspectorClientHandler::handleError(uint64_t reqId,
                                         const Value& payload,
                                         const string& type) {
    OpflexHandler::handleError(reqId, payload, type);
    client->pendingRequests -= 1;
    checkDone();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
