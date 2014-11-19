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

#include <boost/scoped_ptr.hpp>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using std::string;
using boost::scoped_ptr;

OpflexConnection::OpflexConnection(HandlerFactory& handlerFactory)
    : handler(handlerFactory.newHandler(this)) 
#ifdef SIMPLE_RPC
    ,buffer(new std::stringstream()) 
#endif
{
    uv_mutex_init(&queue_mutex);
    connect();
}

OpflexConnection::~OpflexConnection() {
    disconnect();
    if (handler)
        delete handler;
#ifdef SIMPLE_RPC
    if (buffer)
        delete buffer;
#endif
    uv_mutex_destroy(&queue_mutex);
}

void OpflexConnection::connect() {}

void OpflexConnection::disconnect() {
    util::LockGuard guard(&queue_mutex);
    while (write_queue.size() > 0) {
        delete write_queue.front();
        write_queue.pop_front();
    }
}

bool OpflexConnection::isReady() { 
    return handler->isReady();
}

#ifdef SIMPLE_RPC
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
            LOG(ERROR) << "[" << conn->getRemotePeer() << "] " 
                       << "Error reading from socket: "
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
        LOG(ERROR) << "[" << getRemotePeer() << "] " 
                   << "Malformed message received";
        return;
    }

    if (!document.HasMember("id")) {
        LOG(ERROR) << "[" << getRemotePeer() << "] " 
                   << "Message missing ID";
        return;
    }
    const Value& idv = document["id"];

    if (document.HasMember("method")) {
        const Value& methodv = document["method"];
        if (!methodv.IsString()) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Malformed request method";
            return;
        }
        const string method = methodv.GetString();
        if (!document.HasMember("params")) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Request '" << method << "' missing params";
            return;
        }
        const Value& params = document["params"];
        if (!params.IsArray()) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Request '" << method 
                       << "' params is not an array";
            return;
        }

        if (method == "send_identity") {
            handler->handleSendIdentityReq(idv, params);
            //} else if (method == "echo") {
            //    handler->handleEchoReq(idv, params);
        } else if (method == "policy_resolve") {
            handler->handlePolicyResolveReq(idv, params);
        } else if (method == "policy_unresolve") {
            handler->handlePolicyUnresolveReq(idv, params);
        } else if (method == "policy_update") {
            handler->handlePolicyUpdateReq(idv, params);
        } else if (method == "endpoint_declare") {
            handler->handleEPDeclareReq(idv, params);
        } else if (method == "endpoint_undeclare") {
            handler->handleEPUndeclareReq(idv, params);
        } else if (method == "endpoint_resolve") {
            handler->handleEPResolveReq(idv, params);
        } else if (method == "endpoint_unresolve") {
            handler->handleEPUnresolveReq(idv, params);
        } else if (method == "endpoint_update") {
            handler->handleEPUpdateReq(idv, params);
        } else if (method == "state_report") {
            handler->handleStateReportReq(idv, params);
        }
    } else if (document.HasMember("result")) {
        if (!idv.IsString()) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Malformed ID in response message";
            return;
        }

        const string id = idv.GetString();

        const Value& result = document["result"];
        if (!result.IsObject()) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Response '" << id 
                       << "': result is not an object";
            return;
        }

        if (id == "send_identity") {
            handler->handleSendIdentityRes(idv, result);
            //} else if (id == "echo") {
            //    handler->handleEchoRes(idv, result);
        } else if (id == "policy_resolve") {
            handler->handlePolicyResolveRes(idv, result);
        } else if (id == "policy_unresolve") {
            handler->handlePolicyUnresolveRes(idv, result);
        } else if (id == "policy_update") {
            handler->handlePolicyUpdateRes(idv, result);
        } else if (id == "endpoint_declare") {
            handler->handleEPDeclareRes(idv, result);
        } else if (id == "endpoint_undeclare") {
            handler->handleEPUndeclareRes(idv, result);
        } else if (id == "endpoint_resolve") {
            handler->handleEPResolveRes(idv, result);
        } else if (id == "endpoint_unresolve") {
            handler->handleEPUnresolveRes(idv, result);
        } else if (id == "endpoint_update") {
            handler->handleEPUpdateRes(idv, result);
        } else if (id == "state_report") {
            handler->handleStateReportRes(idv, result);
        }
        
    } else if (document.HasMember("error")) {
        if (!idv.IsString()) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Malformed ID in response message";
            return;
        }

        const string id = idv.GetString();

        const Value& error = document["error"];
        if (!error.IsObject()) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Response '" << id 
                       << "': error is not an object";
            return;
        }

        if (id == "send_identity") {
            handler->handleSendIdentityErr(idv, error);
            //} else if (id == "echo") {
            //    handler->handleEchoErr(idv, error);
        } else if (id == "policy_resolve") {
            handler->handlePolicyResolveErr(idv, error);
        } else if (id == "policy_unresolve") {
            handler->handlePolicyUnresolveErr(idv, error);
        } else if (id == "policy_update") {
            handler->handlePolicyUpdateErr(idv, error);
        } else if (id == "endpoint_declare") {
            handler->handleEPDeclareErr(idv, error);
        } else if (id == "endpoint_undeclare") {
            handler->handleEPUndeclareErr(idv, error);
        } else if (id == "endpoint_resolve") {
            handler->handleEPResolveErr(idv, error);
        } else if (id == "endpoint_unresolve") {
            handler->handleEPUnresolveErr(idv, error);
        } else if (id == "endpoint_update") {
            handler->handleEPUpdateErr(idv, error);
        } else if (id == "state_report") {
            handler->handleStateReportErr(idv, error);
        }
    }
}

void OpflexConnection::write(uv_stream_t* stream,
                             const rapidjson::StringBuffer* buf) {
    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
    write_list.push_back(std::make_pair(req, buf));
    uv_buf_t ubuf;
    ubuf.base = (char*)buf->GetString();
    ubuf.len = buf->GetSize();
    int rc = uv_write(req, stream, &ubuf, 1, write_cb);
    if (rc < 0) {
        LOG(ERROR) << "[" << getRemotePeer() << "] " 
                   << "Could not write to socket: "
                   << uv_strerror(rc);
        disconnect();
    }
}

void OpflexConnection::write_cb(uv_write_t* req,
                                int status) {
    OpflexConnection* conn = (OpflexConnection*)req->handle->data;
    write_t& wt = conn->write_list.front();
    free(wt.first);
    delete wt.second;
    conn->write_list.pop_front();

    if (status < 0) {
        LOG(ERROR) << "[" << conn->getRemotePeer() << "] " 
                   << "Error while writing to socket: "
                   << uv_strerror(status);
        conn->disconnect();
    }
}
#endif /* SIMPLE_RPC */

void OpflexConnection::doWrite(OpflexMessage* message) {
    
#ifdef SIMPLE_RPC
        write(message->serialize());
#else
        
#endif

}

void OpflexConnection::processWriteQueue() {
    util::LockGuard guard(&queue_mutex);
    while (write_queue.size() > 0) {
        scoped_ptr<OpflexMessage> message(write_queue.front());
        write_queue.pop_front();
        doWrite(message.get());
    }

}

void OpflexConnection::sendMessage(OpflexMessage* message, bool sync) {
    if (sync) {
        scoped_ptr<OpflexMessage> messagep(message);
        doWrite(message);
    } else {
        util::LockGuard guard(&queue_mutex);
        write_queue.push_back(message);
    }
    messagesReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
