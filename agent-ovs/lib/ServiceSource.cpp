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

#include <opflexagent/ServiceSource.h>
#include <opflexagent/ServiceManager.h>

namespace opflexagent {

ServiceSource::ServiceSource(ServiceManager* manager_)
    : manager(manager_) {}

ServiceSource::~ServiceSource() {}

void ServiceSource::updateService(const Service& service) {
    manager->updateService(service);
}

void ServiceSource::removeService(const std::string& uuid) {
    manager->removeService(uuid);
}

} /* namespace opflexagent */
