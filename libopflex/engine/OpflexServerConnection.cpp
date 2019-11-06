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
#include "opflex/engine/internal/MockOpflexServerImpl.h"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;
using yajr::transport::ZeroCopyOpenSSL;
using opflex::gbp::PolicyUpdateOp;

OpflexServerConnection::OpflexServerConnection(OpflexListener* listener_)
    : OpflexConnection(listener_->handlerFactory),
      listener(listener_), peer(NULL) {
      int rc;

      uv_loop_init(&server_loop);
      policy_update_async.data = this;
      uv_async_init(&server_loop, &policy_update_async, on_policy_update_async);
      cleanup_async.data = this;
      uv_async_init(&server_loop, &cleanup_async, on_cleanup_async);
      prr_timer_async.data = this;
      uv_async_init(&server_loop, &prr_timer_async, on_prr_timer_async);
      rc = uv_thread_create(&server_thread, server_thread_entry, this);
      if (rc < 0) {
          throw std::runtime_error(string("Could not create policy update thread: ") +
                                   uv_strerror(rc));
      }
}

OpflexServerConnection::~OpflexServerConnection() {
      uv_async_send(&cleanup_async);
      uv_thread_join(&server_thread);
      uv_loop_close(&server_loop);
}

void OpflexServerConnection::server_thread_entry(void *data) {
    OpflexServerConnection* conn = (OpflexServerConnection*)data;
    uv_run(&conn->server_loop, UV_RUN_DEFAULT);
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
        uint16_t port;
        inet_ntop(name.ss_family,
                  name.ss_family == AF_INET
                  ? (void *) &(((struct sockaddr_in*)&name)->sin_addr)
                  : (void *) &(((struct sockaddr_in6*)&name)->sin6_addr),
                  addrbuffer, INET6_ADDRSTRLEN);
        port = name.ss_family == AF_INET
               ? ntohs(((struct sockaddr_in*)&name)->sin_port)
               : ntohs(((struct sockaddr_in6*)&name)->sin6_port);
        remote_peer = std::string(addrbuffer) + ":" + std::to_string(port);
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
            p->startKeepAlive();

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

void OpflexServerConnection::disconnect() {
    handler->disconnected();
    OpflexConnection::disconnect();

    if (peer)
        peer->destroy();
}

void OpflexServerConnection::messagesReady() {
    listener->messagesReady();
}

void OpflexServerConnection::addUri(const opflex::modb::URI& uri,
                                    int64_t lifetime) {
    std::lock_guard<std::mutex> lock(uri_map_mutex);

    uri_map[uri] = lifetime;
}

bool OpflexServerConnection::getUri(const opflex::modb::URI& uri) {
    std::lock_guard<std::mutex> lock(uri_map_mutex);

    auto it = uri_map.find(uri);
    return (it != uri_map.end());
}

bool OpflexServerConnection::clearUri(const opflex::modb::URI& uri) {
    std::lock_guard<std::mutex> lock(uri_map_mutex);

    return (0 != uri_map.erase(uri));
}

void OpflexServerConnection::addPendingUpdate(opflex::modb::class_id_t class_id,
                                              const opflex::modb::URI& uri,
                                              PolicyUpdateOp op) {
    std::lock_guard<std::mutex> lock(uri_update_mutex);
    switch (op) {
    case PolicyUpdateOp::REPLACE:
        replace.push_back(std::make_pair(class_id, uri));
        break;
    case PolicyUpdateOp::DELETE:
    case PolicyUpdateOp::DELETE_RECURSIVE:
        deleted.push_back(std::make_pair(class_id, uri));
        break;
    case PolicyUpdateOp::ADD:
        merge.push_back(std::make_pair(class_id, uri));
        break;
    default:
        break;
    }
}

void OpflexServerConnection::sendUpdates() {
    uv_async_send(&policy_update_async);
}

void OpflexServerConnection::sendTimeouts() {
    uv_async_send(&prr_timer_async);
}

void OpflexServerConnection::on_policy_update_async(uv_async_t* handle) {
    OpflexServerConnection* conn = (OpflexServerConnection *)handle->data;
    MockOpflexServerImpl* server = dynamic_cast<MockOpflexServerImpl*>
        (conn->listener->getHandlerFactory());
    std::lock_guard<std::mutex> lock(conn->uri_update_mutex);

    if (conn->replace.empty() && conn->merge.empty() && conn->deleted.empty())
        return;

    server->policyUpdate(conn, conn->replace, conn->merge, conn->deleted);

    conn->replace.clear();
    conn->merge.clear();
    conn->deleted.clear();
}

void OpflexServerConnection::on_prr_timer_async(uv_async_t* handle) {
    OpflexServerConnection* conn = (OpflexServerConnection *)handle->data;
    MockOpflexServerImpl* server = dynamic_cast<MockOpflexServerImpl*>
        (conn->listener->getHandlerFactory());
    std::lock_guard<std::mutex> lock(conn->uri_update_mutex);

    auto it = conn->uri_map.begin();
    while (it != conn->uri_map.end()) {
        it->second -= server->getPrrIntervalSecs();
        if (it->second <= 0)
            it = conn->uri_map.erase(it);
        else
            it++;
    }
}

void OpflexServerConnection::on_cleanup_async(uv_async_t* handle) {
    OpflexServerConnection* conn = (OpflexServerConnection *)handle->data;

    uv_close((uv_handle_t*)&conn->policy_update_async, NULL);
    uv_close((uv_handle_t*)&conn->prr_timer_async, NULL);
    uv_close((uv_handle_t*)handle, NULL);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
