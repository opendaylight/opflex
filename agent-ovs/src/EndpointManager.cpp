/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for EndpointManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "EndpointManager.h"

namespace ovsagent {

    EndpointManager::EndpointManager(opflex::ofcore::OFFramework& framework_,
                                     PolicyManager& policyManager_)
        : framework(framework_), policyManager(policyManager_) {

}

EndpointManager::~EndpointManager() {

}

void EndpointManager::start() {

}

void EndpointManager::stop() {

}

} /* namespace ovsagent */
