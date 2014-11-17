/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexClientConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <stdexcept>

#include "opflex/engine/internal/OpflexClientConnection.h"
#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace engine {
namespace internal {

using std::string;

OpflexClientConnection::OpflexClientConnection(HandlerFactory& handlerFactory, 
                                               OpflexPool* pool_,
                                               const string& hostname_, 
                                               int port_)
    : OpflexConnection(handlerFactory),
      pool(pool_), hostname(hostname_), port(port_),
      active(false) {

}

OpflexClientConnection::~OpflexClientConnection() {

}

const std::string& OpflexClientConnection::getName() {
    return pool->getName();
}

const std::string& OpflexClientConnection::getDomain() {
    return pool->getDomain();
}

void OpflexClientConnection::shutdown_cb(uv_shutdown_t* req, int status) {
    OpflexClientConnection* conn = 
        (OpflexClientConnection*)req->handle->data;
    uv_close((uv_handle_t*)&conn->socket, on_conn_closed);
}

void OpflexClientConnection::connect() {
    if (active) return;
    active = true;

    std::stringstream rp;
    rp << hostname << ":" << port;
    remote_peer = rp.str();
    uv_tcp_init(&pool->client_loop, &socket);
    socket.data = this;
    // xxx for now assume that hostname is an ipv4 address
    struct sockaddr_in dest;
    int rc = uv_ip4_addr(hostname.c_str(), port, &dest);
    if (rc < 0) {
        throw std::runtime_error(string("Could not convert to IP address ") +
                                 hostname);
    }
    uv_tcp_connect(&connect_req, &socket, (struct sockaddr*)&dest, connect_cb);
}

void OpflexClientConnection::disconnect() {
    if (!active) return;
    active = false;

    LOG(INFO) << "[" << getRemotePeer() << "] " 
              << "Disconnected";
    uv_read_stop((uv_stream_t*)&socket);
    handler->disconnected();
    if (0) {
        uv_shutdown(&shutdown, (uv_stream_t*)&socket, shutdown_cb);
    } else {
        uv_close((uv_handle_t*)&socket, OpflexPool::on_conn_closed);
    }
}

void OpflexClientConnection::connect_cb(uv_connect_t* req, int status) {
    OpflexClientConnection* conn = 
        (OpflexClientConnection*)req->handle->data;
    
    if (status < 0) {
        LOG(ERROR) << "[" << conn->getRemotePeer() << "] " 
                   << "Could not connect: " << uv_strerror(status);
        conn->disconnect();
    } else {
        LOG(INFO) << "[" << conn->getRemotePeer() << "] " 
                  << "New client connection";
        uv_read_start((uv_stream_t*)&conn->socket, alloc_cb, read_cb);
        conn->handler->connected();
    }
}

void OpflexClientConnection::write(const rapidjson::StringBuffer* buf) {
    OpflexConnection::write((uv_stream_t*)&socket, buf);
}

void OpflexClientConnection::on_conn_closed(uv_handle_t *handle) {
    OpflexPool::on_conn_closed(handle);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
