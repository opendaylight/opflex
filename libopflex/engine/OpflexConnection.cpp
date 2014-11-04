/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"

namespace opflex {
namespace engine {
namespace internal {

OpflexConnection::OpflexConnection(HandlerFactory& handlerFactory)
    : handler(handlerFactory.newHandler(this)) {
    connect();
}

OpflexConnection::~OpflexConnection() {
    disconnect();
    if (handler)
        delete handler;
}

void OpflexConnection::connect() {}

void OpflexConnection::disconnect() {}

bool OpflexConnection::isReady() { 
    return handler->isReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
