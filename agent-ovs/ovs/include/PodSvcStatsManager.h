/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for pod <--> svc stats manager
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include "PolicyStatsManager.h"

#pragma once
#ifndef OPFLEXAGENT_PodSvcStatsManager_H
#define OPFLEXAGENT_PodSvcStatsManager_H

namespace opflexagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class PodSvcStatsManager : public PolicyStatsManager {
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
    PodSvcStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         IntFlowManager& intFlowManager,
                         long timer_interval = 30000);

    /**
     * Destroy the policy stats manager and clean up all state
     */
    virtual ~PodSvcStatsManager();

    /**
     * Start the policy stats manager
     */
    void start();

    /**
     * Stop the policy stats manager
     */
    void stop();

    /**
     * Timer interval handler.  For unit tests only.
     */
    void on_timer(const boost::system::error_code& ec) override;

    /**
     * Interface: MessageListener 
     */
    void Handle(SwitchConnection* connection,
                int msgType,
                ofpbuf *msg,
                struct ofputil_flow_removed* fentry=NULL) override;

    /** Interface: ObjectListener */
    // Note: This is used to delete observer MOs.
    // EpToSvc and SvcToEp objects are removed in IntFlowManager
    void objectUpdated(opflex::modb::class_id_t class_id,
                       const opflex::modb::URI& uri) override {};

private:
    /**
     * Type used as a key for Pod-Svc counter maps
     */
    struct PodSvcFlowMatchKey_t {
        /**
         * Trivial constructor for pod<-->svc match key
         */
        PodSvcFlowMatchKey_t(uint32_t k1) {
            cookie = k1;
        }

        /**
         * Flow cookie
         */
        uint32_t cookie;

        /**
         * equality operator
         */
        bool operator==(const PodSvcFlowMatchKey_t &other) const;
    };

    /**
     * Hasher for PodSvcFlowMatchKey_t
     */
    struct PodSvcKeyHasher {
        /**
         * Hash for FlowMatch Key
         */
        size_t operator()(const PodSvcFlowMatchKey_t& k) const noexcept;
    };

    /** map flow to Pod-Svc info and counters */
    typedef std::unordered_map<PodSvcFlowMatchKey_t, 
                               FlowStats_t,
                               PodSvcKeyHasher> PodSvcCounterMap_t;

    // Flow state of all pod<-->svc flows in stats table
    flowCounterState_t statsState;

    /**
     * Get aggregated stats counters from for Pod<-->Svc flows
     */
    void on_timer_base(const boost::system::error_code& ec,
                       flowCounterState_t& counterState,
                       PodSvcCounterMap_t& newClassCountersMap);

    /**
     * Generate/update the pod<-->svc stats objects for from the counter maps
     */
    void updatePodSvcStatsObjects(PodSvcCounterMap_t *counters);

    /**
     * The integration bridge flow manager
     */
    IntFlowManager& intFlowManager;

    friend class PodSvcStatsManagerFixture;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PodSvcStatsManager_H */
