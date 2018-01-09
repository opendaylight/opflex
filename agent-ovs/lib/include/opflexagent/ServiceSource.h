/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for service source
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Service.h>

#pragma once
#ifndef OPFLEXAGENT_SERVICESOURCE_H
#define OPFLEXAGENT_SERVICESOURCE_H

namespace opflexagent {

class ServiceManager;

/**
 * An abstract base class for a source of service data.  A component
 * that discovers services on the system can implement this interface
 * to inform the service manager when services are created or
 * modified.
 */
class ServiceSource {
public:
    /**
     * Instantiate a new service source using the service manager
     * specified
     */
    ServiceSource(ServiceManager* manager);

    /**
     * Destroy the service source and clean up all state
     */
    virtual ~ServiceSource();

    /**
     * Add or update the specified service in the service manager.
     *
     * @param service the service to add/update
     */
    virtual void updateService(const Service& service);

    /**
     * Remove an service that no longer exists from the service
     * manager
     *
     * @param uuid the service that no longer exists
     */
    virtual void removeService(const std::string& uuid);

protected:
    /**
     * The service manager that will be updated
     */
    ServiceManager* manager;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SERVICESOURCE_H */
