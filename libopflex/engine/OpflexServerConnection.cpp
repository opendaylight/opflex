/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexServerConnection
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

#include <openssl/err.h>
#include <sys/un.h>

#include "opflex/engine/internal/OpflexServerConnection.h"
#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;
#ifndef SIMPLE_RPC
using yajr::transport::ZeroCopyOpenSSL;
#endif

OpflexServerConnection::OpflexServerConnection(OpflexListener* listener_)
    : OpflexConnection(listener_->handlerFactory), 
      listener(listener_) {
#ifdef SIMPLE_RPC
    uv_tcp_init(&listener->server_loop, &tcp_handle);
    tcp_handle.data = this;
    int rc = uv_accept((uv_stream_t*)&listener->bind_socket, 
                       (uv_stream_t*)&tcp_handle);
    if (rc < 0) {
        LOG(ERROR) << "Could not accept connection: "
                   << uv_strerror(rc);
    }

    struct sockaddr_storage name;
    int len = sizeof(name);
    rc = uv_tcp_getpeername(&tcp_handle, (struct sockaddr*)&name, &len);
    setRemotePeer(rc, name);

    uv_read_start((uv_stream_t*)&tcp_handle, alloc_cb, read_cb);
    handler->connected();
#endif
}

OpflexServerConnection::~OpflexServerConnection() {

}

void OpflexServerConnection::setRemotePeer(int rc, struct sockaddr_storage& name) {
    if (rc < 0) {
        LOG(ERROR) << "New connection but could not get "
                   << "remote peer IP address" << uv_strerror(rc);
        return;
    } 

    if (name.ss_family == AF_UNIX) {
        remote_peer = ((struct sockaddr_un*)&name)->sun_path;
    } else {
        char addrbuffer[INET6_ADDRSTRLEN];
        inet_ntop(name.ss_family,
                  name.ss_family == AF_INET
                  ? (void *) &(((struct sockaddr_in*)&name)->sin_addr)
                  : (void *) &(((struct sockaddr_in6*)&name)->sin6_addr),
                  addrbuffer, INET6_ADDRSTRLEN);
        remote_peer = addrbuffer;
    }

    LOG(INFO) << "[" << getRemotePeer() << "] "
              << "New server connection";
}

const std::string& OpflexServerConnection::getName() {
    return listener->getName();
}

const std::string& OpflexServerConnection::getDomain() {
    return listener->getDomain();
}

#ifdef SIMPLE_RPC
void OpflexServerConnection::shutdown_cb(uv_shutdown_t* req, int status) {
    OpflexServerConnection* conn = 
        (OpflexServerConnection*)req->handle->data;
    uv_close((uv_handle_t*)&conn->tcp_handle, OpflexListener::on_conn_closed);
}
#else

uv_loop_t* OpflexServerConnection::loop_selector(void * data) {
    OpflexServerConnection* conn = (OpflexServerConnection*)data;
    return conn->getListener()->getLoop();
}

void OpflexServerConnection::on_state_change(yajr::Peer * p, void * data, 
                                             yajr::StateChange::To stateChange,
                                             int error) {
    OpflexServerConnection* conn = (OpflexServerConnection*)data;
    conn->peer = p;
    switch (stateChange) {
    case yajr::StateChange::CONNECT:
        {
            struct sockaddr_storage name;
            int len = sizeof(name);
            int rc = p->getPeerName((struct sockaddr*)&name, &len);
            conn->setRemotePeer(rc, name);

            ZeroCopyOpenSSL::Ctx* serverCtx = conn->listener->serverCtx.get();
            if (serverCtx)
                ZeroCopyOpenSSL::attachTransport(p, serverCtx);

            conn->handler->connected();
        }
        break;
    case yajr::StateChange::DISCONNECT:
        LOG(DEBUG) << "[" << conn->getRemotePeer() << "] "
                   << "Disconnected";
        break;
    case yajr::StateChange::TRANSPORT_FAILURE:
        {
            char buf[120];
            ERR_error_string_n(error, buf, sizeof(buf));
            LOG(ERROR) << "[" << conn->getRemotePeer() << "] "
                       << "SSL Connection error: " << buf;
        }
        break;
    case yajr::StateChange::FAILURE:
        LOG(ERROR) << "[" << conn->getRemotePeer() << "] " 
                   << "Connection error: " << uv_strerror(error);
        break;
    case yajr::StateChange::DELETE:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] " 
                  << "Connection closed";
        conn->getListener()->connectionClosed(conn);
        break;
    }
}

#endif

void OpflexServerConnection::disconnect() {
    handler->disconnected();
    OpflexConnection::disconnect();

#ifdef SIMPLE_RPC
    uv_read_stop((uv_stream_t*)&tcp_handle);
    {
        int rc = uv_shutdown(&shutdown, (uv_stream_t*)&tcp_handle, shutdown_cb);
        if (rc < 0) {
            LOG(ERROR) << "[" << getRemotePeer() << "] " 
                       << "Could not shut down socket: " << uv_strerror(rc);
        }
    }
#else
    if (peer)
        peer->destroy();
#endif
}

#ifdef SIMPLE_RPC
void OpflexServerConnection::write(const rapidjson::StringBuffer* buf) {
    OpflexConnection::write((uv_stream_t*)&tcp_handle, buf);
}
#endif

void OpflexServerConnection::messagesReady() {
    listener->messagesReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
