/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for AdvertManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_ADVERTMANAGER_H
#define OVSAGENT_ADVERTMANAGER_H

#include <boost/noncopyable.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/scoped_ptr.hpp>

#include "Agent.h"
#include "PortMapper.h"

namespace ovsagent {

class FlowManager;

/**
 * Class that handles generating all unsolicited advertisements with
 * packet-out.  This includes router advertisements, and gratuituous
 * and/or reverse ARP/NDP.
 */
class AdvertManager : private boost::noncopyable {
public:
    /**
     * Construct a AdvertManager
     */
    AdvertManager(Agent& agent, FlowManager& flowManager);

    /**
     * Set the port mapper to use
     * @param m the port mapper
     */
    void setPortMapper(PortMapper* m) { portMapper = m; }

    /**
     * Set the IO service to use
     * @param io the IO service
     */
    void setIOService(boost::asio::io_service* io) { ioService = io; }

    /**
     * Set the switch connection to use
     * @param c the switch connection
     */
    void registerConnection(SwitchConnection* c) { switchConnection = c; }

    /**
     * Unset the switch connection
     */
    void unregisterConnection() { switchConnection = NULL; }

    /**
     * Enable router advertisements
     */
    void enableRouterAdv(bool enabled) { sendRouterAdv = enabled; }

    /**
     * Enable gratuitous endpoint advertisements
     */
    void enableEndpointAdv(bool enabled) { sendEndpointAdv = enabled; }

    /**
     * Module start
     */
    void start();

    /**
     * Module stop
     */
    void stop();

    /**
     * Schedule a round of initial router advertisements
     */
    void scheduleInitialRouterAdv();

    /**
     * Schedule a round of initial endpoint advertisements
     */
    void scheduleInitialEndpointAdv();

    /**
     * Schedule endpoint advertisements for a specific endpoint
     *
     * @param uuid the uuid of the endpoint
     */
    void scheduleEndpointAdv(const std::string& uuid);

private:
    boost::random::random_device rng;
    boost::random::mt19937 urng;
    boost::random::variate_generator<boost::random::mt19937&,
                                     boost::random::uniform_int_distribution<> > gen;

    /**
     * Synchronously send router advertisements for all active virtual
     * routers
     */
    void sendRouterAdvs();

    /**
     * Timer callback for router advertisements
     */
    bool sendRouterAdv;
    void onRouterAdvTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> routerAdvTimer;
    volatile int initialRouterAdvs;

    /**
     * Synchronously send endpoint gratuitous advertisements for the
     * specified endpoint
     *
     * @param uuid the UUID of the endpoint
     */
    void sendEndpointAdvs(const std::string& uuid);

    /**
     * Synchronously send endpoint gratuitous advertisements for all
     * active endpoints.
     */
    void sendAllEndpointAdvs();

    /**
     * Timer callback for gratuitious endpoint advertisements
     */
    bool sendEndpointAdv;
    void onEndpointAdvTimer(const boost::system::error_code& ec);
    void onAllEndpointAdvTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> endpointAdvTimer;
    boost::scoped_ptr<boost::asio::deadline_timer> allEndpointAdvTimer;
    boost::mutex ep_mutex;
    boost::unordered_set<std::string> pendingEps;

    Agent& agent;
    FlowManager& flowManager;
    PortMapper* portMapper;
    SwitchConnection* switchConnection;
    boost::asio::io_service* ioService;

    bool started;
    bool stopping;
};

}
#endif /* OVSAGENT_ADVERTMANAGER_H */
