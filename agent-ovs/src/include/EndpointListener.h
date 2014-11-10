/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint listener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/modb/URI.h>

#pragma once
#ifndef OVSAGENT_ENDPOINTLISTENER_H
#define OVSAGENT_ENDPOINTLISTENER_H

namespace ovsagent {

/**
 * An abstract interface for classes interested in updates related to
 * the endpoints
 */
class EndpointListener {
public:
    /**
     * Instantiate a new endpoint listener
     */
    EndpointListener() {};

    /**
     * Destroy the endpoint listener and clean up all state
     */
    virtual ~EndpointListener() {};

    /**
     * Called when an endpoint is added, updated, or removed.
     *
     * @param uuid the UUID for the endpoint
     */
    virtual void endpointUpdated(const std::string& uuid) = 0;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINTMANAGER_H */
