/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_ACCESSFLOWMANAGER_H_
#define OPFLEXAGENT_ACCESSFLOWMANAGER_H_

#include <boost/noncopyable.hpp>

#include <opflexagent/Agent.h>
#include <opflexagent/EndpointManager.h>
#include <opflexagent/PolicyListener.h>
#include "PortMapper.h"
#include "SwitchManager.h"
#include <opflexagent/TaskQueue.h>
#include "SwitchStateHandler.h"

namespace opflexagent {

class CtZoneManager;

/**
 * Manage the flow table state in the access bridge, which handles
 * per-endpoint security policy and security groups.
 */
class AccessFlowManager : public EndpointListener,
                          public PortStatusListener,
                          public PolicyListener,
                          public SwitchStateHandler,
                          private boost::noncopyable {
public:
    /**
     * Construct a new access flow manager
     *
     * @param agent the associated agent
     * @param switchManager the switch manager for the access bridge
     * @param idGen the flow ID generator
     * @param ctZoneManager the conntrack zone manager
     */
    AccessFlowManager(Agent& agent,
                      SwitchManager& switchManager,
                      IdGenerator& idGen,
                      CtZoneManager& ctZoneManager);

    /**
     * Enable connection tracking support
     */
    void enableConnTrack();

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
    virtual void secGroupSetUpdated(const EndpointListener::uri_set_t& secGrps);

    /* Interface: PolicyListener */
    virtual void secGroupUpdated(const opflex::modb::URI&);
    virtual void configUpdated(const opflex::modb::URI& configURI);

    /* Interface: PortStatusListener */
    virtual void portStatusUpdate(const std::string& portName,
                                  uint32_t portNo, bool fromDesc);

    /**
     * Run periodic cleanup tasks
     */
    void cleanup();

    /**
     * Indices of tables managed by the access flow manager.
     */
    enum {
        /**
         * Map packets to a security group and set their destination
         * port after applying policy
         */
        GROUP_MAP_TABLE_ID,
        /**
         * Enforce security group policy on packets coming in to the
         * endpoint from the switch
         */
        SEC_GROUP_IN_TABLE_ID,
        /**
         * Enforce security group policy on packets coming out from
         * the endpoint to the switch
         */
        SEC_GROUP_OUT_TABLE_ID,
        /**
         * Output to the final destination port
         */
        OUT_TABLE_ID,
        /**
         * The total number of flow tables
         */
        NUM_FLOW_TABLES
    };

private:
    void createStaticFlows();
    void handleEndpointUpdate(const std::string& uuid);
    void handleSecGrpUpdate(const opflex::modb::URI& uri);
    void handlePortStatusUpdate(const std::string& portName, uint32_t portNo);
    void handleSecGrpSetUpdate(const EndpointListener::uri_set_t& secGrps,
                               const std::string& secGrpsId);

    Agent& agent;
    SwitchManager& switchManager;
    IdGenerator& idGen;
    CtZoneManager& ctZoneManager;
    TaskQueue taskQueue;

    bool conntrackEnabled;
    bool stopping;
};

} // namespace opflexagent

#endif // OPFLEXAGENT_ACCESSFLOWMANAGER_H_
