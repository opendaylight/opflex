/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for Agent
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_AGENT_H
#define OPFLEXAGENT_AGENT_H

#include <opflexagent/EndpointManager.h>
#include <opflexagent/ServiceManager.h>
#include <opflexagent/ExtraConfigManager.h>
#include <opflexagent/LearningBridgeManager.h>
#include <opflexagent/NotifServer.h>
#include <opflexagent/FSWatcher.h>
#include <opflexagent/SpanManager.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/noncopyable.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/ofcore/OFConstants.h>
#include <modelgbp/metadata/metadata.hpp>

#include <set>
#include <utility>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>

namespace opflexagent {

class Renderer;
class RendererPlugin;
class EndpointSource;
class ServiceSource;
class FSRDConfigSource;
class LearningBridgeSource;
class SimStats;

enum StatMode { REAL, SIM, OFF };

/**
 * Master object for the OVS agent.  This class holds the state for
 * the agent and handles initialization, configuration and cleanup.
 */
class Agent : private boost::noncopyable {
typedef opflex::ofcore::OFConstants::OpflexElementMode opflex_elem_t;
public:
    /**
     * Instantiate a new agent using the specified framework
     * instance.
     *
     * @param framework the framework instance to use
     */
    Agent(opflex::ofcore::OFFramework& framework);

    /**
     * Destroy the agent and clean up all state
     */
    ~Agent();

    /**
     * Configure the agent with the property tree specified
     *
     * @param properties the configuration properties to set for the
     * agent
     */
    void setProperties(const boost::property_tree::ptree& properties);

    /**
     * Apply the properties set with setProperties to the agent
     * configuration
     *
     * @throws std::runtime_error if the configuration is invalid
     */
    void applyProperties();

    /**
     * Start the agent
     */
    void start();

    /**
     * Stop the agent
     */
    void stop();

    /**
     * Get the opflex framework object for this agent
     */
    opflex::ofcore::OFFramework& getFramework() { return framework; }

    /**
     * Get the policy manager object for this agent
     */
    PolicyManager& getPolicyManager() { return policyManager; }

    /**
     * Get the span manager object for this agent
     */
    SpanManager& getSpanManager() { return spanManager; }

    /**
     * Get the endpoint manager object for this agent
     */
    EndpointManager& getEndpointManager() { return endpointManager; }

    /**
     * Get the service manager object for this agent
     */
    ServiceManager& getServiceManager() { return serviceManager; }

    /**
     * Get the extra config manager object for this agent
     */
    ExtraConfigManager& getExtraConfigManager() { return extraConfigManager; }

    /**
     * Get renderer forwarding mode for this agent
     */
    uint8_t getRendererForwardingMode() { return rendererFwdMode; }
    /**
     * Set renderer forwarding mode for this agent
     */
     bool setRendererForwardingMode(opflex_elem_t elemMode)
     {   if(started)
            return false;
         presetFwdMode = elemMode;
         if(elemMode != opflex_elem_t::INVALID_MODE) {
            rendererFwdMode = presetFwdMode;
         }
         return true;
     }
    /**
     * Get Proxy addresses for transport mode
     */
    void getV4Proxy(boost::asio::ip::address_v4 &v4ProxyAddress ) {
        framework.getV4Proxy(v4ProxyAddress);
    }

    /**
     * Get V6 Proxy addresses for transport mode
     */
    void getV6Proxy(boost::asio::ip::address_v4 &v6ProxyAddress ) {
        framework.getV6Proxy(v6ProxyAddress);
    }

    /**
     * Get MAC Proxy addresses for transport mode
     */
    void getMacProxy(boost::asio::ip::address_v4 &macProxyAddress ) {
        framework.getMacProxy(macProxyAddress);
    }

    /**
     * Get the learning bridge manager object for this agent
     */
    LearningBridgeManager& getLearningBridgeManager() {
        return learningBridgeManager;
    }

    /**
     * Get the notification server object for this agent
     */
    NotifServer& getNotifServer() { return notifServer; }

    /**
     * Get the ASIO service for the agent for scheduling asynchronous
     * tasks in the io service thread.  You must schedule your async
     * tasks in your start() method and close them (possibly
     * asynchronously) in your stop() method.
     *
     * @return the asio io service
     */
    boost::asio::io_service& getAgentIOService() { return agent_io; }

