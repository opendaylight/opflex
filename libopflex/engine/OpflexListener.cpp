/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexListener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexPool.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;

OpflexListener::OpflexListener(HandlerFactory& handlerFactory_,
                               const std::string& hostname_, 
                               int port_,
                               const std::string& name_,
                               const std::string& domain_)
    : handlerFactory(handlerFactory_),
      hostname(hostname_), port(port_), 
      name(name_), domain(domain_) {

}

OpflexListener::~OpflexListener() {
    disconnect();
}

void OpflexListener::disconnect() {
    // XXX TODO
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
