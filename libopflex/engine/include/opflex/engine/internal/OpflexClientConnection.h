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

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXCLIENTCONNECTION_H
#define OPFLEX_ENGINE_OPFLEXCLIENTCONNECTION_H

#include "opflex/engine/internal/OpflexConnection.h"

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

    virtual yajr::Peer* getPeer() { return peer; }
    virtual void messagesReady();
    virtual void setRoles(uint8_t _role) { role = _role; }
    virtual uint8_t getRoles() { return role; }

private:
    OpflexPool* pool;

    std::string hostname;
    int port;
    std::string remote_peer;
    uint8_t role;

    yajr::Peer* peer;

    bool started;
    bool active;
    bool closing;
    bool ready;
    int failureCount;

    uv_timer_t* handshake_timer;

    static uv_loop_t* loop_selector(void* data);
    static void on_handshake_timer(uv_timer_t* handle);
    static void on_timer_close(uv_handle_t* handle);

    virtual void notifyReady();
    virtual void notifyFailed();

protected:
    static void on_state_change(yajr::Peer* p, void* data,
                                yajr::StateChange::To stateChange,
                                int error);
    void connectionFailure();

};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
