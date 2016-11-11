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
#include "EndpointManager.h"
#include "PortMapper.h"

#pragma once
#ifndef OVSAGENT_STATSMANAGER_H
#define OVSAGENT_STATSMANAGER_H

namespace ovsagent {

class Agent;

/**
 * Periodically query an OpenFlow switch for counters and stats and
 * distribute them as needed to other components for reporting.
 */
class StatsManager : private boost::noncopyable,
                     public MessageHandler {
public:
    /**
     * Instantiate a new stats manager that will use the provided io
     * service for scheduling asynchronous tasks
     * @param agent the agent associated with the stats manager
     * @param portMapper the openflow port mapper to use
     * @param timer_interval the interval for the stats timer in milliseconds
     */
    StatsManager(Agent* agent,
                 PortMapper& portMapper,
                 long timer_interval = 30000);

    /**
     * Destroy the stats manager and clean up all state
     */
    ~StatsManager();

    /**
     * Register the given connection with the stats manager.  This
     * connection will be queried for counters.
     *
     * @param connection the connection to use for stats collection
     */
    void registerConnection(SwitchConnection* connection);

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
    PortMapper& portMapper;
    SwitchConnection* connection;
    boost::asio::io_service& agent_io;
    long timer_interval;
    std::unique_ptr<boost::asio::deadline_timer> timer;

    void on_timer(const boost::system::error_code& ec);

    volatile bool stopping;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_STATSMANAGER_H */
