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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <stdexcept>

#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>

#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/logging/internal/logging.hpp"
#include "RecursiveLockGuard.h"

#ifndef SIMPLE_RPC
#include <yajr/internal/comms.hpp>
#endif

namespace opflex {
namespace engine {
namespace internal {

using std::string;
using util::LockGuard;
#ifndef SIMPLE_RPC
using yajr::transport::ZeroCopyOpenSSL;
#endif

OpflexListener::OpflexListener(HandlerFactory& handlerFactory_,
                               int port_,
                               const std::string& name_,
                               const std::string& domain_)
    : handlerFactory(handlerFactory_), port(port_), 
      name(name_), domain(domain_), active(true) {
    uv_mutex_init(&conn_mutex);
    uv_key_create(&conn_mutex_key);
}

OpflexListener::~OpflexListener() {
    uv_key_delete(&conn_mutex_key);
    uv_mutex_destroy(&conn_mutex);
}

void OpflexListener::enableSSL(const std::string& caStorePath,
                               const std::string& serverKeyPath,
                               const std::string& serverKeyPass,
                               bool verifyPeers) {
#ifndef SIMPLE_RPC
    OpflexConnection::initSSL();
    serverCtx.reset(ZeroCopyOpenSSL::Ctx::createCtx(caStorePath.c_str(), 
                                                    serverKeyPath.c_str(), 
                                                    serverKeyPass.c_str()));
    if (!serverCtx.get())
        throw std::runtime_error("Could not enable SSL");
#endif
}

void OpflexListener::on_cleanup_async(uv_async_t* handle) {
    OpflexListener* listener = (OpflexListener*)handle->data;

    {
        util::RecursiveLockGuard guard(&listener->conn_mutex, 
                                       &listener->conn_mutex_key);
        conn_set_t conns(listener->conns);
        BOOST_FOREACH(OpflexServerConnection* conn, conns) {
            conn->close();
        }
        if (listener->conns.size() != 0) return;
    }

#ifdef SIMPLE_RPC
    uv_close((uv_handle_t*)&listener->bind_socket, NULL);
#endif
    uv_close((uv_handle_t*)&listener->writeq_async, NULL);
    uv_close((uv_handle_t*)handle, NULL);
#ifndef SIMPLE_RPC
    yajr::finiLoop(&listener->server_loop);
#endif
}

void OpflexListener::on_writeq_async(uv_async_t* handle) {
    OpflexListener* listener = (OpflexListener*)handle->data;
    util::RecursiveLockGuard guard(&listener->conn_mutex, 
                                   &listener->conn_mutex_key);
    BOOST_FOREACH(OpflexServerConnection* conn, listener->conns) {
        conn->processWriteQueue();
    }
}

void OpflexListener::listen() {
    int rc;
    uv_loop_init(&server_loop);
    cleanup_async.data = this;
    writeq_async.data = this;
    uv_async_init(&server_loop, &cleanup_async, on_cleanup_async);
    uv_async_init(&server_loop, &writeq_async, on_writeq_async);

#ifdef SIMPLE_RPC

    uv_tcp_init(&server_loop, &bind_socket);
    server_loop.data = this;
    bind_socket.data = this;

    struct sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(port);
    
    LOG(INFO) << "Binding to port " << port;
    rc = uv_tcp_bind(&bind_socket, (struct sockaddr*)&bind_addr, 0);
    if (rc) {
        throw std::runtime_error(string("Could not bind to socket: ") + 
                                 uv_strerror(rc));
    }

    rc = uv_listen((uv_stream_t*)&bind_socket, 128, on_new_connection);
    if (rc < 0) {
        throw std::runtime_error(string("Could not listen for connections: ") +
                                 uv_strerror(rc));
    }
#else
    yajr::initLoop(&server_loop);

    listener = yajr::Listener::create("0.0.0.0", port, 
                                      OpflexServerConnection::on_state_change,
                                      on_new_connection,
                                      this,
                                      &server_loop,
                                      OpflexServerConnection::loop_selector);
                                      
#endif
    rc = uv_thread_create(&server_thread, server_thread_func, this);
    if (rc < 0) {
        throw std::runtime_error(string("Could not create server thread: ") +
                                 uv_strerror(rc));
    }
}

void OpflexListener::disconnect() {
    if (!active) return;
    active = false;
    LOG(INFO) << "Shutting down Opflex listener";

    uv_async_send(&cleanup_async);
    uv_thread_join(&server_thread);
    uv_loop_close(&server_loop);
}

void OpflexListener::server_thread_func(void* listener_) {
    OpflexListener* processor = (OpflexListener*)listener_;
    uv_run(&processor->server_loop, UV_RUN_DEFAULT);
}

#ifdef SIMPLE_RPC
void OpflexListener::on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        LOG(ERROR) << "Error on new connection: " 
                   << uv_strerror(status);
    }
    OpflexListener* listener = (OpflexListener*)server->data;
    if (!listener->active) return;

    util::RecursiveLockGuard guard(&listener->conn_mutex, 
                                   &listener->conn_mutex_key);
    OpflexServerConnection* conn = new OpflexServerConnection(listener);
    listener->conns.insert(conn);
}

void OpflexListener::on_conn_closed(uv_handle_t *handle) {
    OpflexServerConnection* conn = (OpflexServerConnection*)handle->data;
    conn->getListener()->connectionClosed(conn);
}
#else
void* OpflexListener::on_new_connection(yajr::Listener* ylistener, 
                                        void* data, int error) {
    if (error < 0) {
        LOG(ERROR) << "Error on new connection: " 
                   << uv_strerror(error);
    }

    OpflexListener* listener = (OpflexListener*)data;
    util::RecursiveLockGuard guard(&listener->conn_mutex, 
                                   &listener->conn_mutex_key);
    OpflexServerConnection* conn = new OpflexServerConnection(listener);
    listener->conns.insert(conn);
    return conn;
}
#endif

void OpflexListener::doConnectionClosed(OpflexServerConnection* conn) {
    conns.erase(conn);
    delete conn;
}

void OpflexListener::connectionClosed(OpflexServerConnection* conn) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    doConnectionClosed(conn);
    guard.release();
    if (!active)
        uv_async_send(&cleanup_async);
}

void OpflexListener::sendToAll(OpflexMessage* message) {
    boost::scoped_ptr<OpflexMessage> messagep(message);
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    if (!active) return;
    BOOST_FOREACH(OpflexServerConnection* conn, conns) {
        // this is inefficient but we only use this for testing
        conn->sendMessage(message->clone());
    }
}

bool OpflexListener::applyConnPred(conn_pred_t pred, void* user) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    BOOST_FOREACH(OpflexServerConnection* conn, conns) {
        if (!pred(conn, user)) return false;
    }
    return true;
}

void OpflexListener::messagesReady() {
    uv_async_send(&writeq_async);
}

bool OpflexListener::isListening() {
#ifdef SIMPLE_RPC
    return true;
#else
    using yajr::comms::internal::Peer;
    return Peer::LoopData::getPeerList(&server_loop, 
                                       Peer::LoopData::LISTENING)->size() > 0;
#endif
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
