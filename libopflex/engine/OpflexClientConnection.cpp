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

using yajr::Peer;
using yajr::transport::ZeroCopyOpenSSL;
using ofcore::PeerStatusListener;

OpflexClientConnection::OpflexClientConnection(HandlerFactory& handlerFactory,
                                               OpflexPool* pool_,
                                               const string& hostname_,
                                               int port_)
    : OpflexConnection(handlerFactory),
      pool(pool_), hostname(hostname_), port(port_), role(0), peer(NULL),
      started(false), active(false), closing(false), ready(false),
      failureCount(0), handshake_timer(NULL) {
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
    ready = true;
    uv_timer_stop(handshake_timer);
    pool->updatePeerStatus(hostname, port, PeerStatusListener::READY);
    failureCount = 0;
}

void OpflexClientConnection::notifyFailed() {
    connectionFailure();
}

void OpflexClientConnection::connect() {
    if (started) return;
    started = true;
    active = true;
    ready = false;

    handshake_timer = new uv_timer_t;
    uv_timer_init(pool->client_loop, handshake_timer);
    handshake_timer->data = this;

    pool->updatePeerStatus(hostname, port, PeerStatusListener::CONNECTING);

    std::stringstream rp;
    rp << hostname << ":" << port;
    remote_peer = rp.str();

    peer = yajr::Peer::create(hostname,
                              boost::lexical_cast<string>(port),
                              on_state_change,
                              this, loop_selector);
}

void OpflexClientConnection::disconnect() {
    if (!active) return;

    LOG(DEBUG) << "[" << getRemotePeer() << "] "
               << "Disconnecting";
    if (handshake_timer)
        uv_timer_stop(handshake_timer);
    peer->disconnect();
}

void OpflexClientConnection::close() {
    if (closing) return;

    LOG(DEBUG) << "[" << getRemotePeer() << "] "
               << "Closing";

    closing = true;
    pool->updatePeerStatus(hostname, port, PeerStatusListener::CLOSING);
    active = false;
    if (handshake_timer) {
        uv_timer_stop(handshake_timer);
        handshake_timer->data = NULL;
        uv_close((uv_handle_t*)handshake_timer, on_timer_close);
    }
    handler->disconnected();
    OpflexConnection::disconnect();
    if (peer)
        peer->destroy();
}

uv_loop_t* OpflexClientConnection::loop_selector(void * data) {
    OpflexClientConnection* conn = (OpflexClientConnection*)data;
    return conn->getPool()->getLoop();
}

void OpflexClientConnection::on_handshake_timer(uv_timer_t* handle) {
    OpflexClientConnection* conn = (OpflexClientConnection*)handle->data;
    if (conn == NULL) return;
    LOG(ERROR) << "[" << conn->getRemotePeer() << "] "
               << "Handshake timed out";
    conn->disconnect();
}

void OpflexClientConnection::on_timer_close(uv_handle_t* handle) {
    delete (uv_timer_t*)handle;
}

void OpflexClientConnection::on_state_change(Peer * p, void * data,
                                             yajr::StateChange::To stateChange,
                                             int error) {
    OpflexClientConnection* conn = (OpflexClientConnection*)data;
    switch (stateChange) {
    case yajr::StateChange::CONNECT:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] "
                  << "New client connection";
        conn->active = true;
        conn->ready = false;
        uv_timer_stop(conn->handshake_timer);
        uv_timer_start(conn->handshake_timer,
                       on_handshake_timer, 30000, 0);

        if (conn->pool->clientCtx.get())
            ZeroCopyOpenSSL::attachTransport(p, conn->pool->clientCtx.get());
        p->startKeepAlive(5000, 15000, 30000);

        conn->pool->updatePeerStatus(conn->hostname, conn->port,
                                     PeerStatusListener::CONNECTED);
        conn->handler->connected();
        break;
    case yajr::StateChange::DISCONNECT:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] "
                  << "Disconnected";

        uv_timer_stop(conn->handshake_timer);
        conn->active = false;
        conn->ready = false;
        conn->handler->disconnected();
        conn->cleanup();

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
            conn->connectionFailure();
        }
        break;
    case yajr::StateChange::FAILURE:
        LOG(ERROR) << "[" << conn->getRemotePeer() << "] "
                   << "Connection error: " << uv_strerror(error);
        conn->connectionFailure();
        break;
    case yajr::StateChange::DELETE:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] "
                  << "Connection closed";
        conn->getPool()->connectionClosed(conn);
        break;
    }
}

void OpflexClientConnection::connectionFailure() {
    uv_timer_stop(handshake_timer);
    ready = false;
    if (!closing)
        disconnect();
    if (!closing && failureCount >= 3 &&
        !pool->isConfiguredPeer(hostname, port)) {
        LOG(ERROR) << "[" << getRemotePeer() << "] "
                   << "Giving up on bootstrapped peer after " << failureCount
                   << " failures";
        close();
        pool->addConfiguredPeers();
    } else {
        failureCount += 1;
    }
}

void OpflexClientConnection::messagesReady() {
    pool->messagesReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
