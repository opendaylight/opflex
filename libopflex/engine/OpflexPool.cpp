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

#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::make_pair;
using std::string;

OpflexPool::OpflexPool(HandlerFactory& factory_)
    : factory(factory_), active(false) {
    uv_mutex_init(&conn_mutex);
}

OpflexPool::~OpflexPool() {
    uv_mutex_destroy(&conn_mutex);
}

void OpflexPool::on_conn_async(uv_async_t* handle) {
    OpflexPool* pool = (OpflexPool*)handle->data;
    if (!pool->active) {
        util::LockGuard guard(&pool->conn_mutex);
        BOOST_FOREACH(conn_map_t::value_type& v, pool->connections) {
            v.second.conn->disconnect();
        }
        
        uv_close((uv_handle_t*)&pool->writeq_async, NULL);
        uv_close((uv_handle_t*)handle, NULL);
#ifndef SIMPLE_RPC
        yajr::finiLoop(&pool->client_loop);
#endif
    } else {
        util::LockGuard guard(&pool->conn_mutex);
        BOOST_FOREACH(conn_map_t::value_type& v, pool->connections) {
            v.second.conn->connect();
        }
    }
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
#ifndef SIMPLE_RPC
    yajr::initLoop(&client_loop);
#endif

    conn_async.data = this;
    writeq_async.data = this;
    uv_async_init(&client_loop, &conn_async, on_conn_async);
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

    uv_async_send(&conn_async);
    uv_thread_join(&client_thread);
    int rc = uv_loop_close(&client_loop);
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
    guard.release();
    uv_async_send(&conn_async);
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
                            OpflexHandler::OpflexRole role) {
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
        }
    } else if (newroles & role) {
        if (!(cd.roles & role)) {
            conn_set_t& cl = roles[role].conns;
            cl.insert(cd.conn);
        }
    }
}

void OpflexPool::setRoles(OpflexClientConnection* conn,
                          uint8_t newroles) {
    util::LockGuard guard(&conn_mutex);
    ConnData& cd = connections[make_pair(conn->getHostname(), conn->getPort())];
    doSetRoles(cd, newroles);
}

// must be called with conn_mutex held
void OpflexPool::doSetRoles(ConnData& cd, uint8_t newroles) {
    updateRole(cd, newroles, OpflexHandler::POLICY_ELEMENT);
    updateRole(cd, newroles, OpflexHandler::POLICY_REPOSITORY);
    updateRole(cd, newroles, OpflexHandler::ENDPOINT_REGISTRY);
    updateRole(cd, newroles, OpflexHandler::OBSERVER);
}

OpflexClientConnection*
OpflexPool::getMasterForRole(OpflexHandler::OpflexRole role) {
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
void OpflexPool::on_conn_closed(uv_handle_t *handle) {
    OpflexClientConnection* conn = (OpflexClientConnection*)handle->data;
    conn->getPool()->connectionClosed(conn);
}
#endif

void OpflexPool::connectionClosed(OpflexClientConnection* conn) {
    if (conn != NULL) {
        util::LockGuard guard(&conn_mutex);
        doRemovePeer(conn->getHostname(), conn->getPort());
        delete conn;
    }
}

void OpflexPool::messagesReady() {
    uv_async_send(&writeq_async);
}

void OpflexPool::sendToRole(OpflexMessage* message,
                            OpflexHandler::OpflexRole role,
                            bool sync) {
    std::vector<OpflexClientConnection*> conns;
    {
        util::LockGuard guard(&conn_mutex);
        role_map_t::iterator it = roles.find(role);
        if (it == roles.end())
            return;
        
        BOOST_FOREACH(OpflexClientConnection* conn, it->second.conns) {
            conns.push_back(conn);
        }

        OpflexMessage* m_copy = NULL;
        for (int i = 0; i < conns.size(); ++i) {
            if (i + 1 < conns.size()) {
                m_copy = message->clone();
            } else {
                m_copy = message;
            }
            
            conns[i]->sendMessage(m_copy, sync);
        }
    }
    // all allocated buffers should have been dispatched to
    // connections
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
