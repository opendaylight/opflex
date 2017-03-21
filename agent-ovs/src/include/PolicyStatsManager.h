/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for policy stats manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <boost/asio.hpp>

#include "SwitchConnection.h"

#include <string>
#include <unordered_map>
#include <mutex>

#pragma once
#ifndef OVSAGENT_POLICYSTATSMANAGER_H
#define OVSAGENT_POLICYSTATSMANAGER_H

namespace ovsagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for counters and stats and
 * distribute them as needed to other components for reporting.
 */
class PolicyStatsManager : private boost::noncopyable,
                     public MessageHandler {
public:
    /**
     * Instantiate a new stats manager that will use the provided io
     * service for scheduling asynchronous tasks
     * @param agent the agent associated with the stats manager
     * @param timer_interval the interval for the stats timer in milliseconds
     */
    PolicyStatsManager(Agent* agent,
                 SwitchManager& switchManager,
                 IdGenerator& idGen,
                 long timer_interval = 30000);

    /**
     * Destroy the stats manager and clean up all state
     */
    ~PolicyStatsManager();

    /**
     * Register the given connection with the stats manager.  This
     * connection will be queried for counters.
     *
     * @param intConnection the connection to use for integration
     * bridge stats collection
     */
    void registerConnection(SwitchConnection* intConnection);


    /**
     * Start the stats manager
     */
    void start();

    /**
     * Stop the stats manager
     */
    void stop();

    // see: MessageHandler
    void Handle(SwitchConnection *swConn, int type, ofpbuf *msg);

private:
    Agent* agent;
    SwitchManager& switchManager,
    SwitchConnection* intConnection;
    IdGenerator& idGen;
    boost::asio::io_service& agent_io;
    long timer_interval;
    std::unique_ptr<boost::asio::deadline_timer> timer;


#if 0
    struct FlowMatchKey_t {
        uint64_t cookie;
        uint64_t reg0;
        uint64_t reg2;
    };
    /**
     * Counters for L24Classifiers
     */
    struct PolicyCounters_t {
#define  PSM_FLOW_CREATED          1
#define  PSM_FLOW_TO_BE_DELETED    2
         /* if we received flow deletion then mark flow_state "to be deleted" 
            and remove it under any of the following condition.
            1. After receiving last set of flow counter values from switch manager.
            2. Dead timer interval expires (to account for OpenVswitchD died and came up)
            3. A new flow entry with same Cookie is recreated then irrespective
               of flow_state, delete the old entry and create a new one. 
         */
         uint8_t   flow_state; 
         time_t    last_collection_time;
         uint64_t  match_pkts;
         uint64_t  match_bytes;
    };
    /* map flow to Policy counters */ 
    typedef std::unordered_map<FlowMatchKey_t, PolicyCounters_t> PolicyCounterMap_t;

    void updatePolicyStatsCounters(const opflex::modb::URI& ruleURI,
                                   const opflex::modb::URI& srcEpg,
                                   const opflex::modb::URI& dstEpg,
                                   PolicyCounters_t counters);

    PolicyCounterMap_t policyCounterMap;
#endif

    std::mutex pstatMtx;

    void on_timer(const boost::system::error_code& ec);

    volatile bool stopping;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_POLICYSTATSMANAGER_H */

