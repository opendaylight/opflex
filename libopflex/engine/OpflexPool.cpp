/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexPool
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

#include <memory>
#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/logging/internal/logging.hpp"
#include "RecursiveLockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::make_pair;
using std::string;
using ofcore::OFConstants;
using ofcore::PeerStatusListener;
using yajr::transport::ZeroCopyOpenSSL;

OpflexPool::OpflexPool(HandlerFactory& factory_,
                       util::ThreadManager& threadManager_)
    : factory(factory_), threadManager(threadManager_),
      active(false),
      client_mode(OFConstants::OpflexElementMode::STITCHED_MODE),
      transport_state(OFConstants::OpflexTransportModeState::SEEKING_PROXIES),
      ipv4_proxy(0), ipv6_proxy(0),
      mac_proxy(0), curHealth(PeerStatusListener::DOWN)
{
    uv_mutex_init(&conn_mutex);
    uv_key_create(&conn_mutex_key);
}

OpflexPool::~OpflexPool() {
    uv_key_delete(&conn_mutex_key);
    uv_mutex_destroy(&conn_mutex);
}

boost::optional<std::string> OpflexPool::getLocation() {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    return location;
}

void OpflexPool::setLocation(const std::string& location) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    this->location = location;
}

void OpflexPool::enableSSL(const std::string& caStorePath,
                           const std::string& keyAndCertFilePath,
                           const std::string& passphrase,
                           bool verifyPeers) {
    OpflexConnection::initSSL();
    clientCtx.reset(ZeroCopyOpenSSL::Ctx::createCtx(caStorePath.c_str(),
                keyAndCertFilePath.c_str(),
                passphrase.c_str()));
    if (!clientCtx.get())
        throw std::runtime_error("Could not enable SSL");

    if (verifyPeers)
        clientCtx.get()->setVerify();
    else
        clientCtx.get()->setNoVerify();
}

void OpflexPool::enableSSL(const std::string& caStorePath,
                           bool verifyPeers) {
    OpflexConnection::initSSL();
    clientCtx.reset(ZeroCopyOpenSSL::Ctx::createCtx(caStorePath.c_str()));
    if (!clientCtx.get())
        throw std::runtime_error("Could not enable SSL");

    if (verifyPeers)
        clientCtx.get()->setVerify();
    else
        clientCtx.get()->setNoVerify();
}

void OpflexPool::on_conn_async(uv_async_t* handle) {
    OpflexPool* pool = (OpflexPool*)handle->data;
    if (pool->active) {
        util::RecursiveLockGuard guard(&pool->conn_mutex,
                                       &pool->conn_mutex_key);
        BOOST_FOREACH(conn_map_t::value_type& v, pool->connections) {
            v.second.conn->connect();
        }
    }
}

void OpflexPool::on_cleanup_async(uv_async_t* handle) {
    OpflexPool* pool = (OpflexPool*)handle->data;
    {
        util::RecursiveLockGuard guard(&pool->conn_mutex,
                                       &pool->conn_mutex_key);
        conn_map_t conns(pool->connections);
        BOOST_FOREACH(conn_map_t::value_type& v, conns) {
            v.second.conn->close();
        }
        if (!pool->connections.empty())
            return;
    }

    uv_close((uv_handle_t*)&pool->writeq_async, NULL);
    uv_close((uv_handle_t*)&pool->conn_async, NULL);
    uv_close((uv_handle_t*)handle, NULL);
    yajr::finiLoop(pool->client_loop);
}

void OpflexPool::on_writeq_async(uv_async_t* handle) {
    OpflexPool* pool = (OpflexPool*)handle->data;
    util::RecursiveLockGuard guard(&pool->conn_mutex,
                                   &pool->conn_mutex_key);
    BOOST_FOREACH(conn_map_t::value_type& v, pool->connections) {
        v.second.conn->processWriteQueue();
    }
}

void OpflexPool::start() {
    if (active) return;
    active = true;

    client_loop = threadManager.initTask("connection_pool");
    yajr::initLoop(client_loop);

    conn_async.data = this;
    cleanup_async.data = this;
    writeq_async.data = this;
    uv_async_init(client_loop, &conn_async, on_conn_async);
    uv_async_init(client_loop, &cleanup_async, on_cleanup_async);
    uv_async_init(client_loop, &writeq_async, on_writeq_async);

    threadManager.startTask("connection_pool");
}

void OpflexPool::stop() {
    if (!active) return;
    active = false;

    uv_async_send(&cleanup_async);
    threadManager.stopTask("connection_pool");
}

