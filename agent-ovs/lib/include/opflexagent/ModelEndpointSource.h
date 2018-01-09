/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for model endpoint source
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_MODELENDPOINTSOURCE_H
#define OPFLEXAGENT_MODELENDPOINTSOURCE_H

#include <opflexagent/EndpointSource.h>

#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>

#include <set>
#include <string>

namespace opflexagent {

/**
 * An endpoint source that gets information about endpoints from the
 * OpFlex endpoint inventory
 */
class ModelEndpointSource :
        public EndpointSource,
        public opflex::modb::ObjectListener,
        private boost::noncopyable {
public:
    /**
     * Instantiate a new endpoint source using the specified endpoint
     * manager.  It will resolve the endpoint inventory with the given
     * name
     */
    ModelEndpointSource(EndpointManager* manager,
                        opflex::ofcore::OFFramework& framework,
                        std::set<std::string> inventories);

    /**
     * Destroy the endpoint source and clean up all state
     */
    virtual ~ModelEndpointSource();

    /**
     * opflex::modb::ObjectListener
     */
    virtual void objectUpdated (opflex::modb::class_id_t class_id,
                                const opflex::modb::URI& uri);

private:
    typedef std::unordered_map<std::string, std::string> ep_map_t;

    opflex::ofcore::OFFramework& framework;

    /**
     * EPs that are known in the model
     */
    ep_map_t knownEps;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_MODELENDPOINTSOURCE_H */
