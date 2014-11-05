/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for Agent
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <set>
#include <utility>
#include <string>

#include <boost/property_tree/ptree.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>

#include "EndpointManager.h"
#include "EndpointSource.h"

#pragma once
#ifndef OVSAGENT_AGENT_H
#define OVSAGENT_AGENT_H

namespace ovsagent {

/**
 * Master object for the OVS agent.  This class holds the state for
 * the agent and handles initialization, configuration and cleanup.
 */
class Agent {
public:
    /**
     * Instantiate a new agent using the specified framework
     * instance.
     * 
     * @param framework the framework instance to use
     */
    Agent(opflex::ofcore::OFFramework& framework);

    /**
     * Destroy the agent and clean up all state
     */
    ~Agent();

    /**
     * Configure the agent with the property tree specified
     * 
     * @param properties the configuration properties to set for the
     * agent
     */
    void setProperties(const boost::property_tree::ptree& properties);

    /**
     * Start the agent
     */
    void start();

    /**
     * Stop the agent
     */
    void stop();

    /**
     * Get the policy manager object for this agent
     */
    PolicyManager& getPolicyManager() { return policyManager; }

    /**
     * Get the endpoint manager object for this agent
     */
    EndpointManager& getEndpointManager() { return endpointManager; }

private:
    opflex::ofcore::OFFramework& framework;
    PolicyManager policyManager;
    EndpointManager endpointManager;

    std::set<std::string> endpointSourcePaths;
    std::set<EndpointSource*> endpointSources;

    typedef std::pair<std::string, int> host_t;
    std::set<host_t> opflexPeers;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_AGENT_H */
