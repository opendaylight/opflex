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

#include <memory>
#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::make_pair;
using std::string;
using ofcore::OFConstants;

OpflexPool::OpflexPool(HandlerFactory& factory_)
    : factory(factory_), active(false) {
    uv_mutex_init(&conn_mutex);
}

OpflexPool::~OpflexPool() {
    uv_mutex_destroy(&conn_mutex);
}

void OpflexPool::on_conn_async(uv_async_t* handle) {
    OpflexPool* pool = (OpflexPool*)handle->data;
    if (pool->active) {
        util::LockGuard guard(&pool->conn_mutex);
        BOOST_FOREACH(conn_map_t::value_type& v, pool->connections) {
            v.second.conn->connect();
        }
    }
}

void OpflexPool::on_cleanup_async(uv_async_t* handle) {
    OpflexPool* pool = (OpflexPool*)handle->data;
    {
        std::vector<OpflexConnection*> conns;
        {
            util::LockGuard guard(&pool->conn_mutex);
            BOOST_FOREACH(conn_map_t::value_type& v, pool->connections) {
                conns.push_back(v.second.conn);
            }
        }
        BOOST_FOREACH(OpflexConnection* conn, conns) {
            conn->close();
        }
        if (conns.size() > 0)
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
    util::LockGuard guard(&pool->conn_mutex);
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

OpflexClientConnection* OpflexPool::getPeer(const std::string& hostname, 
                                            int port) {
    util::LockGuard guard(&conn_mutex);
    conn_map_t::iterator it = connections.find(make_pair(hostname, port));
    if (it != connections.end()) {
        return it->second.conn;
    }
    return NULL;
}

void OpflexPool::addPeer(const std::string& hostname, int port) {
    util::LockGuard guard(&conn_mutex);
    doAddPeer(hostname, port);
    uv_async_send(&conn_async);
}

void OpflexPool::doAddPeer(const std::string& hostname, int port) {
    if (!active) return;
    ConnData& cd = connections[make_pair(hostname, port)];
    if (cd.conn != NULL) {
        LOG(ERROR) << "Connection for "
                   << hostname << ":" << port
                   << " already exists; not adding peer.";
    } else {
        OpflexClientConnection* conn = 
            new OpflexClientConnection(factory, this, hostname, port);
        cd.conn = conn;
    }
}

void OpflexPool::addPeer(OpflexClientConnection* conn) {
    util::LockGuard guard(&conn_mutex);
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
   util::LockGuard guard(&conn_mutex);

   role_map_t::iterator it = roles.find(role);
   if (it == roles.end()) return 0;
   return it->second.conns.size();
}


void OpflexPool::setRoles(OpflexClientConnection* conn,
                          uint8_t newroles) {
    util::LockGuard guard(&conn_mutex);
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
    util::LockGuard guard(&conn_mutex);
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
    if (conn != NULL) {
        util::LockGuard guard(&conn_mutex);
        doRemovePeer(conn->getHostname(), conn->getPort());
        delete conn;
    }
    if (!active)
        uv_async_send(&cleanup_async);
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
        util::LockGuard guard(&conn_mutex);
        role_map_t::iterator it = roles.find(role);
        if (it == roles.end())
            return;
        
        int i = 0;
        OpflexMessage* m_copy = NULL;
        BOOST_FOREACH(OpflexClientConnection* conn, it->second.conns) {
            if (!conn->isReady()) continue;

            if (i + 1 < conns.size()) {
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

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
