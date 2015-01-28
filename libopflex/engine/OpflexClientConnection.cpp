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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <openssl/err.h>

#include "opflex/engine/internal/OpflexClientConnection.h"
#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace engine {
namespace internal {

using std::string;
#ifndef SIMPLE_RPC
using yajr::Peer;
using yajr::transport::ZeroCopyOpenSSL;
#endif
using ofcore::PeerStatusListener;

OpflexClientConnection::OpflexClientConnection(HandlerFactory& handlerFactory, 
                                               OpflexPool* pool_,
                                               const string& hostname_, 
                                               int port_)
    : OpflexConnection(handlerFactory),
      pool(pool_), hostname(hostname_), port(port_),
#ifdef SIMPLE_RPC
      retry(true),
#endif
      started(false), active(false), closing(false) {

}

OpflexClientConnection::~OpflexClientConnection() {

}

const std::string& OpflexClientConnection::getName() {
    return pool->getName();
}

const std::string& OpflexClientConnection::getDomain() {
    return pool->getDomain();
}

void OpflexClientConnection::notifyReady() {
    pool->updatePeerStatus(hostname, port, PeerStatusListener::READY);
}

void OpflexClientConnection::connect() {
    if (started) return;
    started = true;
    active = true;

    pool->updatePeerStatus(hostname, port, PeerStatusListener::CONNECTING);

    std::stringstream rp;
    rp << hostname << ":" << port;
    remote_peer = rp.str();

#ifdef SIMPLE_RPC
    uv_tcp_init(&pool->client_loop, &socket);
    socket.data = this;
    // assume that hostname is an ipv4 address for simple rpc
    struct sockaddr_in dest;
    int rc = uv_ip4_addr(hostname.c_str(), port, &dest);
    if (rc < 0) {
        throw std::runtime_error(string("Could not convert to IP address ") +
                                 hostname);
    }
    uv_tcp_connect(&connect_req, &socket, (struct sockaddr*)&dest, connect_cb);
#else
    peer = yajr::Peer::create(hostname, 
                              boost::lexical_cast<string>(port),
                              on_state_change, 
                              this, loop_selector);
#endif
}

void OpflexClientConnection::disconnect() {
    if (!active) return;
    active = false;

    LOG(DEBUG) << "[" << getRemotePeer() << "] " 
               << "Disconnected";
    handler->disconnected();
    OpflexConnection::disconnect();

    closing = true;

#ifdef SIMPLE_RPC
    uv_read_stop((uv_stream_t*)&socket);
    uv_close((uv_handle_t*)&socket, on_conn_closed);
#else
    peer->disconnect();
#endif
}

void OpflexClientConnection::close() {
    pool->updatePeerStatus(hostname, port, PeerStatusListener::CLOSING);
#ifdef SIMPLE_RPC
    retry = false;
#endif
    if (active) {
        disconnect();
    }
#ifdef SIMPLE_RPC
    else if (!closing) {
        pool->connectionClosed(this);
    }
#else
    peer->destroy();
#endif
}

#ifdef SIMPLE_RPC
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

void OpflexClientConnection::on_conn_closed(uv_handle_t *handle) {
    OpflexClientConnection* conn = (OpflexClientConnection*)handle->data;
    OpflexPool::on_conn_closed(conn);
}

#else

uv_loop_t* OpflexClientConnection::loop_selector(void * data) {
    OpflexClientConnection* conn = (OpflexClientConnection*)data;
    return conn->getPool()->getLoop();
}

void OpflexClientConnection::on_state_change(Peer * p, void * data, 
                                             yajr::StateChange::To stateChange,
                                             int error) {
    OpflexClientConnection* conn = (OpflexClientConnection*)data;
    switch (stateChange) {
    case yajr::StateChange::CONNECT:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] " 
                  << "New client connection";
        if (conn->pool->clientCtx.get())
            ZeroCopyOpenSSL::attachTransport(p, conn->pool->clientCtx.get());

        conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                     PeerStatusListener::CONNECTED);
        conn->handler->connected();
        break;
    case yajr::StateChange::DISCONNECT:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] " 
                  << "Disconnected";
        if (!conn->closing)
            conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                         PeerStatusListener::CONNECTING);
        break;
    case yajr::StateChange::TRANSPORT_FAILURE:
        {
            char buf[120];
            ERR_error_string_n(error, buf, sizeof(buf));
            LOG(ERROR) << "[" << conn->getRemotePeer() << "] " 
                       << "SSL Connection error: " << buf;
        }
        if (conn->closing)
            conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                         PeerStatusListener::DISCONNECTED);
        else
            conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                         PeerStatusListener::CONNECTING);
        break;
    case yajr::StateChange::FAILURE:
        LOG(ERROR) << "[" << conn->getRemotePeer() << "] " 
                   << "Connection error: " << uv_strerror(error);
        if (conn->closing)
            conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                         PeerStatusListener::DISCONNECTED);
        else
            conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                         PeerStatusListener::CONNECTING);
        break;
    case yajr::StateChange::DELETE:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] " 
                  << "Connection closed";
        conn->getPool()->connectionClosed(conn);
        break;
    }
}
#endif

#ifdef SIMPLE_RPC
void OpflexClientConnection::write(const rapidjson::StringBuffer* buf) {
    OpflexConnection::write((uv_stream_t*)&socket, buf);
}
#endif

void OpflexClientConnection::messagesReady() {
    pool->messagesReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
