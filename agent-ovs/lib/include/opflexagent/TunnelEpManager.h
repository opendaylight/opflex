/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/EndpointListener.h>

#include <opflex/ofcore/OFFramework.h>

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include <mutex>

#pragma once
#ifndef OPFLEXAGENT_TUNNELEPMANAGER_H
#define OPFLEXAGENT_TUNNELEPMANAGER_H

namespace opflexagent {

class Agent;
class Renderer;

/**
 * The tunnel endpoint manager creates a tunnel termination endpoint
 * for renderers that require it.  This is the tunnel destination IP
 * that should be used for sending encapsulated traffic to the host.
 */
class TunnelEpManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new tunnelEp manager using the specified framework
     * instance.
     */
    TunnelEpManager(Agent* agent, long timer_interval = 5000);

    /**
     * Destroy the tunnel endpoint manager and clean up all state
     */
    virtual ~TunnelEpManager();

    /**
     * Start the tunnelEp manager
     */
    void start();

    /**
     * Stop the tunnelEp manager
     */
    void stop();

    /**
     * Set the uplink interface name to search for to use as the
     * uplink IP.  If this isn't set or is set to the empty string
     * then it will search all interfaces for an IP to use.
     *
     * @param uplinkIface the interface name to use
     */
    void setUplinkIface(const std::string& uplinkIface) {
        this->uplinkIface = uplinkIface;
    }

    /**
     * Get the uplink interface name
     * @param uplinkInterface sting to populate
     */
    void  getUplinkIface(std::string& uplinkInterface) {
        uplinkInterface = this->uplinkIface;
    }

    /**
     * Set the VLAN tag set on the uplink interface
     *
     * @param uplinkVlan the vlan tag used on the uplink interface
     */
    void setUplinkVlan(uint16_t uplinkVlan) {
        this->uplinkVlan = uplinkVlan;
    }

    void setParentRenderer(Renderer *r) {
        this->renderer = r;
    }

    /**
     * Get the tunnel termination IP address for the tunnel endpoint
     * with the given uuid
     *
     * @param uuid the UUID of the tunnel termination endpoint
     * @throws std::out_of_range if there is no such uuid
     */
    const std::string& getTerminationIp(const std::string& uuid);

    /**
     * Get the mac address for the tunnel endpoint
     * with the given uuid
     *
     * @param uuid the UUID of the tunnel termination endpoint
     * @throws std::out_of_range if there is no such uuid
     */
    const std::string& getTerminationMac(const std::string& uuid);

    /**
     * Register a listener for tunnelEp change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see TunnelEpListener
     */
    void registerListener(EndpointListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(EndpointListener* listener);

    /**
     * Check whether given UUID is of the TunnelEp
     * @param uuid uuid to check
     * @return whether uuid is tunnelEpUUID
     */
    bool isTunnelEp(const std::string &uuid) {
        return (uuid == tunnelEpUUID);
    }

private:
    Agent* agent;
    Renderer* renderer;
    boost::asio::io_service& agent_io;
    long timer_interval;
    std::unique_ptr<boost::asio::deadline_timer> timer;

    void on_timer(const boost::system::error_code& ec);

    volatile bool stopping;

    /**
     * the uplink interface to search
     */
    std::string uplinkIface;

    /**
     * The VLAN tag for the uplink; 0 for no vlan
     */
    uint16_t uplinkVlan;

    /**
     * The uuid of the tunnel termination endpoint
     */
    std::string tunnelEpUUID;

    /**
     * The tunnel termination IP address as discovered from the local
     * system.
     */
    std::string terminationIp;

    /**
     * The tunnel termination MAC address as discovered from the local
     * system.
     */
    std::string terminationMac;

    /**
     * Whether discovered tunnel termination IP is IPv4 address. A value of
     * false indicates IPv6 or unknown address family.
     */
    bool terminationIpIsV4;

    std::mutex termip_mutex;

    /**
     * The endpoint listeners that have been registered
     */
    std::list<EndpointListener*> tunnelEpListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);

    friend class EndpointSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_TUNNELEPMANAGER_H */
