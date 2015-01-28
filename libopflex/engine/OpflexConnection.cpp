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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <boost/scoped_ptr.hpp>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

#ifndef SIMPLE_RPC
#include "yajr/transport/ZeroCopyOpenSSL.hpp"
#include "yajr/rpc/message_factory.hpp"    

static uv_once_t ssl_once = UV_ONCE_INIT;
#endif

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using std::string;
using boost::scoped_ptr;
#ifndef SIMPLE_RPC
using yajr::rpc::OutboundRequest;
using yajr::rpc::OutboundResult;
using yajr::rpc::OutboundError;
using yajr::transport::ZeroCopyOpenSSL;
#endif

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
    cleanup();
    if (handler)
        delete handler;
#ifdef SIMPLE_RPC
    if (buffer)
        delete buffer;
#endif
    uv_mutex_destroy(&queue_mutex);
}

static void init_ssl() {
#ifndef SIMPLE_RPC
    ZeroCopyOpenSSL::initOpenSSL(true);
#endif
}

void OpflexConnection::initSSL() {
#ifndef SIMPLE_RPC
    uv_once(&ssl_once, init_ssl);
#endif
}

void OpflexConnection::connect() {}

void OpflexConnection::cleanup() {
    util::LockGuard guard(&queue_mutex);
    while (write_queue.size() > 0) {
        delete write_queue.front();
        write_queue.pop_front();
    }
}

void OpflexConnection::disconnect() {
    cleanup();
}

void OpflexConnection::close() {
    disconnect();
}

bool OpflexConnection::isReady() { 
    return handler->isReady();
}

void OpflexConnection::notifyReady() { 

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
            handler->handleSendIdentityRes(result);
            //} else if (id == "echo") {
            //    handler->handleEchoRes(result);
        } else if (id == "policy_resolve") {
            handler->handlePolicyResolveRes(result);
        } else if (id == "policy_unresolve") {
            handler->handlePolicyUnresolveRes(result);
        } else if (id == "policy_update") {
            handler->handlePolicyUpdateRes(result);
        } else if (id == "endpoint_declare") {
            handler->handleEPDeclareRes(result);
        } else if (id == "endpoint_undeclare") {
            handler->handleEPUndeclareRes(result);
        } else if (id == "endpoint_resolve") {
            handler->handleEPResolveRes(result);
        } else if (id == "endpoint_unresolve") {
            handler->handleEPUnresolveRes(result);
        } else if (id == "endpoint_update") {
            handler->handleEPUpdateRes(result);
        } else if (id == "state_report") {
            handler->handleStateReportRes(result);
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
            handler->handleSendIdentityErr(error);
            //} else if (id == "echo") {
            //    handler->handleEchoErr(error);
        } else if (id == "policy_resolve") {
            handler->handlePolicyResolveErr(error);
        } else if (id == "policy_unresolve") {
            handler->handlePolicyUnresolveErr(error);
        } else if (id == "policy_update") {
            handler->handlePolicyUpdateErr(error);
        } else if (id == "endpoint_declare") {
            handler->handleEPDeclareErr(error);
        } else if (id == "endpoint_undeclare") {
            handler->handleEPUndeclareErr(error);
        } else if (id == "endpoint_resolve") {
            handler->handleEPResolveErr(error);
        } else if (id == "endpoint_unresolve") {
            handler->handleEPUnresolveErr(error);
        } else if (id == "endpoint_update") {
            handler->handleEPUpdateErr(error);
        } else if (id == "state_report") {
            handler->handleStateReportErr(error);
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

#else /* SIMPLE_RPC */

class PayloadWrapper {
public:
    PayloadWrapper(OpflexMessage* message_)
        : message(message_) { }

    bool operator()(yajr::rpc::SendHandler& handler) {
        message->serializePayload(handler);
        return true;
    }

    OpflexMessage* message;
};
#endif

void OpflexConnection::doWrite(OpflexMessage* message) {
#ifdef SIMPLE_RPC
    write(message->serialize());
#else
    if (getPeer() == NULL) return;

    PayloadWrapper wrapper(message);
    switch (message->getType()) {
    case OpflexMessage::REQUEST:
        {
            yajr::rpc::MethodName method(message->getMethod().c_str());
            OutboundRequest outm(wrapper,
                                 &method,
                                 0,
                                 getPeer());
            outm.send();
        }
        break;
    case OpflexMessage::RESPONSE:
        {
            OutboundResult outm(*getPeer(),
                                wrapper,
                                message->getId());
            outm.send();
        }
        break;
    case OpflexMessage::ERROR_RESPONSE:
        {
            OutboundError outm(*getPeer(),
                               wrapper,
                               message->getId());
            outm.send();
        }
        break;
    }
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
