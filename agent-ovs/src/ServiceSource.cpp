/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ServiceSource class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ServiceSource.h"
#include "ServiceManager.h"

namespace ovsagent {

ServiceSource::ServiceSource(ServiceManager* manager_)
    : manager(manager_) {}

ServiceSource::~ServiceSource() {}

void ServiceSource::updateAnycastService(const AnycastService& service) {
    manager->updateAnycastService(service);
}

void ServiceSource::removeAnycastService(const std::string& uuid) {
    manager->removeAnycastService(uuid);
}

} /* namespace ovsagent */