void OpflexPool::setOpflexIdentity(const std::string& name,
                                   const std::string& domain) {
    this->name = name;
    this->domain = domain;
}

void OpflexPool::setOpflexIdentity(const std::string& name,
                                   const std::string& domain,
                                   const std::string& location) {
    this->name = name;
    this->domain = domain;
    this->location = location;
}

void
OpflexPool::registerPeerStatusListener(PeerStatusListener* listener) {
    peerStatusListeners.push_back(listener);
}

void OpflexPool::updatePeerStatus(const std::string& hostname, int port,
                                  PeerStatusListener::PeerStatus status) {
    PeerStatusListener::Health newHealth = PeerStatusListener::DOWN;
    bool notifyHealth = false;
    bool hasReadyConnection = false;
    bool hasDegradedConnection = false;
    {
        util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
        BOOST_FOREACH(conn_map_t::value_type& v, connections) {
            if (v.second.conn->isReady())
                hasReadyConnection = true;
            else
                hasDegradedConnection = true;

        }

        if (hasReadyConnection) {
            newHealth = PeerStatusListener::HEALTHY;
            if (hasDegradedConnection)
                newHealth = PeerStatusListener::DEGRADED;
        }

        if (newHealth != curHealth) {
            notifyHealth = true;
            curHealth = newHealth;
        }
    }

    BOOST_FOREACH(PeerStatusListener* l, peerStatusListeners) {
        l->peerStatusUpdated(hostname, port, status);
    }

    if (notifyHealth) {
        LOG(DEBUG) << "Health updated to: "
                   << ((newHealth == PeerStatusListener::HEALTHY)
                       ? "HEALTHY"
                       : ((newHealth == PeerStatusListener::DEGRADED)
                          ? "DEGRADED" : "DOWN"));
        BOOST_FOREACH(PeerStatusListener* l, peerStatusListeners) {
            l->healthUpdated(newHealth);
        }
    }
}

OpflexClientConnection* OpflexPool::getPeer(const std::string& hostname,
                                            int port) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    conn_map_t::iterator it = connections.find(make_pair(hostname, port));
    if (it != connections.end()) {
        return it->second.conn;
    }
    return NULL;
}

void OpflexPool::addPeer(const std::string& hostname, int port,
                         bool configured) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    if (configured)
        configured_peers.insert(make_pair(hostname, port));
    doAddPeer(hostname, port);
    uv_async_send(&conn_async);
}

void OpflexPool::doAddPeer(const std::string& hostname, int port) {
    if (!active) return;
    ConnData& cd = connections[make_pair(hostname, port)];
    if (cd.conn != NULL) {
        LOG(DEBUG) << "Connection for "
                   << hostname << ":" << port
                   << " already exists; not adding peer.";
    } else {
        LOG(INFO) << "Adding peer "
                  << hostname << ":" << port;

        OpflexClientConnection* conn =
            new OpflexClientConnection(factory, this, hostname, port);
        cd.conn = conn;
    }
}

void OpflexPool::addPeer(OpflexClientConnection* conn) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    ConnData& cd = connections[make_pair(conn->getHostname(), conn->getPort())];
    if (cd.conn != NULL) {
        LOG(ERROR) << "Connection for "
                   << conn->getHostname() << ":" << conn->getPort()
                   << " already exists";
    }
    cd.conn = conn;
}

void OpflexPool::doRemovePeer(const std::string& hostname, int port) {
    conn_map_t::iterator it = connections.find(make_pair(hostname, port));
    if (it != connections.end()) {
        if (it->second.conn) {
            doSetRoles(it->second, 0);
        }
        connections.erase(it);
    }
}

void OpflexPool::resetAllPeers() {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    BOOST_FOREACH(conn_map_t::value_type& v, connections) {
        v.second.conn->close();
    }
}

// must be called with conn_mutex held
void OpflexPool::updateRole(ConnData& cd,
                            uint8_t newroles,
                            OFConstants::OpflexRole role) {
    if (cd.roles & role) {
        if (!(newroles & role)) {
            role_map_t::iterator it = roles.find(role);
            if (it != roles.end()) {
                if (it->second.curMaster == cd.conn)
                    it->second.curMaster = NULL;
                it->second.conns.erase(cd.conn);
                if (it->second.conns.empty())
                    roles.erase(it);
            }
            cd.roles &= ~role;
        }
    } else if (newroles & role) {
        if (!(cd.roles & role)) {
            conn_set_t& cl = roles[role].conns;
            cl.insert(cd.conn);
            cd.roles |= role;
        }
    }
}

