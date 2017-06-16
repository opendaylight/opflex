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


#include "BaseStatsManager.h"

#pragma once
#ifndef OVSAGENT_ContractStatsManager_H
#define OVSAGENT_ContractStatsManager_H

namespace ovsagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class ContractStatsManager : public BaseStatsManager, public PolicyListener {
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
    ~ContractStatsManager();

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
    void on_timer(const boost::system::error_code& ec);
    bool isTableIdFound(uint8_t table_id);
    void resolveCounterMaps(uint8_t table_id);
    void objectUpdated(opflex::modb::class_id_t class_id,
        const opflex::modb::URI& uri);
    void clearCounterObject(const std::string& key,uint8_t index);
    void contractUpdated(const opflex::modb::URI& contractURI);
    void egDomainUpdated(const opflex::modb::URI& epgURI);
    void handleDomainUpdate(opflex::modb::class_id_t cid, const opflex::modb::URI& domURI);
private:
    std::unordered_map<std::string,std::tuple<std::string,std::string,std::string> > keyMap_;
    std::unordered_map<std::string,std::vector<std::tuple<std::string,std::string,std::string> > > classifierKeyMap_;
    std::unordered_map<std::string,std::vector<std::tuple<std::string,std::string,std::string> > > epgKeyMap_;
    std::unordered_map<std::string,CircularBuffer*> dropCounterList_;

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



    void updatePolicyStatsDropCounters(const std::string& rdURI,
                                       PolicyDropCounters_t& counters);

    void updatePolicyStatsCounters(const std::string& srcEpg,
                                   const std::string& dstEpg,
                                   const std::string& ruleURI,
                                   PolicyCounters_t& counters);
    void updatePolicyFlowEntryMap(uint64_t cookie, uint16_t priority,
                                            const struct match& match);

    void handleDropStats(uint32_t rdId,
                         boost::optional<std::string> idRdStr,
                         struct ofputil_flow_stats* fentry);
    void updatePolicyStatsCounters(const std::string& l24Classifier,
                          PolicyCounters_t& newVals1,
                          PolicyCounters_t& newVals2) {
    }
    PolicyDropCounterMap_t policyDropCountersMap;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_ContractStatsManager_H */
