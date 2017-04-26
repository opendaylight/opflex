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


#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agent.h"
#include "SwitchConnection.h"
#include "IdGenerator.h"

#include "ovs-ofputil.h"

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
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the flow ID generator
     * @param timer_interval the interval for the stats timer in milliseconds
     */
    PolicyStatsManager(Agent* agent, IdGenerator& idGen_,
                 SwitchManager& switchManager,
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

    uint64_t getNextClsfrGenId() { return ++clsfrGenId; };
    uint64_t getCurrClsfrGenId() { return clsfrGenId; };
    uint64_t resetClsfrGenId() { clsfrGenId = 0; };
    uint64_t getNextDropGenId() { return ++dropGenId; };
    uint64_t getCurrDropGenId() { return dropGenId; };
    uint64_t resedDropGenId() { dropGenId = 0; };
    std::string& getAgentUUID() { return agentUUID; };

private:
    Agent* agent;
    SwitchManager& switchManager;
    SwitchConnection* intConnection;
    IdGenerator& idGen;
    boost::asio::io_service& agent_io;
    long timer_interval;
    std::unique_ptr<boost::asio::deadline_timer> timer;
    std::string agentUUID;
    std::atomic<std::uint64_t> clsfrGenId;
    std::atomic<std::uint64_t> dropGenId;


    struct FlowMatchKey_t {
        uint32_t cookie;
        uint32_t reg0;
        uint32_t reg2;
        bool operator==(const FlowMatchKey_t &other) const {
            return (cookie == other.cookie
                 && reg0 == other.reg0
                 && reg2 == other.reg2);
        }
    };

    struct KeyHasher {
        /**
         * Hash for FlowMatch Key
         */
        size_t operator()(const FlowMatchKey_t& k) const noexcept;
    };

    /* per flow entry match key */
    struct FlowEntryMatchKey_t {
        uint32_t cookie;
        uint32_t priority;
        struct match match;
        bool operator==(const FlowEntryMatchKey_t &other) const {
            return (cookie == other.cookie
                 && priority == other.priority
                 && match_equal(&match, &(other.match)));
        }
    };

    /**
     * Per FlowEntry counters and state information
     */
    struct FlowCounters_t {
         bool visited;
         bool deleted;
         uint32_t age;
         time_t create_time;
         time_t refresh_time;
         struct match match;
         boost::optional<uint64_t> diff_packet_count;
         boost::optional<uint64_t> diff_byte_count;
         boost::optional<uint64_t> last_packet_count;
         boost::optional<uint64_t> last_byte_count;
    };

    /**
     * Counters for L24Classifiers
     */
    struct PolicyCounters_t {
         boost::optional<uint64_t> packet_count;
         boost::optional<uint64_t> byte_count;
    };
    /* map flow to Policy counters */ 
    typedef std::unordered_map<FlowMatchKey_t, PolicyCounters_t, KeyHasher> PolicyCounterMap_t;

    void updatePolicyStatsCounters(const std::string& srcEpg,
                                   const std::string&  dstEpg,
                                   const std::string& ruleURI,
                                   PolicyCounters_t& counters);

    PolicyCounterMap_t policyCountersMap;

    /**
     * Drop Counters for Routing Domain
     */
    struct PolicyDropCounters_t {
         boost::optional<uint64_t> packet_count;
         boost::optional<uint64_t> byte_count;
         boost::optional<std::string> rdURI;
    };

    /* map Routing Domain Id to Policy Drop counters */

    typedef std::unordered_map<uint32_t, PolicyDropCounters_t> PolicyDropCounterMap_t;

   struct FlowKeyHasher {
        /**
         * Hash for FlowEntryMatch Key
         */
        size_t operator()(const FlowEntryMatchKey_t& k) const noexcept;
    };

    /* flow entry counter map */
    typedef std::unordered_map<FlowEntryMatchKey_t, FlowCounters_t, FlowKeyHasher> FlowEntryCounterMap_t;

    static FlowEntryCounterMap_t oldFlowCounterMap;
    static FlowEntryCounterMap_t newFlowCounterMap;
    static FlowEntryCounterMap_t removedFlowCounterMap;

    void static updateFlowEntryMap(uint64_t cookie, uint16_t priority,
                            const struct match& match);

    void generatePolicyStatsObjects(PolicyCounterMap_t& counters);

    void updatePolicyStatsDropCounters(const std::string& rdURI,
                                   PolicyDropCounters_t& counters);

    void updateNewFlowCounters(uint32_t cookie,
                             uint16_t priority,
                             struct match& match,
                             uint64_t packet_count,
                             uint64_t byte_count,
                             bool flowRemoved);

    // void updatePolicyStatsMap(PolicyCounterMap_t& newCountersMap);

    void handleFlowStats(int msgType, ofpbuf *msg);

    void handleFlowRemoved(ofpbuf *msg);

    void handleDropStats(uint32_t rdId,
                         boost::optional<std::string> idRdStr,
                         struct ofputil_flow_stats* fentry);

    PolicyDropCounterMap_t policyDropCountersMap;

    std::mutex pstatMtx;

    void on_timer(const boost::system::error_code& ec);

    volatile bool stopping;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_POLICYSTATSMANAGER_H */

