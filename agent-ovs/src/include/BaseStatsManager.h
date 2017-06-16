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
#define MAX_COUNTER_LIMIT 5
#define MAX_DROP_COUNTER_LIMIT 1000
namespace ovsagent {

class Agent;
class SwitchManager;
/**
 * Periodically query an OpenFlow switch for policy counters and stats
 * and distribute them as needed to other components for reporting.
 */
class BaseStatsManager : private boost::noncopyable,
                           public MessageHandler, public opflex::modb::ObjectListener {
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

    virtual void objectUpdated(opflex::modb::class_id_t class_id,
        const opflex::modb::URI& uri) = 0;
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
    virtual void on_timer_base(const boost::system::error_code& ec,uint8_t table_id,
        PolicyCounterMap_t& newClassCountersMap);
    virtual void on_timer(const boost::system::error_code& ec) = 0;
    virtual void updateFlowEntryMap(uint64_t cookie, uint16_t priority,
                                            uint8_t table_id,const struct match& match);
    virtual void updateNewFlowCounters(uint32_t cookie,
                                               uint16_t priority,
                                               struct match& match,
                                               uint64_t flow_packet_count,
                                               uint64_t flow_byte_count,
                                               uint8_t table_id,
                                               bool flowRemoved);
    void sendRequest(uint32_t table_id);
    virtual bool isTableIdFound(uint8_t table_id) = 0;
    virtual void resolveCounterMaps(uint8_t table_id) = 0;
    virtual void clearCounterObject(const std::string& key,uint8_t index) = 0;
    virtual void clearOldCounters(const std::string& key,uint64_t nextId);

    void removeAllCounterObjects(const std::string& key);
    struct CircularBuffer {
        std::vector<uint64_t> uidList;
        uint64_t count;
        CircularBuffer() {
            count = 0;
        }
    };
protected:
    std::unordered_map<std::string,std::unique_ptr<CircularBuffer>> genIdList_;
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

    struct flowCounterState_t {
        FlowEntryCounterMap_t oldFlowCounterMap;
        FlowEntryCounterMap_t newFlowCounterMap;
        FlowEntryCounterMap_t removedFlowCounterMap;
    };

    FlowEntryCounterMap_t *oldFlowCounterMap;
    FlowEntryCounterMap_t *newFlowCounterMap;
    FlowEntryCounterMap_t *removedFlowCounterMap;


    virtual void handleFlowStats(int msgType,ofpbuf *msg);
    virtual void handleFlowRemoved(ofpbuf *msg);
    void generatePolicyStatsObjects(PolicyCounterMap_t *counters1,PolicyCounterMap_t *counters2 = NULL);
    virtual void updatePolicyStatsCounters(const std::string& l24Classifier,
                          PolicyCounters_t& newVals1,
                          PolicyCounters_t& newVals2) = 0;
   
    virtual void handleDropStats(uint32_t rdId,
                         boost::optional<std::string> idRdStr,
                         struct ofputil_flow_stats* fentry) = 0;
    virtual void updatePolicyStatsCounters(const std::string& srcEpg,
                                   const std::string& dstEpg,
                                   const std::string& ruleURI,
                                   PolicyCounters_t& counters) = 0;
    volatile bool stopping;

};

} /* namespace ovsagent */

#endif /* OVSAGENT_BASESTATSMANAGER_H */
