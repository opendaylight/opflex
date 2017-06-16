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
#ifndef OVSAGENT_ACCSTATSMANAGER_H
#define OVSAGENT_ACCSTATSMANAGER_H

namespace ovsagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class AccStatsManager : public BaseStatsManager {
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
    AccStatsManager(Agent* agent, IdGenerator& idGen,
                       SwitchManager& switchManager,
                       long timer_interval = 30000);

    /**
     * Destroy the policy stats manager and clean up all state
     */
    ~AccStatsManager();

    /**
     * Register the given connection with the policy stats manager.
     * This connection will be queried for counters.
     *
     * @param intConnection the connection to use for integration
     * bridge policy stats collection
     */
    void registerConnection(SwitchConnection* accConnection);

    /**
     * Start the policy stats manager
     */
    void start();

    /**
     * Stop the policy stats manager
     */
    void stop();

    /**
     * Get and increment the classifier counter generation ID
     *
     * @return the current classifier generation ID
     */
    /**
     * Timer interval handler.  For unit tests only.
     */
    void on_timer(const boost::system::error_code& ec);

private:
    void updateAccInFlowEntryMap(uint64_t cookie, uint16_t priority,
                            const struct match& match);
    void updateAccOutFlowEntryMap(uint64_t cookie, uint16_t priority,
                            const struct match& match);
    void updatePolicyStatsCounters(const std::string& l24Classifier,
                          PolicyCounters_t& newVals1,
                          PolicyCounters_t& newVals2);
    void generatePolicyStatsObjects(PolicyCounterMap_t& counters1,PolicyCounterMap_t& counters2);


    void handleFlowStats(int msgType, ofpbuf *msg);

    void handleFlowRemoved(ofpbuf *msg);
};

} /* namespace ovsagent */

#endif /* OVSAGENT_ACCSTATSMANAGER_H */
