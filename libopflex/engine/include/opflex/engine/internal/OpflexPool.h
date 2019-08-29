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

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/functional/hash.hpp>
#include <boost/asio.hpp>
#include <uv.h>

#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexClientConnection.h"
#include "opflex/ofcore/PeerStatusListener.h"
#include "opflex/ofcore/OFTypes.h"
#include "opflex/modb/MAC.h"
#include "yajr/transport/ZeroCopyOpenSSL.hpp"
#include "ThreadManager.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXPOOL_H
#define OPFLEX_ENGINE_OPFLEXPOOL_H

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if __cplusplus > 199711L

namespace std {
/**
 * Template specialization for std::hash<OpflexPool::peer_name_t>, making
 * prop_key_t suitable as a key in a std::unordered_map
 */
template<> struct hash<std::pair<std::string, int>> {
    /**
     * Hash the peer_name_t
     */
    std::size_t operator()(const std::pair<std::string, int>& u) const {
        return boost::hash_value(u);
    }
};

} /* namespace std */

#endif

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
     * @param factory a factory for generating new connection handlers
     * upon connection.
     * @param threadManager thread manager
     */
    OpflexPool(HandlerFactory& factory, util::ThreadManager& threadManager);
    ~OpflexPool();

    /**
     * Get the unique name for this component in the policy domain
     *
     * @return the string name
     */
    const std::string& getName() { return name; }

    /**
     * Get the globally unique name for this policy domain
     *
     * @return the string domain name
     */
    const std::string& getDomain() { return domain; }

    /**
     * Get a copy of the location string for this policy element, if
     * it has been set
     *
     * @return the location string
     */
    boost::optional<std::string> getLocation();

    /**
     * Set the location string for this policy element
     *
     * @param location the location string
     */
    void setLocation(const std::string& location);

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
     * Set the opflex identity information for this pool
     *
     * @param name the unique name for this opflex component within
     * the policy domain
     * @param domain the globally unique name for this policy domain
     * @param location the location string for this policy element.
     */
    void setOpflexIdentity(const std::string& name,
                           const std::string& domain,
                           const std::string& location);

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
    void enableSSL(const std::string& caStorePath,
                   bool verifyPeers = true);

    /**
     * Enable SSL for connections to opflex peers
     *
     * @param caStorePath the filesystem path to a directory
     * containing CA certificates, or to a file containing a specific
     * CA certificate.
     * @param keyAndCertFilePath the path to the PEM file for this peer,
     * containing its certificate and its private key, possibly encrypted.
     * @param passphrase the passphrase to be used to decrypt the private
     * key within this peer's PEM file
     * @param verifyPeers set to true to verify that peer certificates
     * properly chain to a trusted root
     */
    void enableSSL(const std::string& caStorePath,
                   const std::string& keyAndCertFilePath,
                   const std::string& passphrase,
                   bool verifyPeers = true);

    /**
     * Add an OpFlex peer.
     *
     * @param hostname the hostname or IP address to connect to
     * @param port the TCP port to connect on
     * @param configured true if this peer was configured, false if it
     * was added because of bootstrapping
     * @see opflex::ofcore::OFFramework::addPeer
     */
    void addPeer(const std::string& hostname,
                 int port, bool configured = true);

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
     * @return the client connection, or NULL if there is no such
     * connection
     */
    OpflexClientConnection* getPeer(const std::string& hostname, int port);

    void resetAllPeers();

    /**
     * Register the given peer status listener to get updates on the
     * health of the connection pool and on individual connections.
     *
     * @param listener the listener to register
     */
    void registerPeerStatusListener(ofcore::PeerStatusListener* listener);

    /**
     * Set the roles for the specified connection
     *
     * @param conn the connection to change the roles for
     * @param roles the new roles bitmask
     */
    void setRoles(OpflexClientConnection* conn, uint8_t roles);

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
     * @return the number of ready connections to which we sent the message
     */
    size_t sendToRole(OpflexMessage* message,
                      ofcore::OFConstants::OpflexRole role,
                      bool sync = false);

    /**
     * Get the number of connections in a particular role
     *
     * @param role the role to search for
     * @return the count of connections in that role
     */
