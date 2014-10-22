/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for libvirt endpoint source
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>

#include <libvirt/libvirt.h>

#include "EndpointSource.h"

#pragma once
#ifndef OVSAGENT_VIRTENDPOINTSOURCE_H
#define OVSAGENT_VIRTENDPOINTSOURCE_H

namespace ovsagent {

/**
 * An endpoint source that gets information about endpoints from
 * libvirt
 */
class VirtEndpointSource : public EndpointSource {
public:
    /**
     * Instantiate a new libvirt endpoint source using the specified
     * hypervisor name and endpoint manager
     */
    VirtEndpointSource(EndpointManager* manager,
                       const std::string& hypervisorName);

    /**
     * Destroy the endpoint source and clean up all state
     */
    virtual ~VirtEndpointSource();

    /**
     * Get the hypervisor name associated with this libvirt endpoint
     * source.
     */
    const std::string& getHypervisorName() { return hypervisorName; }

    // see EndpointSource
    virtual void start();

    // see EndpointSource
    virtual void stop();
private:

    std::string hypervisorName;
    virConnectPtr virtConn;

};

} /* namespace ovsagent */

#endif /* OVSAGENT_VIRTENDPOINTSOURCE_H */
