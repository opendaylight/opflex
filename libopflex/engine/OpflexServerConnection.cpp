/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexServerConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexServerConnection.h"
#include "opflex/engine/internal/OpflexListener.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;

OpflexServerConnection::OpflexServerConnection(HandlerFactory& handlerFactory, 
                                               OpflexListener* listener_,
                                               const string& hostname_, 
                                               int port_)
    : OpflexConnection(handlerFactory),
      listener(listener_), hostname(hostname_), port(port_) {

}

OpflexServerConnection::~OpflexServerConnection() {

}

const std::string& OpflexServerConnection::getName() {
    return listener->getName();
}

const std::string& OpflexServerConnection::getDomain() {
    return listener->getDomain();
}

void OpflexServerConnection::disconnect() {
    // XXX TODO
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
