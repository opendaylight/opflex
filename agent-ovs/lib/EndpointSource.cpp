/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for EndpointSource class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/EndpointSource.h>
#include <opflexagent/EndpointManager.h>

namespace opflexagent {

EndpointSource::EndpointSource(EndpointManager* manager_)
    : manager(manager_) {}

EndpointSource::~EndpointSource() {}

void EndpointSource::updateEndpoint(const Endpoint& endpoint) {
    manager->updateEndpoint(endpoint);
}

void EndpointSource::removeEndpoint(const std::string& uuid) {
    manager->removeEndpoint(uuid);
}

} /* namespace opflexagent */
