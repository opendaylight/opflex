/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexClientConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexClientConnection.h"
#include "opflex/engine/internal/OpflexPool.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;

OpflexClientConnection::OpflexClientConnection(HandlerFactory& handlerFactory, 
                                               OpflexPool* pool_,
                                               const string& hostname_, 
                                               int port_)
    : OpflexConnection(handlerFactory),
      pool(pool_), hostname(hostname_), port(port_) {

}

OpflexClientConnection::~OpflexClientConnection() {

}

const std::string& OpflexClientConnection::getName() {
    return pool->getName();
}

const std::string& OpflexClientConnection::getDomain() {
    return pool->getDomain();
}

void OpflexClientConnection::disconnect() {
    // XXX TODO
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
