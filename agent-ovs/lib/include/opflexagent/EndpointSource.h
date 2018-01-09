/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint source
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Endpoint.h>

#pragma once
#ifndef OPFLEXAGENT_ENDPOINTSOURCE_H
#define OPFLEXAGENT_ENDPOINTSOURCE_H

namespace opflexagent {

class EndpointManager;

/**
 * An abstract base class for a source of endpoint data.  A component
 * that discovers endpoints on the system can implement this interface
 * to inform the endpoint manager when endpoints are created or
 * modified.
 */
class EndpointSource {
public:
    /**
     * Instantiate a new endpoint source using the endpoint manager
     * specified
     */
    EndpointSource(EndpointManager* manager);

    /**
     * Destroy the endpoint source and clean up all state
     */
    virtual ~EndpointSource();

    /**
     * Add or update the specified endpoint in the endpoint manager.
     *
     * @param endpoint the endpoint to add/update
     */
    virtual void updateEndpoint(const Endpoint& endpoint);

    /**
     * Remove an endpoint that no longer exists from the endpoint
     * manager
     *
     * @param uuid the endpoint that no longer exists
     */
    virtual void removeEndpoint(const std::string& uuid);

protected:
    /**
     * The endpoint manager that will be updated
     */
    EndpointManager* manager;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ENDPOINTSOURCE_H */
