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
#ifndef OPFLEXAGENT_SecGrpStatsManager_H
#define OPFLEXAGENT_SecGrpStatsManager_H

namespace opflexagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class SecGrpStatsManager : public PolicyStatsManager {
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
    SecGrpStatsManager(Agent* agent, IdGenerator& idGen,
                       SwitchManager& switchManager,
                       long timer_interval = 30000);

    /**
     * Destroy the policy stats manager and clean up all state
     */
    virtual ~SecGrpStatsManager();

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
    void on_timer(const boost::system::error_code& ec);

    /**
     * Delete indexth counter object
     *
     * @param key the key of the object to remove
     * @param index the index of the counter to remove
     */
    void clearCounterObject(const std::string& key,uint8_t index);

    /** Interface: ObjectListener */
    void objectUpdated(opflex::modb::class_id_t class_id,
                       const opflex::modb::URI& uri) override;

    /* Interface: MessageListener */
    void Handle(SwitchConnection* connection,
                int msgType, ofpbuf *msg) override;

    void updatePolicyStatsCounters(const std::string& l24Classifier,
                                   PolicyCounters_t& newVals1,
                                   PolicyCounters_t& newVals2) override;

private:
    flowCounterState_t secGrpInState;
    flowCounterState_t secGrpOutState;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SecGrpStatsManager_H */
