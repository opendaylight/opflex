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
#include <memory>

#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>
#include <uv.h>

#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexClientConnection.h"
#include "opflex/ofcore/PeerStatusListener.h"
#ifndef SIMPLE_RPC
#include "yajr/transport/ZeroCopyOpenSSL.hpp"
#endif

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXPOOL_H
#define OPFLEX_ENGINE_OPFLEXPOOL_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexMessage;

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
     * Enable SSL for connections to opflex peers
     *
     * @param caStorePath the filesystem path to a directory
     * containing CA certificates, or to a file containing a specific
     * CA certificate.
     * @param verifyPeers set to true to verify that peer certificates
     * properly chain to a trusted root
     */
    void enableSSL(const std::string& caStorePath, bool verifyPeers = true);

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
     * Register the given peer status listener to get updates on the
     * health of the connection pool and on individual connections.
     *
     * @param listener the listener to register
     */
    void registerPeerStatusListener(ofcore::PeerStatusListener* listener);

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
    OpflexClientConnection* getMasterForRole(ofcore::OFConstants::OpflexRole role);

    /**
     * Send a given message to all the connected and ready peers with
     * the given role.  This message can be called from any thread.
     *
     * @param message the message to write.  The memory will be owned by the pool.
     * @param role the role to which the message should be sent
     * @param sync if true then this is being called from the libuv
     * thread
     */
    void sendToRole(OpflexMessage* message, 
                    ofcore::OFConstants::OpflexRole role,
                    bool sync = false);

    /**
     * Get the number of connections in a particular role
     *
     * @param role the role to search for
     * @return the count of connections in that role
     */
    int getRoleCount(ofcore::OFConstants::OpflexRole role);

private:
    HandlerFactory& factory;

    /** opflex unique name */
    std::string name;
    /** globally unique opflex domain */
    std::string domain;

    std::auto_ptr<yajr::transport::ZeroCopyOpenSSL::Ctx> clientCtx;

    uv_mutex_t conn_mutex;
    uv_key_t conn_mutex_key;

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
    bool active;

    uv_loop_t client_loop;
    uv_thread_t client_thread;
    uv_async_t conn_async;
    uv_async_t cleanup_async;
    uv_async_t writeq_async;
    uv_timer_t timer;

    std::list<ofcore::PeerStatusListener*> peerStatusListeners;
    ofcore::PeerStatusListener::Health curHealth;

    void doRemovePeer(const std::string& hostname, int port);
    void doAddPeer(const std::string& hostname, int port);
    void doSetRoles(ConnData& cd, uint8_t newroles);
    void updateRole(ConnData& cd, uint8_t newroles, 
                    ofcore::OFConstants::OpflexRole role);
    void connectionClosed(OpflexClientConnection* conn);
    void doConnectionClosed(OpflexClientConnection* conn);
    uv_loop_t* getLoop() { return &client_loop; }
    void messagesReady();

    static void client_thread_func(void* pool);
    static void on_conn_async(uv_async_t *handle);
    static void on_cleanup_async(uv_async_t *handle);
    static void on_writeq_async(uv_async_t *handle);
#ifdef SIMPLE_RPC
    static void on_conn_closed(OpflexClientConnection* conn);
    static void on_timer(uv_timer_t* timer);
#endif

    void updatePeerStatus(const std::string& hostname, int port,
                          ofcore::PeerStatusListener::PeerStatus status);

    friend class OpflexClientConnection;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXPOOL_H */