size_t getRoleCount(ofcore::OFConstants::OpflexRole role);

    /**
     * Check whether the given port and hostname is in the set of
     * configured peers (as opposed to peers learned through
     * bootstrapping)
     *
     * @param hostname the hostname
     * @param port the port number
     * @return true if the hostname and port represent a configured
     * peer
     */
    bool isConfiguredPeer(const std::string& hostname, int port);

    /**
     * A peer name.  Hostname, port number
     */
    typedef std::pair<std::string, int> peer_name_t;
    /**
     * A set of peer names
     */
    typedef OF_UNORDERED_SET<peer_name_t> peer_name_set_t;

    /**
     * Update the set of connections in the pool to include only
     * configured peers and the peers that appear in the provided set
     * of peer names.
     *
     * @param conn the current connection on which peer set was received
     * @param peerNames the set of peer names to validate against
     */
    void validatePeerSet(OpflexClientConnection *conn,
        const peer_name_set_t& peers);

    /**
     * Add configured peers back into the connection pool
     */
    void addConfiguredPeers();

    typedef opflex::ofcore::OFConstants::OpflexElementMode AgentMode;

    void setClientMode(AgentMode mode) {
        client_mode = mode;
    }
    AgentMode getClientMode() {
        return client_mode;
    }
    void setV4Proxy(const boost::asio::ip::address_v4& v4_proxy) {
        ipv4_proxy = v4_proxy;
    }
    void setV6Proxy(const boost::asio::ip::address_v4& v6_proxy) {
        ipv6_proxy = v6_proxy;
    }
    void setMacProxy(const boost::asio::ip::address_v4& l2a_proxy) {
        mac_proxy = l2a_proxy;
    }

    boost::asio::ip::address_v4 getV4Proxy() {
        return ipv4_proxy;
    }

    boost::asio::ip::address_v4 getV6Proxy() {
        return ipv6_proxy;
    }

    boost::asio::ip::address_v4 getMacProxy() {
        return mac_proxy;
    }

    opflex::ofcore::OFConstants::OpflexTransportModeState
    getTransportModeState() {
        return transport_state;
    }

    void setTransportModeState(
       opflex::ofcore::OFConstants::OpflexTransportModeState state) {
         transport_state = state;
    }

    void setTunnelMac(const opflex::modb::MAC &mac) {
        tunnelMac = mac;
    }

    opflex::modb::MAC getTunnelMac() {
        return tunnelMac;
    }

private:
    HandlerFactory& factory;
    util::ThreadManager& threadManager;

    /** opflex unique name */
    std::string name;
    /** globally unique opflex domain */
    std::string domain;
    /** location string for this policy element */
    boost::optional<std::string> location;

#ifdef HAVE_CXX11
    std::unique_ptr<yajr::transport::ZeroCopyOpenSSL::Ctx> clientCtx;
#else
    std::auto_ptr<yajr::transport::ZeroCopyOpenSSL::Ctx> clientCtx;
#endif

    uv_mutex_t conn_mutex;
    uv_key_t conn_mutex_key;

    class ConnData {
    public:
        OpflexClientConnection* conn;
        uint8_t roles;
    };

    typedef OF_UNORDERED_MAP<peer_name_t, ConnData> conn_map_t;
    typedef std::set<OpflexClientConnection*> conn_set_t;

    class RoleData {
    public:
        conn_set_t conns;
        OpflexClientConnection* curMaster;
    };

    typedef std::map<uint8_t, RoleData> role_map_t;

    peer_name_set_t configured_peers;
    conn_map_t connections;
    role_map_t roles;
    bool active;

    opflex::ofcore::OFConstants::OpflexElementMode client_mode;
    opflex::ofcore::OFConstants::OpflexTransportModeState transport_state;
    boost::asio::ip::address_v4 ipv4_proxy;
    boost::asio::ip::address_v4 ipv6_proxy;
    boost::asio::ip::address_v4 mac_proxy;
    opflex::modb::MAC tunnelMac;

    uv_loop_t* client_loop;
    uv_async_t conn_async;
    uv_async_t cleanup_async;
    uv_async_t writeq_async;

    std::list<ofcore::PeerStatusListener*> peerStatusListeners;
    ofcore::PeerStatusListener::Health curHealth;

    void doRemovePeer(const std::string& hostname, int port);
    void doAddPeer(const std::string& hostname, int port);
    void doSetRoles(ConnData& cd, uint8_t newroles);
    void updateRole(ConnData& cd, uint8_t newroles,
                    ofcore::OFConstants::OpflexRole role);
    void connectionClosed(OpflexClientConnection* conn);
    void doConnectionClosed(OpflexClientConnection* conn);
    uv_loop_t* getLoop() { return client_loop; }
    void messagesReady();

    static void on_conn_async(uv_async_t *handle);
    static void on_cleanup_async(uv_async_t *handle);
    static void on_writeq_async(uv_async_t *handle);

    void updatePeerStatus(const std::string& hostname, int port,
                          ofcore::PeerStatusListener::PeerStatus status);

    friend class OpflexClientConnection;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXPOOL_H */
