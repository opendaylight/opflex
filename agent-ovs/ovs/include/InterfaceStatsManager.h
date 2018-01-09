/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for stats manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/asio.hpp>

#include "SwitchConnection.h"
#include <opflexagent/EndpointManager.h>
#include "PortMapper.h"

#include <string>
#include <unordered_map>
#include <mutex>

#pragma once
#ifndef OPFLEXAGENT_STATSMANAGER_H
#define OPFLEXAGENT_STATSMANAGER_H

namespace opflexagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for counters and stats and
 * distribute them as needed to other components for reporting.
 */
class InterfaceStatsManager : private boost::noncopyable,
                              public EndpointListener,
                              public MessageHandler {
public:
    /**
     * Instantiate a new stats manager that will use the provided io
     * service for scheduling asynchronous tasks
     *
     * @param agent the agent associated with the stats manager
     * @param intPortMapper the integration bridge port mapper
     * @param accessPortMapper the access bridge port mapper
     * @param timer_interval the interval for the stats timer in
     * milliseconds
     */
    InterfaceStatsManager(Agent* agent,
                          PortMapper& intPortMapper,
                          PortMapper& accessPortMapper,
                          long timer_interval = 30000);

    /**
     * Destroy the stats manager and clean up all state
     */
    ~InterfaceStatsManager();

    /**
     * Call back for endpoint deletion detection.
     * Corresponding stats that are not yet reported may be deleted here
     *
     * @param  uuid of the endpoint
     */
    void endpointUpdated(const std::string& uuid);
    /**
     * Register the connections with the stats manager.  This
     * connection will be queried for counters.
     *
     * @param intConnection the connection to use for integration
     * bridge stats collection
     * @param accessConnection the connection to use for access bridge
     * stats collection
     */
    void registerConnection(SwitchConnection* intConnection,
                            SwitchConnection *accessConnection);


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
    PortMapper& intPortMapper;
    PortMapper& accessPortMapper;
    SwitchConnection* intConnection;
    SwitchConnection* accessConnection;
    boost::asio::io_service& agent_io;
    long timer_interval;
    std::unique_ptr<boost::asio::deadline_timer> timer;

    /**
     * Counters for endpoints.
     */
    struct IntfCounters {
        boost::optional<EndpointManager::EpCounters> intCounters;
        boost::optional<EndpointManager::EpCounters> accessCounters;
    };
    typedef std::unordered_map<std::string, IntfCounters> intf_counter_map_t;

    intf_counter_map_t intfCounterMap;
    std::mutex statMtx;

    void on_timer(const boost::system::error_code& ec);
    void updateEndpointCounters(const std::string& uuid,
                                SwitchConnection *swConn,
                                EndpointManager::EpCounters& counters);

    volatile bool stopping;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_STATSMANAGER_H */
