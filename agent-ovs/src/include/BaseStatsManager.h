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
#ifndef OVSAGENT_BASESTATSMANAGER_H
#define OVSAGENT_BASESTATSMANAGER_H

namespace ovsagent {

class Agent;
class SwitchManager;
/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class BaseStatsManager : private boost::noncopyable,
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
    BaseStatsManager(Agent* agent, IdGenerator& idGen,
                       SwitchManager& switchManager,
                       long timer_interval = 30000);

    /**
     * Destroy the base stats manager and clean up all state
     */
    ~BaseStatsManager();
    /**
     * Register the given connection with the policy stats manager.
     * This connection will be queried for counters.
     *
     * @param intConnection the connection to use for integration
     * bridge policy stats collection
     */
    virtual void registerConnection(SwitchConnection* connection);

    /**
     * Start the policy stats manager
     */
    void start();
   /**
     * Get the classifier counter generation ID
     *
     * @return the current classifier generation ID
     */
    virtual uint64_t getCurrClsfrGenId() const { return clsfrGenId; };
    /**
     * Stop the policy stats manager
     */
    void stop();
    /**
     * Get and increment the classifier counter generation ID
     *
     * @return the current classifier generation ID
     */
    virtual uint64_t getNextClsfrGenId() { return ++clsfrGenId; };
    /**
     * Get the agent instance ID for this PolicyStatsManager instance.
     *
     * @return the generated UUID string
     */
    virtual const std::string& getAgentUUID() const { return agentUUID; };



    // see: MessageHandler
    virtual void Handle(SwitchConnection *swConn, int type, ofpbuf *msg);
      struct FlowMatchKey_t {
        uint32_t cookie;
        uint32_t reg0;
        uint32_t reg2;
        bool operator==(const FlowMatchKey_t &other) const {
            return (cookie == other.cookie
                    && reg0 == other.reg0
                    && reg2 == other.reg2);
        }
        FlowMatchKey_t(uint32_t k1, uint32_t k2, uint32_t k3) {
            cookie = k1;
            reg0 = k2;
            reg2 = k3;
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
        FlowEntryMatchKey_t(uint32_t k1, uint32_t k2, struct match k3) {
            cookie = k1;
            priority = k2;
            match = k3;
        }
    };

    /**
     * Per FlowEntry counters and state information
     */
    struct FlowCounters_t {
        bool visited;
        uint32_t age;
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
    typedef std::unordered_map<FlowMatchKey_t, PolicyCounters_t,
                               KeyHasher> PolicyCounterMap_t;
    virtual void on_timer_base(const boost::system::error_code& ec,int index,
        PolicyCounterMap_t& newClassCountersMap);
    virtual void on_timer(const boost::system::error_code& ec) = 0;
    virtual void updateFlowEntryMap(uint64_t cookie, uint16_t priority,
                                            uint8_t index,const struct match& match);
    virtual void updateNewFlowCounters(uint32_t cookie,
                                               uint16_t priority,
                                               struct match& match,
                                               uint64_t flow_packet_count,
                                               uint64_t flow_byte_count,
                                               uint8_t index,
                                               bool flowRemoved);
protected:
    std::unique_ptr<boost::asio::deadline_timer> timer;
    std::mutex pstatMtx;
    IdGenerator& idGen;
    Agent* agent;
    SwitchManager& switchManager;
    SwitchConnection* connection;
    boost::asio::io_service& agent_io;
    long timer_interval;
    std::string agentUUID;
    
    std::atomic<std::uint64_t> clsfrGenId;
    struct FlowKeyHasher {
        /**
         * Hash for FlowEntryMatch Key
         */
        size_t operator()(const FlowEntryMatchKey_t& k) const noexcept;
    };

    /* flow entry counter map */
    typedef std::unordered_map<FlowEntryMatchKey_t, FlowCounters_t,
                               FlowKeyHasher> FlowEntryCounterMap_t;

    FlowEntryCounterMap_t oldFlowCounterMap[2];
    FlowEntryCounterMap_t newFlowCounterMap[2];
    FlowEntryCounterMap_t removedFlowCounterMap[2];


    virtual void handleFlowStats(int msgType,ofpbuf *msg) = 0;
    virtual void handleFlowRemoved(ofpbuf *msg) = 0;
    

    volatile bool stopping;

};

} /* namespace ovsagent */

#endif /* OVSAGENT_BASESTATSMANAGER_H */
