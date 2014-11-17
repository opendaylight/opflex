/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexListener.h
 * @brief Interface definition file for OpflexListener
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <set>
#include <netinet/in.h>

#include <uv.h>

#include "opflex/engine/internal/OpflexServerConnection.h"
#include "opflex/engine/internal/OpflexMessage.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXLISTENER_H
#define OPFLEX_ENGINE_OPFLEXLISTENER_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;

/**
 * Represents a listen socket that will spawn OpflexServerConnections
 */
class OpflexListener {
public:
    /**
     * Create a new opflex listener for the given IP address and port
     *
     * @param handlerFactory a factory that can allocate a handler for
     * the spawned OpflexServerConnection objects
     * @param port the TCP port number to bind to
     * @param name the opflex name for the server
     * @param domain the opflex domain for the server
     */
    OpflexListener(HandlerFactory& handlerFactory,
                   int port,
                   const std::string& name,
                   const std::string& domain);
    ~OpflexListener();

    /**
     * Stop listening on the local socket for new connections
     */
    void disconnect();

    /**
     * Get the bind port for this connection
     */
    int getPort() const { return port; }

    /**
     * Get the unique name for this component in the policy domain
     *
     * @returns the string name
     */
    const std::string& getName() { return name; }

    /**
     * Get the globally unique name for this policy domain
     *
     * @returns the string domain name
     */
    const std::string& getDomain() { return domain; }

    /**
     * Write a given message to all the connected and ready peers.
     *
     * @param message the message to write
     */
    void writeToAll(OpflexMessage& message);

    /**
     * A predicate for use with applyConnPred
     */
    typedef bool (*conn_pred_t)(OpflexServerConnection* conn, void* user);

    /**
     * Apply the given predicate to all connection objects, returning
     * true if the predicate is true for all connections
     */
    bool applyConnPred(conn_pred_t pred, void* user);

private:
    HandlerFactory& handlerFactory;

    std::string hostname;
    int port;

    std::string name;
    std::string domain;

    volatile bool active;

    uv_loop_t server_loop;
    uv_thread_t server_thread;

    uv_tcp_t bind_socket;

    uv_mutex_t conn_mutex;
    typedef std::set<OpflexServerConnection*> conn_set_t;
    conn_set_t conns;

    uv_async_t async;

    static void server_thread_func(void* processor);
    static void on_new_connection(uv_stream_t *server, int status);
    static void on_conn_closed(uv_handle_t *handle);
    static void on_async(uv_async_t *handle);

    friend class OpflexServerConnection;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
