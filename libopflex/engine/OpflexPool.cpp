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

static void async_cb(uv_async_t* handle) {
    uv_close((uv_handle_t*)handle, NULL);
}

OpflexPool::OpflexPool(HandlerFactory& factory_)
    : factory(factory_) {
    uv_mutex_init(&conn_mutex);
    uv_loop_init(&client_loop);
    uv_timer_init(&client_loop, &timer);
    timer.data = this;
    uv_timer_start(&timer, timer_cb, 1000, 1000);
    uv_async_init(&client_loop, &async, async_cb);

    int rc = uv_thread_create(&client_thread, client_thread_func, this);
    if (rc < 0) {
        throw std::runtime_error(string("Could not create client thread: ") +
                                 uv_strerror(rc));
    }
}

OpflexPool::~OpflexPool() {
    LOG(INFO) << "Shutting down Opflex client";
    clear();
    uv_timer_stop(&timer);
    uv_close((uv_handle_t*)&timer, NULL);
    uv_async_send(&async);
    uv_thread_join(&client_thread);
    int rc = uv_loop_close(&client_loop);
    uv_mutex_destroy(&conn_mutex);
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
        conn->connect();
    }
}

void OpflexPool::addPeer(OpflexClientConnection* conn) {
    util::LockGuard guard(&conn_mutex);
    ConnData& cd = connections[make_pair(conn->getHostname(), conn->getPort())];
    if (cd.conn != NULL) {
        LOG(ERROR) << "Connection for "
                   << conn->getHostname() << ":" << conn->getPort()
                   << " already exists";
        conn->disconnect();
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

void OpflexPool::clear() {
    util::LockGuard guard(&conn_mutex);
    BOOST_FOREACH(conn_map_t::value_type& v, connections) {
        v.second.conn->disconnect();
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

void OpflexPool::timer_cb(uv_timer_t* handle) {
    // TODO retry connections, read timeouts, etc
}

void OpflexPool::on_conn_closed(uv_handle_t *handle) {
    OpflexClientConnection* conn = (OpflexClientConnection*)handle->data;
    OpflexPool* pool = conn->getPool();
    if (conn != NULL) {
        util::LockGuard guard(&pool->conn_mutex);
        pool->doRemovePeer(conn->getHostname(), conn->getPort());
        delete conn;
    }
}

void OpflexPool::writeToRole(OpflexMessage& message,
                             OpflexHandler::OpflexRole role) {
    std::vector<OpflexClientConnection*> conns;
    {
        util::LockGuard guard(&conn_mutex);
        role_map_t::iterator it = roles.find(role);
        if (it == roles.end())
            return;
        
        BOOST_FOREACH(OpflexClientConnection* conn, it->second.conns) {
            conns.push_back(conn);
        }
    }

    rapidjson::StringBuffer* sb = NULL;
    rapidjson::StringBuffer* sb_copy = NULL;
    for (int i = 0; i < conns.size(); ++i) {
        // only call serialize once
        if (sb == NULL) sb = message.serialize();
        // make a copy for all but the last connection
        if (i + 1 < conns.size()) {
            sb_copy = new rapidjson::StringBuffer();
            // sadly this appears to be the only way to copy a
            // rapidjson string buffer.  Revisit use of string buffer
            // later
            const char* str = sb->GetString();
            for (int j = 0; j < sb->GetSize(); ++j)
                sb_copy->Put(str[j]);
        } else {
            sb_copy = sb;
        }

        conns[i]->write(sb_copy);
    }
    // all allocated buffers should have been dispatched to
    // connections
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
