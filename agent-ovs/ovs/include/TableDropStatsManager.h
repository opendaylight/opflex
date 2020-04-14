/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for policy stats manager
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include "PolicyStatsManager.h"

#pragma once
#ifndef OPFLEXAGENT_TABLEDROPSTATSMANAGER_H
#define OPFLEXAGENT_TABLEDROPSTATSMANAGER_H

namespace opflexagent {

class Agent;
/**
 * Abstraction layer for table drop counters statistics collection
 */
class BaseTableDropStatsManager : public PolicyStatsManager {
public:
    /**s
     * Instantiate a new policy stats manager that will use the
     * provided io service for scheduling asynchronous tasks
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param switchManager the switchManager associated with the
     * Policy Stats Manager
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    BaseTableDropStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         long timer_interval = 30000):
                             PolicyStatsManager(agent, idGen, switchManager,
                                     timer_interval) {
        /*SwitchManager instance is assumed to be already
         *initialized at this point
         */
        switchManager.getForwardingTableList(tableDescMap);
    }

    /**
     * Destroy the policy stats manager and clean up all state
     */
    ~BaseTableDropStatsManager() {};

    /**
     * Start the policy stats manager
     */
    void start(bool register_listener=false);

    /**
     * Stop the policy stats manager
     */
    void stop(bool unregister_listener=false);

    /**
     * Timer interval handler.  For unit tests only.
     */
    void on_timer(const boost::system::error_code& ec) override;

    /** Interface: ObjectListener */
    void objectUpdated(opflex::modb::class_id_t class_id,
                       const opflex::modb::URI& uri) override;

    /* Interface: MessageListener */
    void Handle(SwitchConnection* connection,
                int msgType,
                ofpbuf *msg,
                struct ofputil_flow_removed* fentry=NULL) override;

    void handleTableDropStats(struct ofputil_flow_stats* fentry) override;


private:
    //Drop Flow Counter States per table
    std::unordered_map<int,
            PolicyStatsManager::FlowStats_t> TableDropCounterState;
    std::unordered_map<int,
        PolicyStatsManager::flowCounterState_t> CurrentDropCounterState;

    SwitchManager::TableDescriptionMap tableDescMap;

    void updateDropFlowStatsCounters(flowCounterState_t& counterState,
            uint64_t cookie, uint16_t priority, const struct match& match);
    void on_timer_base(const boost::system::error_code& ec,
            flowCounterState_t& counterState,
            PolicyStatsManager::FlowStats_t& dropFlowCounter);
};

/**
 * Periodically query an OpenFlow switch for int bridge table drops
 * and distribute them as needed to other components for reporting.
 */
class IntTableDropStatsManager : public BaseTableDropStatsManager {
public:
    /**
     * Instantiate a new policy stats manager that will use the
     * provided io service for scheduling asynchronous tasks
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param switchManager the switchManager associated with the
     * Policy Stats Manager
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    IntTableDropStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         long timer_interval = 30000):
                             BaseTableDropStatsManager(agent, idGen,
                                     switchManager, timer_interval){};

    /**
     * Destroy the policy stats manager and clean up all state
     */
    ~IntTableDropStatsManager() {};
};

/**
 * Periodically query an OpenFlow switch for access bridge table drops
 * and distribute them as needed to other components for reporting.
 */
class AccessTableDropStatsManager : public BaseTableDropStatsManager {
public:
    /**
     * Instantiate a new policy stats manager that will use the
     * provided io service for scheduling asynchronous tasks
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param switchManager the switchManager associated with the
     * Policy Stats Manager
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    AccessTableDropStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         long timer_interval = 30000):
                             BaseTableDropStatsManager(agent, idGen,
                                     switchManager, timer_interval) {};

    /**
     * Destroy the policy stats manager and clean up all state
     */
    ~AccessTableDropStatsManager() {};
};

/**
 * Facade and container class for table drop stats managers
 */
class TableDropStatsManager {
public:
    /**
     * Instantiate a table drop stats manager that will use the
     * policy stats framework for counting per table stats within
     * the list of bridges for the agent
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param intSwitchManager the integration switchManager associated with
     * the policy Stats Manager
     * @param accSwitchManager the access switchManager associated with
     * the policy Stats Manager
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    TableDropStatsManager(Agent* agent,
                          IdGenerator& idGen,
                          SwitchManager& intSwitchManager,
                          SwitchManager& accSwitchManager,
                          long timer_interval = 30000):
                             intTableDropStatsMgr(agent,
                                 idGen, intSwitchManager,
                                 timer_interval),
                             accTableDropStatsMgr(agent,
                                 idGen, accSwitchManager,
                                 timer_interval) {}
    /**
     * Start the table drop stats manager
     */
    void start() {
        intTableDropStatsMgr.start(false);
        accTableDropStatsMgr.start(false);
    }

    /**
     * Stop the table drop stats manager
     */
    void stop() {
        intTableDropStatsMgr.stop(false);
        accTableDropStatsMgr.stop(false);
    }

    /**
     * Set the agent instance ID for the respective PolicyStatsManager
     * instances.
     * @param uuid the UUID to set
     */
    void setAgentUUID(const std::string& uuid) {
        intTableDropStatsMgr.setAgentUUID(uuid);
        accTableDropStatsMgr.setAgentUUID(uuid);
    }
    /**
     * Set the interval between stats requests.
     *
     * @param timerInterval the interval in milliseconds
     */
    void setTimerInterval(long timerInterval) {
        intTableDropStatsMgr.setTimerInterval(timerInterval);
        accTableDropStatsMgr.setTimerInterval(timerInterval);
    }
    /**
     * Register the given connection with the policy stats manager.
     * This connection will be queried for counters.
     * @param intConnection the connection to use for integration bridge
     * @param accessConnection the connection to use for access bridge
     */
    void registerConnection(SwitchConnection* intConnection,
            SwitchConnection* accessConnection) {
        intTableDropStatsMgr.registerConnection(intConnection);
        if(accessConnection) {
            accTableDropStatsMgr.registerConnection(accessConnection);
        }
    }

private:
    IntTableDropStatsManager intTableDropStatsMgr;
    AccessTableDropStatsManager accTableDropStatsMgr;
    friend class TableDropStatsManagerFixture;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_TABLEDROPSTATSMANAGER_H */