    /**
     * Get a unique identifer for the agent incarnation
     */
    const std::string& getUuid() { return uuid; }
    /**
     * Get contract counter timer interval.
     * returns millisesonds.
     */
    long getContractInterval() { return contractInterval; }
    /**
     * Set contract counter timer in  milliseconds.
     * A 0 setting means timer is turned off.
     * @param  interval - time in milliseconds
     */
    void setContractInterval(const long interval) { contractInterval = interval; }
    /**
     * Get security group counter timer interval.
     * returns milliseconds.
     */
    long getSecurityGroupInterval() { return securityGroupInterval; }
    /**
     * Set security group counter timer in milliseconds.
     * A 0 setting means timer is turned off.
     * @param  interval - time in milliseconds
     */
    void setSecurityGroupInterval(const long interval) { securityGroupInterval = interval; }
    /**
     * Get interface counter timer interval.
     * returns milliseconds.
     */
    long getInterfaceInterval() { return interfaceInterval; }
    /**
     * Set interface counter timer in milliseconds.
     * A 0 setting means timer is turned off.
     * @param  interval - time in milliseconds
     */
    void setInterfaceInterval(const long interval) { interfaceInterval = interval; }
    /**
     * A class used as a POD (Plain Old Data) object
     * to pass counter settings around.
     */
    class StatProps {
        public:
        /**
         * setting to enable/disable counter timer.
         */
        bool enabled;
        /**
         * setting for timer interval.
         */
        long interval;
    };

private:
    boost::asio::io_service agent_io;
    std::unique_ptr<boost::asio::io_service::work> io_work;

    opflex::ofcore::OFFramework& framework;
    PolicyManager policyManager;
    EndpointManager endpointManager;
    ServiceManager serviceManager;
    ExtraConfigManager extraConfigManager;
    LearningBridgeManager learningBridgeManager;
    NotifServer notifServer;
    FSWatcher fsWatcher;
    opflex_elem_t rendererFwdMode;

    boost::optional<std::string> opflexName;
    boost::optional<std::string> opflexDomain;

    boost::optional<bool> enableInspector;
    boost::optional<std::string> inspectorSock;
    boost::optional<bool> enableNotif;
    boost::optional<std::string> notifSock;
    boost::optional<std::string> notifOwner;
    boost::optional<std::string> notifGroup;
    boost::optional<std::string> notifPerms;
    // stats simulation
    StatMode statMode = StatMode::REAL;
    std::unique_ptr<SimStats> pSimStats;

    // timers
    // prr timer - policy resolve request timer
    boost::uint_t<64>::fast prr_timer = 3600;  /* seconds */

    std::set<std::string> endpointSourceFSPaths;
    std::set<std::string> endpointSourceModelLocalNames;
    std::vector<std::unique_ptr<EndpointSource>> endpointSources;
    std::vector<std::unique_ptr<FSRDConfigSource>> rdConfigSources;
    std::vector<std::unique_ptr<LearningBridgeSource>> learningBridgeSources;

    std::set<std::string> serviceSourcePaths;
    std::vector<std::unique_ptr<ServiceSource>> serviceSources;

    std::unordered_set<std::string> rendPluginLibs;
    std::unordered_set<void*> rendPluginHandles;
    typedef std::unordered_map<std::string, const RendererPlugin*> rend_map_t;
    rend_map_t rendPlugins;
    std::unordered_map<std::string, std::unique_ptr<Renderer>> renderers;

    typedef std::pair<std::string, int> host_t;
    std::set<host_t> opflexPeers;
    boost::optional<std::string> sslMode;
    boost::optional<std::string> sslCaStore;
    boost::optional<std::string> sslClientCert;
    boost::optional<std::string> sslClientCertPass;

    /**
     * Thread for asynchronous tasks
     */
    std::unique_ptr<std::thread> io_service_thread;

    bool started;
    opflex_elem_t presetFwdMode;

    void loadPlugin(const std::string& name);

    std::string uuid;

    StatMode getStatModeFromString(const std::string& mode);
    int setInterval(int& upd_interval);

    void setSimStatProperties(const std::string& enabled_prop,
                              const std::string& interval_prop,
                              const boost::property_tree::ptree& properties,
                              Agent::StatProps& props);
    long contractInterval;
    long securityGroupInterval;
    long interfaceInterval;

    SpanManager spanManager;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_AGENT_H */
