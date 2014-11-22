/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexClientConnection.h
 * @brief Interface definition file for OpflexClientConnection
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexConnection.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXCLIENTCONNECTION_H
#define OPFLEX_ENGINE_OPFLEXCLIENTCONNECTION_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;

/**
 * Maintain the clientConnection state information for a clientConnection to an
 * opflex peer
 */
class OpflexClientConnection : public OpflexConnection {
public:
    /**
     * Create a new opflex clientConnection for the given hostname and port
     *
     * @param handlerFactory a factory that can allocate a handler for
     * the connection
     * @param pool the connection pool that hosts this connection
     * @param hostname the hostname or IP address for the remote peer
     * @param port the TCP port number of the remote peer
     */
    OpflexClientConnection(HandlerFactory& handlerFactory,
                           OpflexPool* pool,
                           const std::string& hostname, 
                           int port);
    virtual ~OpflexClientConnection();

    /**
     * Connect to the remote peer
     */
    virtual void connect();

    /**
     * Disconnect this connection from the remote peer
     */
    virtual void disconnect();

    /**
     * Close the connection
     */ 
    virtual void close();

    /**
     * Get the connection pool that hosts this connection
     *
     * @return a pointer to the OpflexPool
     */
    OpflexPool* getPool() { return pool; }

    /**
     * Get the unique name for this component in the policy domain
     *
     * @returns the string name
     */
    virtual const std::string& getName();

    /**
     * Get the globally unique name for this policy domain
     *
     * @returns the string domain name
     */
    virtual const std::string& getDomain();

    /**
     * Get the hostname for this connection
     */
    const std::string& getHostname() const { return hostname; }

    /**
     * Get the port for this connection
     */
    int getPort() const { return port; }

    virtual const std::string& getRemotePeer() { return remote_peer; }

#ifdef SIMPLE_RPC
    virtual void write(const rapidjson::StringBuffer* buf);
    bool shouldRetry() { return retry; }
#else
    virtual yajr::Peer* getPeer() { return peer; }
#endif
    virtual void messagesReady();

private:
    OpflexPool* pool;

    std::string hostname;
    int port;
    std::string remote_peer;

#ifdef SIMPLE_RPC
    uv_tcp_t socket;
    uv_connect_t connect_req;
    uv_shutdown_t shutdown;
    bool retry;
#else
    yajr::Peer* peer;
#endif
    volatile bool started;
    volatile bool active;

#ifdef SIMPLE_RPC
    static void connect_cb(uv_connect_t* req, int status);
#else
    static void on_state_change(yajr::Peer* p, void* data, 
                                yajr::StateChange::To stateChange,
                                int error);
    static uv_loop_t* loop_selector(void* data);
#endif

protected:
#ifdef SIMPLE_RPC
    /**
     * Handler for connection close
     */
    static void on_conn_closed(uv_handle_t *handle);
#endif
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
