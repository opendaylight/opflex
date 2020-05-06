/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for service stats manager
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include "PolicyStatsManager.h"

#pragma once
#ifndef OPFLEXAGENT_ServiceStatsManager_H
#define OPFLEXAGENT_ServiceStatsManager_H

namespace opflexagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class ServiceStatsManager : public PolicyStatsManager {
public:
    /**
     * Instantiate a new policy stats manager that will use the
     * provided io service for scheduling asynchronous tasks
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param switchManager the switchManager associated with the
     * Policy Stats Manager
     * @param intFlowManager the intFlowManager associated with the
     * Policy Stats Manager
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    ServiceStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         IntFlowManager& intFlowManager,
                         long timer_interval = 30000);

    /**
     * Destroy the policy stats manager and clean up all state
     */
    virtual ~ServiceStatsManager();

    /**
     * Start the policy stats manager
     */
    void start();

    /**
     * Stop the policy stats manager
     */
    void stop();

    /**
     * Timer interval handler
     */
    void on_timer(const boost::system::error_code& ec) override;

    /**
     * Interface: MessageListener 
     */
    void Handle(SwitchConnection* connection,
                int msgType,
                ofpbuf *msg,
                struct ofputil_flow_removed* fentry=NULL) override;

    /**
     * Update stats state
     */
    void update_state(const boost::system::error_code& ec);

    /** Interface: ObjectListener */
    // Note: This is used to delete observer MOs.
    // EpToSvc and SvcToEp objects are removed in IntFlowManager
    void objectUpdated(opflex::modb::class_id_t class_id,
                       const opflex::modb::URI& uri) override {};

private:
    /**
     * Type used as a key for service counter maps
     */
    struct ServiceFlowMatchKey_t {
        /**
         * Trivial constructor for service match key
         */
        ServiceFlowMatchKey_t(uint32_t k1) {
            cookie = k1;
        }

        /**
         * Flow cookie
         */
        uint32_t cookie;

        /**
         * equality operator
         */
        bool operator==(const ServiceFlowMatchKey_t &other) const;
    };

    /**
     * Hasher for ServiceFlowMatchKey_t
     */
    struct ServiceKeyHasher {
        /**
         * Hash for FlowMatch Key
         */
        size_t operator()(const ServiceFlowMatchKey_t& k) const noexcept;
    };

    /** map flow to service info and counters */
    typedef std::unordered_map<ServiceFlowMatchKey_t,
                               FlowStats_t,
                               ServiceKeyHasher> ServiceCounterMap_t;

    // Flow state of all service flows in stats table
    flowCounterState_t statsState;

    // Flow state of all service flows in svh table
    flowCounterState_t svhState;

    // Flow state of all service flows in svr table
    flowCounterState_t svrState;

    /**
     * Get aggregated stats counters from for service flows
     */
    void on_timer_base(const boost::system::error_code& ec,
                       flowCounterState_t& counterState,
                       ServiceCounterMap_t& newClassCountersMap);

    /**
     * Generate/update the service stats objects for from the counter maps
     */
    void updateServiceStatsObjects(ServiceCounterMap_t *counters);

    /**
     * The integration bridge flow manager
     */
    IntFlowManager& intFlowManager;

    friend class ServiceStatsManagerFixture;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ServiceStatsManager_H */
