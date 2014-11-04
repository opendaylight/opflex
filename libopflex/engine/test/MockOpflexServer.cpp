/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MockOpflexServer
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "MockOpflexServer.h"
#include "MockServerHandler.h"

namespace opflex {
namespace engine {
namespace internal {

MockOpflexServer::MockOpflexServer(int port_, uint8_t roles_) 
    : port(port_), roles(roles_), 
      listener(*this, "127.0.0.1", 8009, "name", "domain") {

}

MockOpflexServer::~MockOpflexServer() {

}

OpflexHandler* MockOpflexServer::newHandler(OpflexConnection* conn) {
    return new MockServerHandler(conn, this);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
