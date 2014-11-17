/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexListener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <stdexcept>

#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;
using util::LockGuard;

OpflexListener::OpflexListener(HandlerFactory& handlerFactory_,
                               int port_,
                               const std::string& name_,
                               const std::string& domain_)
    : handlerFactory(handlerFactory_), port(port_), 
      name(name_), domain(domain_), active(true) {
    uv_mutex_init(&conn_mutex);
    uv_loop_init(&server_loop);
    uv_async_init(&server_loop, &async, on_async);
    async.data = this;

    uv_tcp_init(&server_loop, &bind_socket);
    server_loop.data = this;
    bind_socket.data = this;

    struct sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(port);
    
    LOG(INFO) << "Binding to port " << port_;
    int rc = uv_tcp_bind(&bind_socket, (struct sockaddr*)&bind_addr, 0);
    if (rc) {
        throw std::runtime_error(string("Could not bind to socket: ") + 
                                 uv_strerror(rc));
    }

    rc = uv_listen((uv_stream_t*)&bind_socket, 128, on_new_connection);
    if (rc < 0) {
        throw std::runtime_error(string("Could not listen for connections: ") +
                                 uv_strerror(rc));
    }

    rc = uv_thread_create(&server_thread, server_thread_func, this);
    if (rc < 0) {
        throw std::runtime_error(string("Could not create server thread: ") +
                                 uv_strerror(rc));
    }
}

OpflexListener::~OpflexListener() {
    disconnect();
    uv_mutex_destroy(&conn_mutex);
}

void OpflexListener::on_async(uv_async_t* handle) {
    OpflexListener* listener = (OpflexListener*)handle->data;

    {
        LockGuard guard(&listener->conn_mutex);
        BOOST_FOREACH(OpflexServerConnection* conn, listener->conns) {
            conn->disconnect();
        }
    }

    uv_close((uv_handle_t*)&listener->bind_socket, NULL);
    uv_close((uv_handle_t*)handle, NULL);
}

void OpflexListener::disconnect() {
    if (!active) return;
    active = false;
    LOG(INFO) << "Shutting down Opflex listener";

    uv_async_send(&async);
    uv_thread_join(&server_thread);
    uv_loop_close(&server_loop);
}

void OpflexListener::server_thread_func(void* listener_) {
    OpflexListener* processor = (OpflexListener*)listener_;
    uv_run(&processor->server_loop, UV_RUN_DEFAULT);
}

void OpflexListener::on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        LOG(ERROR) << "Error on new connection: " 
                   << uv_strerror(status);
    }
    OpflexListener* listener = (OpflexListener*)server->data;
    if (!listener->active) return;

    LockGuard guard(&listener->conn_mutex);
    OpflexServerConnection* conn = new OpflexServerConnection(listener);
    listener->conns.insert(conn);
}

void OpflexListener::on_conn_closed(uv_handle_t *handle) {
    OpflexServerConnection* conn = (OpflexServerConnection*)handle->data;
    OpflexListener* listener = conn->getListener();
    if (conn != NULL) {
        LockGuard guard(&listener->conn_mutex);
        listener->conns.erase(conn);
        delete conn;
    }
}

void OpflexListener::writeToAll(OpflexMessage& message) {
    LockGuard guard(&conn_mutex);
    BOOST_FOREACH(OpflexServerConnection* conn, conns) {
        // this is inefficient but we only use this for testing
        conn->write(message.serialize());
    }
}

bool OpflexListener::applyConnPred(conn_pred_t pred, void* user) {
    LockGuard guard(&conn_mutex);
    BOOST_FOREACH(OpflexServerConnection* conn, conns) {
        if (!pred(conn, user)) return false;
    }
    return true;
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
