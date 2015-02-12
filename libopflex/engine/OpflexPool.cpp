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
#ifndef SIMPLE_RPC
using yajr::transport::ZeroCopyOpenSSL;
#endif

OpflexPool::OpflexPool(HandlerFactory& factory_)
    : factory(factory_), active(false), curHealth(PeerStatusListener::DOWN) {
    uv_mutex_init(&conn_mutex);
    uv_key_create(&conn_mutex_key);
}

OpflexPool::~OpflexPool() {
    uv_key_delete(&conn_mutex_key);
    uv_mutex_destroy(&conn_mutex);
}

void OpflexPool::enableSSL(const std::string& caStorePath,
                           bool verifyPeers) {
#ifndef SIMPLE_RPC
    OpflexConnection::initSSL();
    clientCtx.reset(ZeroCopyOpenSSL::Ctx::createCtx(caStorePath.c_str(), NULL));
    if (!clientCtx.get())
        throw std::runtime_error("Could not enable SSL");
#endif
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
        if (pool->connections.size() > 0)
            return;
    }

    uv_close((uv_handle_t*)&pool->writeq_async, NULL);
    uv_close((uv_handle_t*)&pool->conn_async, NULL);
    uv_close((uv_handle_t*)handle, NULL);
#ifdef SIMPLE_RPC
    uv_timer_stop(&pool->timer);
    uv_close((uv_handle_t*)&pool->timer, NULL);
#else
    yajr::finiLoop(&pool->client_loop);
#endif
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

    uv_loop_init(&client_loop);
#ifdef SIMPLE_RPC
    timer.data = this;
    uv_timer_init(&client_loop, &timer);
    uv_timer_start(&timer, on_timer, 5000, 5000);
#else
    yajr::initLoop(&client_loop);
#endif

    conn_async.data = this;
    cleanup_async.data = this;
    writeq_async.data = this;
    uv_async_init(&client_loop, &conn_async, on_conn_async);
    uv_async_init(&client_loop, &cleanup_async, on_cleanup_async);
    uv_async_init(&client_loop, &writeq_async, on_writeq_async);

    int rc = uv_thread_create(&client_thread, client_thread_func, this);
    if (rc < 0) {
        throw std::runtime_error(string("Could not create client thread: ") +
                                 uv_strerror(rc));
    }
}

void OpflexPool::stop() {
    if (!active) return;
    active = false;

    uv_async_send(&cleanup_async);
    uv_thread_join(&client_thread);
    uv_loop_close(&client_loop);
}

void OpflexPool::setOpflexIdentity(const std::string& name,
                                   const std::string& domain) {
    this->name = name;
    this->domain = domain;
}

void
OpflexPool::registerPeerStatusListener(PeerStatusListener* listener) {
    peerStatusListeners.push_back(listener);
}

void OpflexPool::updatePeerStatus(const std::string& hostname, int port,
                                  PeerStatusListener::PeerStatus status) {
    PeerStatusListener::Health newHealth = PeerStatusListener::HEALTHY;
    bool notifyHealth = false;
    bool hasConnection = false;
    {
        util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
        BOOST_FOREACH(conn_map_t::value_type& v, connections) {
            hasConnection = true;
            if (!v.second.conn->isReady()) {
                switch (newHealth) {
                case PeerStatusListener::HEALTHY:
                    newHealth = PeerStatusListener::DEGRADED;
                    break;
                case PeerStatusListener::DEGRADED:
                case PeerStatusListener::DOWN:
                default:
                    newHealth = PeerStatusListener::DOWN;
                    break;
                }
            }
        }
        if (!hasConnection)
            newHealth = PeerStatusListener::DOWN;
        if (newHealth != curHealth) {
            notifyHealth = true;
            curHealth = newHealth;
        }
    }

    BOOST_FOREACH(PeerStatusListener* l, peerStatusListeners) {
        l->peerStatusUpdated(hostname, port, status);
    }

    if (notifyHealth) {
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
                if (it->second.conns.size() == 0)
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

int OpflexPool::getRoleCount(ofcore::OFConstants::OpflexRole role) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);

    role_map_t::iterator it = roles.find(role);
    if (it == roles.end()) return 0;
    return it->second.conns.size();
}


void OpflexPool::setRoles(OpflexClientConnection* conn,
                          uint8_t newroles) {
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
    ConnData& cd = connections.at(make_pair(conn->getHostname(),
                                            conn->getPort()));
    doSetRoles(cd, newroles);
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

void OpflexPool::client_thread_func(void* pool_) {
    OpflexPool* pool = (OpflexPool*)pool_;
    uv_run(&pool->client_loop, UV_RUN_DEFAULT);
}

#ifdef SIMPLE_RPC
void OpflexPool::on_conn_closed(OpflexClientConnection* conn) {
    std::string host = conn->getHostname();
    int port = conn->getPort();
    bool retry = conn->shouldRetry();
    OpflexPool* pool = conn->getPool();
    conn->getPool()->connectionClosed(conn);
    if (retry && pool->active)
        pool->doAddPeer(host, port);
}

void OpflexPool::on_timer(uv_timer_t* timer) {
    OpflexPool* pool = (OpflexPool*)timer->data;
    on_conn_async(&pool->conn_async);
}
#endif

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

void OpflexPool::sendToRole(OpflexMessage* message,
                            OFConstants::OpflexRole role,
                            bool sync) {
    std::auto_ptr<OpflexMessage> messagep(message);
    if (!active) return;
    std::vector<OpflexClientConnection*> conns;
    {
        util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);
        role_map_t::iterator it = roles.find(role);
        if (it == roles.end())
            return;
        
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
    }
    // all allocated buffers should have been dispatched to
    // connections
}

void OpflexPool::validatePeerSet(const peer_name_set_t& peers) {
    peer_name_set_t to_remove;
    util::RecursiveLockGuard guard(&conn_mutex, &conn_mutex_key);

    BOOST_FOREACH(const conn_map_t::value_type& cv, connections) {
        OpflexClientConnection* c = cv.second.conn;
        peer_name_t peer_name = make_pair(c->getHostname(), c->getPort());
        if (peers.find(peer_name) == peers.end() &&
            configured_peers.find(peer_name) == configured_peers.end()) {
            LOG(INFO) << "Removing stale peer connection: "
                      << peer_name.first << ":" << peer_name.second;
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
