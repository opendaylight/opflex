/*
 * Implementation of JsonCmdExecutor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include "logging.h"
#include "ovs.h"
#include "JsonCmdExecutor.h"

using namespace std;
using namespace boost;

typedef unique_lock<mutex> mutex_guard;

namespace ovsagent {

JsonCmdExecutor::JsonCmdExecutor() : swConn(NULL) {
}

JsonCmdExecutor::~JsonCmdExecutor() {
}

void
JsonCmdExecutor::installListenersForConnection(SwitchConnection *conn) {
    swConn = conn;
    conn->RegisterOnConnectListener(this);
    conn->registerJsonMessageHandler(this);
}

void
JsonCmdExecutor::uninstallListenersForConnection(SwitchConnection *conn) {
    conn->UnregisterOnConnectListener(this);
    conn->unregisterJsonMessageHandler(this);
}

bool
JsonCmdExecutor::execute(const string& command,
                         const vector<string>& params,
                         string& result) {
    jsonrpc_msg *req = createRequest(command, params);
    json *reqId = json_clone(req->id);
    {
        mutex_guard lock(reqMtx);
        jsonRequests[reqId];
    }

    LOG(DEBUG) << "Executing JSON command: " << command;
    int err = swConn->sendJsonMessage(req);
    if (err != 0) {
        mutex_guard lock(reqMtx);
        jsonRequests.erase(reqId);
        json_destroy(reqId);
        result = ovs_strerror(err);
        return false;
    }

    mutex_guard lock(reqMtx);
    while (jsonRequests[reqId].status == REPLY_NONE) {
        reqCondVar.wait(lock);
    }
    Reply& reply = jsonRequests[reqId];
    bool reqStatus = (reply.status == REPLY_RESULT);
    result.swap(reply.data);

    jsonRequests.erase(reqId);
    json_destroy(reqId);
    return reqStatus;
}

bool
JsonCmdExecutor::executeNoBlock(const string& command,
                                const vector<string>& params) {
    jsonrpc_msg *req = createRequest(command, params);
    return swConn->sendJsonMessage(req) == 0;
}

jsonrpc_msg *
JsonCmdExecutor::createRequest(const string& command,
                               const vector<string>& params) {
    json **jsonArgArr = NULL;
    if (!params.empty()) {
        jsonArgArr = (json **)malloc(params.size() * sizeof(json *));
        for (size_t i = 0; i < params.size(); ++i) {
            jsonArgArr[i] = json_string_create(params[i].c_str());
        }
    }
    jsonrpc_msg *req = jsonrpc_create_request(command.c_str(),
        json_array_create(jsonArgArr, params.size()),
        NULL);
    return req;
}

void JsonCmdExecutor::Handle(SwitchConnection *c, jsonrpc_msg *msg) {
    mutex_guard lock(reqMtx);
    RequestMap::iterator itr = jsonRequests.find(msg->id);
    if (itr != jsonRequests.end()) {
        Reply& reply = itr->second;
        if (msg->type != JSONRPC_REPLY && msg->type != JSONRPC_ERROR) {
            LOG(ERROR) << "Received unexpected JSON message type: "
                << jsonrpc_msg_type_to_string(msg->type);
            reply.data = ovs_strerror(EINVAL);
            reply.status = REPLY_ERROR;
        } else {
            reply.status = (msg->error == NULL) ? REPLY_RESULT : REPLY_ERROR;
            json *res = msg->error ? msg->error : msg->result;
            if (res) {
                if (res->type == JSON_STRING) {
                    reply.data = json_string(res);
                } else {
                    LOG(ERROR) << "Unexpected type of JSON reply ("
                        << res->type << ":" << json_type_to_string(res->type);
                    reply.data = ovs_strerror(EINVAL);
                    reply.status = REPLY_ERROR;
                }
            }
        }
        reqCondVar.notify_all();
    }
}

void JsonCmdExecutor::Connected(SwitchConnection *swConn) {
    /* If connection was re-established, fail outstanding requests */
    mutex_guard lock(reqMtx);
    BOOST_FOREACH (RequestMap::value_type& kv, jsonRequests) {
        kv.second.data = ovs_strerror(ENOTCONN);
        kv.second.status = REPLY_ERROR;
    }
    reqCondVar.notify_all();
}

size_t JsonPtrHash::operator()(json * const obj) const {
    return obj ? json_hash(obj, 0) : 0;
}

bool JsonPtrEquals::operator()(json * const lhs, json * const rhs) const {
    if (lhs == rhs) {
        return true;
    }
    if (lhs == NULL || rhs == NULL) {
        return false;
    }
    return json_equal(lhs, rhs);
}

} // namespace ovsagent
