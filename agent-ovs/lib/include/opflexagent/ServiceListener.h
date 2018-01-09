/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for service listener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SERVICELISTENER_H
#define OPFLEXAGENT_SERVICELISTENER_H

namespace opflexagent {

/**
 * An abstract interface for classes interested in updates related to
 * the services
 */
class ServiceListener {
public:
    /**
     * Instantiate a new service listener
     */
    ServiceListener() {};

    /**
     * Destroy the service listener and clean up all state
     */
    virtual ~ServiceListener() {};

    /**
     * Called when a service is added, updated, or removed.
     *
     * @param uuid the UUID for the service
     */
    virtual void serviceUpdated(const std::string& uuid) = 0;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SERVICELISTENER_H */
