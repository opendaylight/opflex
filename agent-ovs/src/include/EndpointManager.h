/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_ENDPOINTMANAGER_H
#define OVSAGENT_ENDPOINTMANAGER_H

#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>

namespace ovsagent {

/**
 * The endpoint manager is responsible for maintaining the state
 * related to endpoints.  It discovers new endpoints on the system and
 * write the appropriate objects and references into the endpoint
 * registry.  It also indexes endpoints in various useful ways and
 * exposes events related to endpoint updates that are useful for
 * compiling policy into local system configuration.
 */
class EndpointManager {
public:
    /**
     * Instantiate a new endpoint manager using the specified framework
     * instance.
     */
    EndpointManager(opflex::ofcore::OFFramework& framework_);

    /**
     * Destroy the endpoint manager and clean up all state
     */
    ~EndpointManager();

    /**
     * Start the endpoint manager
     */
    void start();

    /**
     * Stop the endpoint manager
     */
    void stop();

private:
    opflex::ofcore::OFFramework& framework;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINTMANAGER_H */
