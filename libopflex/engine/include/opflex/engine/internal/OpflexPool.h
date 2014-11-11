/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexPool.h
 * @brief Interface definition file for OpflexPool
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <utility>
#include <map>
#include <set>

#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>
#include <uv.h>

#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/OpflexClientConnection.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXPOOL_H
#define OPFLEX_ENGINE_OPFLEXPOOL_H

namespace opflex {
namespace engine {
namespace internal {

/**
 * A pool of OpFlex connections that will keep track of connection
 * state and assign a master 
 */
class OpflexPool : private boost::noncopyable {
public:
    /**
     * Allocate a new opflex connection pool
     */
    OpflexPool(HandlerFactory& factory);
    ~OpflexPool();

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
     * Set the opflex identity information for this pool
     *
     * @param name the unique name for this opflex component within
     * the policy domain
     * @param domain the globally unique name for this policy domain
     */
    void setOpflexIdentity(const std::string& name,
                           const std::string& domain);

    /**
     * Start the pool
     */
    void start();

    /**
     * Stop the pool
     */
    void stop();

    /**
     * Disconnect and clear all active OpFlex connections
     */
    void clear();

    /**
     * Add an OpFlex peer.
     *
     * @param hostname the hostname or IP address to connect to
     * @param port the TCP port to connect on
     * @see opflex::ofcore::OFFramework::addPeer
     */
    void addPeer(const std::string& hostname,
                 int port);

    /**
     * Add an OpFlex peer.
     *
     * @param conn the OpflexClientConnection to add
     */
    void addPeer(OpflexClientConnection* conn);

    /**
     * Get the peer that is connecting to the specified host and port.
     *
     * @param hostname the remote hostname
     * @param port the port on the remote host
     * @returns the client connection, or NULL if there is no such
     * connection
     */
    OpflexClientConnection* getPeer(const std::string& hostname, int port);

    /**
     * Set the roles for the specified connection
     */
    void setRoles(OpflexClientConnection* conn,
                  uint8_t roles);

    /**
     * Get the primary connection for the given role.  This will be
     * the first connection in the set of ready connections with that
     * role.
     *
     * @return the master for the role, or NULL if there is no ready
     * connection with that role
     */
    OpflexClientConnection* getMasterForRole(OpflexHandler::OpflexRole role);

    /**
     * Write a given message to all the connected and ready peers with
     * the given role.
     *
     * @param message the message to write
     * @param role
     */
    void writeToRole(OpflexMessage& message, OpflexHandler::OpflexRole role);

private:
    HandlerFactory& factory;

    /** opflex unique name */
    std::string name;
    /** globally unique opflex domain */
    std::string domain;

    uv_mutex_t conn_mutex;

    class ConnData {
    public:
        OpflexClientConnection* conn;
        uint8_t roles;
    };

    typedef std::pair<std::string, int> peer_name_t;
    typedef boost::unordered_map<peer_name_t, ConnData> conn_map_t;
    typedef std::set<OpflexClientConnection*> conn_set_t;

    class RoleData {
    public:
        conn_set_t conns;
        OpflexClientConnection* curMaster;
    };

    typedef std::map<uint8_t, RoleData> role_map_t;

    conn_map_t connections;
    role_map_t roles;

    uv_loop_t client_loop;
    uv_thread_t client_thread;
    uv_timer_t timer;
    uv_async_t async;

    void doRemovePeer(const std::string& hostname, int port);
    void doSetRoles(ConnData& cd, uint8_t newroles);
    void updateRole(ConnData& cd, uint8_t newroles, 
                    OpflexHandler::OpflexRole role);

    static void client_thread_func(void* pool);
    static void timer_cb(uv_timer_t* handle);
    static void on_conn_closed(uv_handle_t *handle);

    friend class OpflexClientConnection;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXPOOL_H */
