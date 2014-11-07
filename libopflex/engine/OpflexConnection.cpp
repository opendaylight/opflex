/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using std::string;

OpflexConnection::OpflexConnection(HandlerFactory& handlerFactory)
    : handler(handlerFactory.newHandler(this)), 
      buffer(new std::stringstream()) {
    uv_mutex_init(&write_mutex);
    connect();
}

OpflexConnection::~OpflexConnection() {
    disconnect();
    if (handler)
        delete handler;
    if (buffer)
        delete buffer;
    uv_mutex_destroy(&write_mutex);
}

void OpflexConnection::connect() {}

void OpflexConnection::disconnect() {}

bool OpflexConnection::isReady() { 
    return handler->isReady();
}

void OpflexConnection::alloc_cb(uv_handle_t* handle,
                                size_t suggested_size,
                                uv_buf_t* buf) {
    *buf = uv_buf_init((char*)malloc(suggested_size), suggested_size);
}

void OpflexConnection::read_cb(uv_stream_t* stream,
                               ssize_t nread,
                               const uv_buf_t* buf) {
    OpflexConnection* conn = (OpflexConnection*)stream->data;
    if (nread < 0) {
        if (nread != UV_EOF) {
            LOG(ERROR) << "Error reading from socket: "
                       << uv_strerror(nread);
        }
        conn->disconnect();
    } else if (nread > 0) {
        for (int i = 0; i < nread; ++i) {
            *conn->buffer << buf->base[i];
            if (buf->base[i] == '\0')
                conn->dispatch();
        }
    }
    if (buf->base)
        free(buf->base);
}

void OpflexConnection::dispatch() {
    document.Parse(buffer->str().c_str());
    delete buffer;
    buffer = new std::stringstream();
    if (document.HasParseError()) {
        LOG(ERROR) << "Malformed message received from " << getRemotePeer();
        return;
    }

    if (!document.HasMember("id")) {
        LOG(ERROR) << "Message missing ID from " << getRemotePeer();
        return;
    }
    const Value& idv = document["id"];

    if (document.HasMember("method")) {
        const Value& methodv = document["method"];
        if (!methodv.IsString()) {
            LOG(ERROR) << "Malformed request method";
            return;
        }
        const string method = methodv.GetString();
        if (!document.HasMember("params")) {
            LOG(ERROR) << "Request '" << method << "' missing params";
            return;
        }

        if (method == "send_identity") {
            handler->handleSendIdentityReq(idv, document["params"]);
            //} else if (method == "echo") {
            //    handler->handleEchoReq(idv, document["params"]);
        } else if (method == "policy_resolve") {
            handler->handlePolicyResolveReq(idv, document["params"]);
        } else if (method == "policy_unresolve") {
            handler->handlePolicyUnresolveReq(idv, document["params"]);
        } else if (method == "policy_update") {
            handler->handlePolicyUpdateReq(idv, document["params"]);
        } else if (method == "endpoint_declare") {
            handler->handleEPDeclareReq(idv, document["params"]);
        } else if (method == "endpoint_undeclare") {
            handler->handleEPUndeclareReq(idv, document["params"]);
        } else if (method == "endpoint_resolve") {
            handler->handleEPResolveReq(idv, document["params"]);
        } else if (method == "endpoint_unresolve") {
            handler->handleEPUnresolveReq(idv, document["params"]);
        } else if (method == "endpoint_update") {
            handler->handleEPUpdateReq(idv, document["params"]);
        } else if (method == "state_report") {
            handler->handleStateReportReq(idv, document["params"]);
        }
    } else if (document.HasMember("result")) {
        if (!idv.IsString()) {
            LOG(ERROR) << "Malformed ID in response message";
            return;
        }

        const string id = idv.GetString();

        if (id == "send_identity") {
            handler->handleSendIdentityRes(idv, document["result"]);
            //} else if (id == "echo") {
            //    handler->handleEchoRes(idv, document["result"]);
        } else if (id == "policy_resolve") {
            handler->handlePolicyResolveRes(idv, document["result"]);
        } else if (id == "policy_unresolve") {
            handler->handlePolicyUnresolveRes(idv, document["result"]);
        } else if (id == "policy_update") {
            handler->handlePolicyUpdateRes(idv, document["result"]);
        } else if (id == "endpoint_declare") {
            handler->handleEPDeclareRes(idv, document["result"]);
        } else if (id == "endpoint_undeclare") {
            handler->handleEPUndeclareRes(idv, document["result"]);
        } else if (id == "endpoint_resolve") {
            handler->handleEPResolveRes(idv, document["result"]);
        } else if (id == "endpoint_unresolve") {
            handler->handleEPUnresolveRes(idv, document["result"]);
        } else if (id == "endpoint_update") {
            handler->handleEPUpdateRes(idv, document["result"]);
        } else if (id == "state_report") {
            handler->handleStateReportRes(idv, document["result"]);
        }
        
    } else if (document.HasMember("error")) {
        if (!idv.IsString()) {
            LOG(ERROR) << "Malformed ID in response message";
            return;
        }

        const string id = idv.GetString();

        if (id == "send_identity") {
            handler->handleSendIdentityErr(idv, document["error"]);
            //} else if (id == "echo") {
            //    handler->handleEchoErr(idv, document["error"]);
        } else if (id == "policy_resolve") {
            handler->handlePolicyResolveErr(idv, document["error"]);
        } else if (id == "policy_unresolve") {
            handler->handlePolicyUnresolveErr(idv, document["error"]);
        } else if (id == "policy_update") {
            handler->handlePolicyUpdateErr(idv, document["error"]);
        } else if (id == "endpoint_declare") {
            handler->handleEPDeclareErr(idv, document["error"]);
        } else if (id == "endpoint_undeclare") {
            handler->handleEPUndeclareErr(idv, document["error"]);
        } else if (id == "endpoint_resolve") {
            handler->handleEPResolveErr(idv, document["error"]);
        } else if (id == "endpoint_unresolve") {
            handler->handleEPUnresolveErr(idv, document["error"]);
        } else if (id == "endpoint_update") {
            handler->handleEPUpdateErr(idv, document["error"]);
        } else if (id == "state_report") {
            handler->handleStateReportErr(idv, document["error"]);
        }
    }
}

void OpflexConnection::write(uv_stream_t* stream,
                             const rapidjson::StringBuffer* buf) {
    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
    {
        util::LockGuard guard(&write_mutex);
        write_list.push_back(std::make_pair(req, buf));
        uv_buf_t ubuf;
        ubuf.base = (char*)buf->GetString();
        ubuf.len = buf->GetSize();
        int rc = uv_write(req, stream, &ubuf, 1, write_cb);
        if (rc < 0) {
            LOG(ERROR) << "Could not write to socket: "
                       << uv_strerror(rc);
            disconnect();
        }
    }
}

void OpflexConnection::write_cb(uv_write_t* req,
                                int status) {
    OpflexConnection* conn = (OpflexConnection*)req->handle->data;
    {
        util::LockGuard guard(&conn->write_mutex);
        write_t& wt = conn->write_list.front();
        free(wt.first);
        delete wt.second;
        conn->write_list.pop_front();
    }

    if (status < 0) {
        LOG(ERROR) << "Error while writing to socket: "
                   << uv_strerror(status);
        conn->disconnect();
    }
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
