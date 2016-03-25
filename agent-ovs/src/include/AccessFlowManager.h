/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_ACCESSFLOWMANAGER_H_
#define OVSAGENT_ACCESSFLOWMANAGER_H_

#include <boost/noncopyable.hpp>

#include "Agent.h"
#include "EndpointManager.h"
#include "SwitchManager.h"
#include "TaskQueue.h"
#include "SwitchStateHandler.h"

namespace ovsagent {

/**
 * Manage the flow table state in the access bridge, which handles
 * per-endpoint security policy and security groups.
 */
class AccessFlowManager : public EndpointListener,
                          public SwitchStateHandler,
                          private boost::noncopyable {
public:
    /**
     * Construct a new access flow manager
     *
     * @param agent the associated agent
     * @param switchManager the switch manager for the access bridge
     * @param idGen the flow ID generator
     */
    AccessFlowManager(Agent& agent,
                      SwitchManager& switchManager,
                      IdGenerator& idGen);

    /**
     * Start the access flow manager
     */
    void start();

    /**
     * Stop the access flow manager
     */
    void stop();

    /* Interface: EndpointListener */
    virtual void endpointUpdated(const std::string& uuid);

private:
    Agent& agent;
    SwitchManager& switchManager;
    IdGenerator& idGen;
    TaskQueue taskQueue;
};

} // namespace ovsagent

#endif // OVSAGENT_ACCESSFLOWMANAGER_H_
