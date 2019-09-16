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

#ifndef OPFLEXAGENT_ADVERTMANAGER_H
#define OPFLEXAGENT_ADVERTMANAGER_H

#include <opflexagent/Agent.h>
#include "PortMapper.h"

#include <boost/noncopyable.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <mutex>
#include <random>

namespace opflexagent {

class IntFlowManager;

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
    AdvertManager(Agent& agent, IntFlowManager& intFlowManager);

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
     * Enable router advertisements
     */
    void enableRouterAdv(bool enabled) { sendRouterAdv = enabled; }

    /**
     * Modes for sending endpoint advertisements
     */
    enum EndpointAdvMode {
        /**
         * Disable sending endpoint advertisements
         */
        EPADV_DISABLED,
        /**
         * Send gratuitous endpoint advertisements as unicast packets
         * to the router mac.
         */
        EPADV_GRATUITOUS_UNICAST,
        /**
         * Broadcast gratuitous endpoint advertisements.
         */
        EPADV_GRATUITOUS_BROADCAST,
        /**
         * Unicast a spoofed request/solicitation for the subnet's
         * gateway router.
         */
        EPADV_ROUTER_REQUEST,
        /**
         * Broadcast RARP endpoint advertisements. Only for TunnelEp
         */
        EPADV_RARP_BROADCAST
    };

    /**
     * Enable gratuitous endpoint advertisements
     */
    void enableEndpointAdv(EndpointAdvMode mode) { sendEndpointAdv = mode; }

    void enableTunnelEndpointAdv(EndpointAdvMode tunnelMode,
            uint64_t delay = 300)
    { tunnelEndpointAdv = tunnelMode;
      tunnelEpAdvInterval = delay;}

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
     *
     * @param delay number of milliseconds to delay before sending
     * advertisements
     */
    void scheduleInitialEndpointAdv(uint64_t delay = 10000);

    /**
     * Schedule endpoint advertisements for a specific endpoint
     *
     * @param uuid the uuid of the endpoint
     */
    void scheduleEndpointAdv(const std::string& uuid);

    /**
     * Schedule endpoint advertisements for a set of endpoints
     *
     * @param uuids the uuids of the endpoints
     */
    void scheduleEndpointAdv(const std::unordered_set<std::string>& uuids);

    /**
     * Schedule service advertisements for a specific service
     *
     * @param uuid the uuid of the service
     */
    void scheduleServiceAdv(const std::string& uuid);

    /**
     * Schedule tunnelEp RARP advertisement
     *
     * @param uuid the uuid of the tunnelEp
     */
    void scheduleTunnelEpAdv(const std::string& uuid);

private:
    std::random_device rng;
    std::mt19937 urng;
    std::uniform_int_distribution<> all_ep_dis;
    std::uniform_int_distribution<> repeat_dis;

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
    std::unique_ptr<boost::asio::deadline_timer> routerAdvTimer;
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
     * Synchronously send service gratuitous advertisements for all
     * active endpoints.
     */
    void sendAllServiceAdvs();

    /**
     * Synchronously send service advertisements for service endpoints
     *
     * @param uuid the UUID of the service
     */
    void sendServiceAdvs(const std::string& uuid);

    /**
     * Synchronously send advertisements for tunnel endpoints
     *
     * @param uuid the UUID of the tunnel ep
     */
    void sendTunnelEpAdvs(const std::string& uuid);

    /**
     * Timer callback for gratuitious endpoint advertisements
     */
    EndpointAdvMode sendEndpointAdv;
    EndpointAdvMode tunnelEndpointAdv;
    uint64_t tunnelEpAdvInterval;
    void onEndpointAdvTimer(const boost::system::error_code& ec);
    void onAllEndpointAdvTimer(const boost::system::error_code& ec);
    void doScheduleEpAdv(uint64_t time = 250);
    void onTunnelEpAdvTimer(const boost::system::error_code& ec);
    void doScheduleTunnelEpAdv(uint64_t time = 1);
    std::unique_ptr<boost::asio::deadline_timer> endpointAdvTimer;
    std::unique_ptr<boost::asio::deadline_timer> allEndpointAdvTimer;
    std::unique_ptr<boost::asio::deadline_timer> tunnelEpAdvTimer;
    std::mutex ep_mutex;
    std::mutex tunnelep_mutex;
    typedef std::unordered_map<std::string, uint8_t> pending_ep_map_t;
    pending_ep_map_t pendingEps;
    pending_ep_map_t pendingServices;
    pending_ep_map_t pendingTunnelEps;

    Agent& agent;
    IntFlowManager& intFlowManager;
    PortMapper* portMapper;
    SwitchConnection* switchConnection;
    boost::asio::io_service* ioService;

    bool started;
    bool stopping;
};

}
#endif /* OPFLEXAGENT_ADVERTMANAGER_H */