size_t OpflexPool::getRoleCount(ofcore::OFConstants::OpflexRole role) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);

    auto it = roles.find(role);
    if (it == roles.end()) return 0;
    return it->second.conns.size();
}


void OpflexPool::setRoles(OpflexClientConnection* conn,
                          uint8_t newroles) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    ConnData& cd = connections.at(make_pair(conn->getHostname(),
                                            conn->getPort()));
    doSetRoles(cd, newroles);
    cd.conn->setRoles(newroles);
}

// must be called with conn_mutex held
void OpflexPool::doSetRoles(ConnData& cd, uint8_t newroles) {
    updateRole(cd, newroles, OFConstants::POLICY_ELEMENT);
    updateRole(cd, newroles, OFConstants::POLICY_REPOSITORY);
    updateRole(cd, newroles, OFConstants::ENDPOINT_REGISTRY);
    updateRole(cd, newroles, OFConstants::OBSERVER);
}

OpflexClientConnection*
OpflexPool::getMasterForRole(OFConstants::OpflexRole role) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);

    role_map_t::iterator it = roles.find(role);
    if (it == roles.end())
        return NULL;
    if (it->second.curMaster != NULL && it->second.curMaster->isReady())
        return it->second.curMaster;
    BOOST_FOREACH(OpflexClientConnection* conn, it->second.conns) {
        if (conn->isReady()) {
            it->second.curMaster = conn;
            return conn;
        }
    }
    return NULL;
}

void OpflexPool::connectionClosed(OpflexClientConnection* conn) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);

    doConnectionClosed(conn);

    guard.release();
    if (!active)
        uv_async_send(&cleanup_async);
}

void OpflexPool::doConnectionClosed(OpflexClientConnection* conn) {
    doRemovePeer(conn->getHostname(), conn->getPort());
    delete conn;
}

void OpflexPool::messagesReady() {
    uv_async_send(&writeq_async);
}

size_t OpflexPool::sendToRole(OpflexMessage* message,
                           OFConstants::OpflexRole role,
                           bool sync) {
#ifdef HAVE_CXX11
    std::unique_ptr<OpflexMessage> messagep(message);
#else
    std::auto_ptr<OpflexMessage> messagep(message);
#endif

    if (!active) return 0;
    std::vector<OpflexClientConnection*> conns;

    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    role_map_t::iterator it = roles.find(role);
    if (it == roles.end())
        return 0;

    size_t i = 0;
    OpflexMessage* m_copy = NULL;
    std::vector<OpflexClientConnection*> ready;
    BOOST_FOREACH(OpflexClientConnection* conn, it->second.conns) {
        if (!conn->isReady()) continue;
        ready.push_back(conn);
    }
    BOOST_FOREACH(OpflexClientConnection* conn, ready) {
        if (i < (ready.size() - 1)) {
            m_copy = message->clone();
        } else {
            m_copy = message;
            messagep.release();
        }
        conn->sendMessage(m_copy, sync);
        i += 1;
    }
    // all allocated buffers should have been dispatched to
    // connections

    return i;
}

void OpflexPool::validatePeerSet(OpflexClientConnection * conn, const peer_name_set_t& peers) {
    peer_name_set_t to_remove;
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);

    BOOST_FOREACH(const conn_map_t::value_type& cv, connections) {
        OpflexClientConnection* c = cv.second.conn;
        OpflexClientConnection* srcPeer = getPeer(conn->getHostname(), conn->getPort());
        peer_name_t peer_name = make_pair(c->getHostname(), c->getPort());
        if ((peers.find(peer_name) == peers.end()) &&
            (configured_peers.find(peer_name) == configured_peers.end()) &&
            ((srcPeer->getRoles()==0) || (getClientMode() != AgentMode::TRANSPORT_MODE))) {
            LOG(INFO) << "Removing stale peer connection: "
                      << peer_name.first << ":" << peer_name.second
                      << " based on peer-list from "
                      << srcPeer->getHostname() << ":" << srcPeer->getPort();
            c->close();
        }
    }
}

bool OpflexPool::isConfiguredPeer(const std::string& hostname, int port) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    return configured_peers.find(make_pair(hostname, port)) !=
        configured_peers.end();
}

void OpflexPool::addConfiguredPeers() {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    BOOST_FOREACH(const peer_name_t& peer_name, configured_peers) {
        addPeer(peer_name.first, peer_name.second, false);
    }
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
