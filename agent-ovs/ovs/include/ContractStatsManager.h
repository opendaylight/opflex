/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for policy stats manager
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include "PolicyStatsManager.h"

#pragma once
#ifndef OPFLEXAGENT_ContractStatsManager_H
#define OPFLEXAGENT_ContractStatsManager_H

namespace opflexagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class ContractStatsManager : public PolicyStatsManager {
public:
    /**
     * Instantiate a new policy stats manager that will use the
     * provided io service for scheduling asynchronous tasks
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param switchManager the switchManager associated with the
     * Policy Stats Manager
     * @param idGen the flow ID generator
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    ContractStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         long timer_interval = 30000);

    /**
     * Destroy the policy stats manager and clean up all state
     */
    virtual ~ContractStatsManager();

    /**
     * Start the policy stats manager
     */
    void start();

    /**
     * Stop the policy stats manager
     */
    void stop();

    /**
     * Get and increment the drop counter generation ID
     *
     * @return the current drop counter generation ID
     */
    uint64_t getNextDropGenId() { return ++dropGenId; };

    /**
     * Get the drop counter generation ID
     *
     * @return the current drop counter generation ID
     */
    uint64_t getCurrDropGenId() const { return dropGenId; };

    /**
     * Timer interval handler.  For unit tests only.
     */
    void on_timer(const boost::system::error_code& ec) override;

    /*
     * Derived class API to delete indexth counter object
     */
    void clearCounterObject(const std::string& key,uint8_t index) override;

    /** Interface: ObjectListener */
    void objectUpdated(opflex::modb::class_id_t class_id,
                       const opflex::modb::URI& uri) override;

    /* Interface: MessageListener */
    void Handle(SwitchConnection* connection,
                int msgType, ofpbuf *msg) override;

    void updatePolicyStatsCounters(const std::string& srcEpg,
                                   const std::string& dstEpg,
                                   const std::string& ruleURI,
                                   PolicyCounters_t & counters) override;
    void handleDropStats(struct ofputil_flow_stats* fentry) override;

private:
    typedef std::tuple<std::string,std::string,std::string> counter_key_t;
    std::unordered_map<int, counter_key_t> counterObjectKeys_;

    std::unordered_map<std::string,
                       std::unique_ptr<CircularBuffer>> dropCounterList_;

    flowCounterState_t contractState;
    std::atomic<std::uint64_t> dropGenId;

    /**
     * Drop Counters for Routing Domain
     */
    struct PolicyDropCounters_t {
        boost::optional<uint64_t> packet_count;
        boost::optional<uint64_t> byte_count;
        boost::optional<std::string> rdURI;
    };

    /* map Routing Domain Id to Policy Drop counters */
    typedef std::unordered_map<uint32_t,
                               PolicyDropCounters_t> PolicyDropCounterMap_t;
    PolicyDropCounterMap_t policyDropCountersMap;

    void updatePolicyStatsDropCounters(const std::string& rdURI,
                                       PolicyDropCounters_t& counters);
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ContractStatsManager_H */
