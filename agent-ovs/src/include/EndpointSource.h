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

#pragma once
#ifndef OVSAGENT_ENDPOINTSOURCE_H
#define OVSAGENT_ENDPOINTSOURCE_H

namespace ovsagent {

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
     * Update the endpoint manager with the endpoint information
     * specified.
     */
    virtual void updateEndpoint();

    /**
     * Start the endpoint source
     */
    virtual void start() = 0;

    /**
     * Stop the endpoint source
     */
    virtual void stop() = 0;

protected:
    /**
     * The endpoint manager that will be updated
     */
    EndpointManager* manager;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINTSOURCE_H */
