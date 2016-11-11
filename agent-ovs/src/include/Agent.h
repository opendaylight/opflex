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
#ifndef OVSAGENT_AGENT_H
#define OVSAGENT_AGENT_H

#include "EndpointManager.h"
#include "ServiceManager.h"
#include "ExtraConfigManager.h"
#include "NotifServer.h"
#include "FSWatcher.h"

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/noncopyable.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>

#include <set>
#include <utility>
#include <string>
#include <vector>
#include <thread>

namespace ovsagent {

class Renderer;
class EndpointSource;
class ServiceSource;
class FSRDConfigSource;

/**
 * Master object for the OVS agent.  This class holds the state for
 * the agent and handles initialization, configuration and cleanup.
 */
class Agent : private boost::noncopyable {
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
     * Get the endpoint manager object for this agent
     */
    EndpointManager& getEndpointManager() { return endpointManager; }

    /**
     * Get the service manager object for this agent
     */
    ServiceManager& getServiceManager() { return serviceManager; }

    /**
     * Get the extra config object for this agent
     */
    ExtraConfigManager& getExtraConfigManager() { return extraConfigManager; }

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

private:
    boost::asio::io_service agent_io;

    opflex::ofcore::OFFramework& framework;
    PolicyManager policyManager;
    EndpointManager endpointManager;
    ServiceManager serviceManager;
    ExtraConfigManager extraConfigManager;
    NotifServer notifServer;
    FSWatcher fsWatcher;

    boost::optional<std::string> opflexName;
    boost::optional<std::string> opflexDomain;

    boost::optional<bool> enableInspector;
    boost::optional<std::string> inspectorSock;
    boost::optional<bool> enableNotif;
    boost::optional<std::string> notifSock;
    boost::optional<std::string> notifOwner;
    boost::optional<std::string> notifGroup;
    boost::optional<std::string> notifPerms;

    std::set<std::string> endpointSourcePaths;
    std::vector<EndpointSource*> endpointSources;
    std::vector<FSRDConfigSource*> rdConfigSources;

    std::set<std::string> serviceSourcePaths;
    std::vector<ServiceSource*> serviceSources;

    std::vector<Renderer*> renderers;

    typedef std::pair<std::string, int> host_t;
    std::set<host_t> opflexPeers;
    boost::optional<std::string> sslMode;
    boost::optional<std::string> sslCaStore;

    /**
     * Thread for asynchronous tasks
     */
    std::unique_ptr<std::thread> io_service_thread;

    bool started;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_AGENT_H */
