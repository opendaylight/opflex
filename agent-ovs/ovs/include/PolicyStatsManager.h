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

#include "SwitchConnection.h"
#include <opflexagent/IdGenerator.h>

#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>

#pragma once
#ifndef OPFLEXAGENT_POLICYSTATSMANAGER_H
#define OPFLEXAGENT_POLICYSTATSMANAGER_H

namespace opflexagent {

class Agent;
class SwitchManager;

/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class PolicyStatsManager : private boost::noncopyable,
                           public opflex::modb::ObjectListener,
                           public MessageHandler {
public:
    /**
     * Instantiate a new policy stats manager that will use the
     * provided io service for scheduling asynchronous tasks
     * @param agent the agent associated with the Policy Stats Manager
     * @param idGen the ID generator
     * @param switchManager the switchManager associated with the
     * Base Stats Manager
     * @param idGen the flow ID generator
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    PolicyStatsManager(Agent* agent, IdGenerator& idGen,
                       SwitchManager& switchManager,
                       long timer_interval = 10000);

    /**
     * Destroy the base stats manager and clean up all state
     */
    virtual ~PolicyStatsManager();

    /**
     * Register the given connection with the policy stats manager.
     * This connection will be queried for counters.
     *
     * @param connection the connection to use for stats collection
     */
    void registerConnection(SwitchConnection* connection);

    /**
     * Set the interval between stats requests.
     *
     * @param timerInterval the interval in milliseconds
     */
    void setTimerInterval(long timerInterval) {
        timer_interval = timerInterval;
    }

    /**
     * Start the policy stats manager
     */
    void start();

    /**
     * Get the classifier counter generation ID
     *
     * @return the current classifier generation ID
     */
    uint64_t getCurrClsfrGenId() const { return clsfrGenId; };

    /**
     * Stop the policy stats manager
     */
    void stop();

    /**
     * Get and increment the classifier counter generation ID
     *
     * @return the current classifier generation ID
     */
    uint64_t getNextClsfrGenId() { return ++clsfrGenId; };

    /**
     * Set the agent instance ID for this PolicyStatsManager instance.
     *
     * @param uuid the UUID to set
     */
    void setAgentUUID(std::string uuid) { agentUUID = uuid; };

    /**
     * Get the agent instance ID for this PolicyStatsManager instance.
     *
     * @return the generated UUID string
     */
    const std::string& getAgentUUID() const { return agentUUID; };

    /**
     * timer function for unit tests
     */
    virtual void on_timer(const boost::system::error_code& ec) = 0;

    /**
     * Size of window of counters to maintain for each classifier
     */
    static const int MAX_COUNTER_LIMIT = 5;

    /**
     * Size of window of drop counters to maintain for each domain
     */
    static const int MAX_DROP_COUNTER_LIMIT = 5;

protected:
    /**
     * Type used as a key for counter maps
     */
    struct FlowMatchKey_t {
        /**
         * Trivial constructor for flow match key
         */
        FlowMatchKey_t(uint32_t k1, uint32_t k2, uint32_t k3) {
            cookie = k1;
            reg0 = k2;
            reg2 = k3;
        }

        /**
         * Flow cookie
         */
        uint32_t cookie;
        /**
         * The source register
         */
        uint32_t reg0;
        /**
         * The dest register
         */
        uint32_t reg2;

        /**
         * equality operator
         */
        bool operator==(const FlowMatchKey_t &other) const;
    };

    /**
     * Hasher for FlowMatchKey_t
     */
    struct KeyHasher {
        /**
         * Hash for FlowMatch Key
         */
        size_t operator()(const FlowMatchKey_t& k) const noexcept;
    };

    /**
     * Per flow entry match key
     */
    struct FlowEntryMatchKey_t {
        /**
         * Basic constructor for FlowEntryMatchKey_t
         */
        FlowEntryMatchKey_t(uint32_t k1, uint32_t k2, const struct match& k3);

        /**
         * Copy constructor
         */
        FlowEntryMatchKey_t(const FlowEntryMatchKey_t& o);

        /**
         * Flow cookie
         */
        uint32_t cookie;
        /**
         * Flow priority
         */
        uint32_t priority;
        /**
         * Match structure
         */
        std::unique_ptr<struct match> match;

        /**
         * equality operator
         */
        bool operator==(const FlowEntryMatchKey_t &other) const;
    };

    /**
     * Hasher for FlowEntryMatchKey_t
     */
    struct FlowKeyHasher {
        /**
         * Hash for FlowEntryMatch Key
         */
        size_t operator()(const FlowEntryMatchKey_t& k) const noexcept;
    };

    /**
     * Per FlowEntry counters and state information
     */
    struct FlowCounters_t {
        /**
         * visited bit for counter
         */
        bool visited;
        /**
         * Age of counter
         */
        uint32_t age;

        /**
         * match associated with this counter
         */
        std::unique_ptr<struct match> match;

        /**
         * Current diff in packages
         */
        boost::optional<uint64_t> diff_packet_count;
        /**
         * Current diff in bytes
         */
        boost::optional<uint64_t> diff_byte_count;
        /**
         * Previous packet count
         */
        boost::optional<uint64_t> last_packet_count;
        /**
         * Previous byte count
         */
        boost::optional<uint64_t> last_byte_count;
    };

    /**
     * Counters for L24Classifiers
     */
    struct PolicyCounters_t {
        /**
         * Counter in packets
         */
        boost::optional<uint64_t> packet_count;
        /**
         * Counter in bytes
         */
        boost::optional<uint64_t> byte_count;
    };

    /** flow entry counter map */
    typedef std::unordered_map<FlowEntryMatchKey_t, FlowCounters_t,
                               FlowKeyHasher> FlowEntryCounterMap_t;

    /**
     * Store flow entry state
     */
    struct flowCounterState_t : private boost::noncopyable {
        /**
         * The previous flow entry state
         */
        FlowEntryCounterMap_t oldFlowCounterMap;
        /**
         * The new flow entries
         */
        FlowEntryCounterMap_t newFlowCounterMap;
        /**
         * Removed flow entries
         */
        FlowEntryCounterMap_t removedFlowCounterMap;
    };

    /** map flow to Policy counters */
    typedef std::unordered_map<FlowMatchKey_t, PolicyCounters_t,
                               KeyHasher> PolicyCounterMap_t;
    /**
     * A simple circular buffer of integers
     */
    struct CircularBuffer {
        /**
         * Storage for buffer
         */
        std::vector<uint64_t> uidList;
        /**
         * Current index
         */
        uint64_t count;

        /**
         * Initialize the buffer
         */
        CircularBuffer() : count(0) {}
    };

    /**
     * map classifier URIs to a circular buffer of counter ids
     */
    std::unordered_map<std::string,std::unique_ptr<CircularBuffer>> genIdList_;

    /**
     * mutex for stat indexes
     */
    std::mutex pstatMtx;

    /**
     * ID generator
     */
    IdGenerator& idGen;

    /**
     * The agent object
     */
    Agent* agent;

    /**
     * The switch manager that owns the bridges
     */
    SwitchManager& switchManager;

    /**
     * The switch connection for this stats manager
     */
    SwitchConnection* connection;

    /**
     * timer for periodically querying for stats
     */
    std::unique_ptr<boost::asio::deadline_timer> timer;

    /**
     * The timer interval to use for querying stats
     */
    long timer_interval;

    /**
     * A UUID for this agent instance
     */
    std::string agentUUID;

    /**
     * Atomic generation counter for stats syncing
     */
    std::atomic<uint64_t> clsfrGenId{0};

    /**
     * Update a the flow entry maps from the given entry
     */
    void updateFlowEntryMap(flowCounterState_t& counterState,
                            uint64_t cookie, uint16_t priority,
                            const struct match& match);

    /**
     * Send a flow stats request to the given table
     */
    void sendRequest(uint32_t table_id);

    /**
     * Clear stale counter values
     */
    void clearOldCounters(const std::string& key, uint64_t nextId);

    /**
     * Clear all counters for the given stale key
     */
    void removeAllCounterObjects(const std::string& key);

    /**
     * A functor that maps a table ID to a flow counter state object
     */
    typedef std::function<flowCounterState_t* (uint32_t)> table_map_t;

    /**
     * handle the OpenFlow message provided using the given table map
     */
    void handleMessage(int msgType, ofpbuf *msg, const table_map_t& tableMap);

    /**
     * Handle a drop stats message
     */
    virtual void handleDropStats(struct ofputil_flow_stats* fentry) {};

    /**
     * Generate the policy stats objects for from the counter maps
     */
    void generatePolicyStatsObjects(PolicyCounterMap_t *counters1,
                                    PolicyCounterMap_t *counters2 = NULL);

    /**
     * Base timer function
     */
    virtual void on_timer_base(const boost::system::error_code& ec,
                               flowCounterState_t& counterState,
                               PolicyCounterMap_t& newClassCountersMap);

    /**
     * Clear the counter objects for the given key
     */
    virtual void clearCounterObject(const std::string& key,uint8_t index) = 0;

    /**
     * Update the security group stats counters
     */
    virtual void updatePolicyStatsCounters(const std::string& l24Classifier,
                                           PolicyCounters_t& newVals1,
                                           PolicyCounters_t& newVals2) {};

    /**
     * Update the contract stats counters
     */
    virtual void updatePolicyStatsCounters(const std::string& srcEpg,
                                           const std::string& dstEpg,
                                           const std::string& ruleURI,
                                           PolicyCounters_t& counters) {};

    /**
     * True if shutting down
     */
    volatile bool stopping;

private:
    void handleFlowStats(ofpbuf *msg, const table_map_t& tableMap);
    void handleFlowRemoved(ofpbuf *msg, const table_map_t& tableMap);

    void updateNewFlowCounters(uint32_t cookie,
                               uint16_t priority,
                               struct match& match,
                               uint64_t flow_packet_count,
                               uint64_t flow_byte_count,
                               flowCounterState_t& counterState,
                               bool flowRemoved);
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_POLICYSTATSMANAGER_H */
