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

#pragma once
#ifndef OVSAGENT_AGENT_H
#define OVSAGENT_AGENT_H

#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>

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
     */
    Agent(opflex::ofcore::OFFramework& framework_);

    /**
     * Destroy the agent and clean up all state
     */
    ~Agent();

private:
    void start();
    void stop();

    opflex::ofcore::OFFramework& framework;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_AGENT_H */
