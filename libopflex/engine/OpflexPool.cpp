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

OpflexPool::OpflexPool(HandlerFactory& factory_)
    : factory(factory_) {
    uv_mutex_init(&conn_mutex);

}

OpflexPool::~OpflexPool() {
    clear();
    uv_mutex_destroy(&conn_mutex);
}

void OpflexPool::setOpflexIdentity(const std::string& name,
                                   const std::string& domain) {
    this->name = name;
    this->domain = domain;
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
}

void OpflexPool::addPeer(OpflexClientConnection* conn) {
    util::LockGuard guard(&conn_mutex);
    ConnData& cd = connections[make_pair(conn->getHostname(), conn->getPort())];
    if (cd.conn != NULL) {
        LOG(ERROR) << "Connection for "
                   << conn->getHostname() << ":" << conn->getPort()
                   << " already exists; removing old peer";
        doRemovePeer(conn->getHostname(), conn->getPort());
    }
    cd.conn = conn;
}

void OpflexPool::removePeer(const std::string& hostname, int port) {
    util::LockGuard guard(&conn_mutex);
    doRemovePeer(hostname, port);
}

void OpflexPool::doRemovePeer(const std::string& hostname, int port) {
    conn_map_t::iterator it = connections.find(make_pair(hostname, port));
    if (it != connections.end()) {
        if (it->second.conn) {
            doSetRoles(it->second, 0);
            delete it->second.conn;
        }
        connections.erase(it);
    }
}

void OpflexPool::clear() {
    util::LockGuard guard(&conn_mutex);
    roles.clear();
    connections.clear();
}

// must be called with conn_mutex held
void OpflexPool::updateRole(ConnData& cd,
                            uint8_t newroles,
                            OpflexHandler::OpflexRole role) {
    if (cd.roles & role) {
        if (!(newroles & role)) {
            role_map_t::iterator it = roles.find(role);
            if (it != roles.end()) {
                it->second.erase(cd.conn);
                if (it->second.size() == 0)
                    roles.erase(it);
            }
        }
    } else if (newroles & role) {
        if (!(cd.roles & role)) {
            conn_set_t& cl = roles[role];
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
    role_map_t::const_iterator it = roles.find(role);
    if (it == roles.end())
        return NULL;
    BOOST_FOREACH(OpflexClientConnection* conn, it->second) {
        if (conn->isReady()) return conn;
    }
    return NULL;
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
