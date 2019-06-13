/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for Renderer
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/property_tree/ptree.hpp>
#include <opflex/ofcore/OFFramework.h>

#include <opflexagent/Agent.h>

#pragma once
#ifndef OPFLEXAGENT_RENDERER_H
#define OPFLEXAGENT_RENDERER_H

namespace opflexagent {

/**
 * Abstract base type for a renderer.  A renderer is the component
 * that actually enforces the policy.  For example, a renderer might
 * enforce the policy by writing OpenFlow rules to a local bridge, by
 * creating ACLs and route entries on a router, or other means.
 */
class Renderer : private boost::noncopyable {
public:
    /**
     * Instantiate a renderer
     *
     * @param agent the agent object
     */
    Renderer(Agent& agent);

    /**
     * Destroy the renderer and clean up all state
     */
    virtual ~Renderer();

    /**
     * Configure the renderer with the property tree specified, which
     * will be a subtree in the base agent configuration
     *
     * @param properties the configuration properties to set for the
     * agent
     */
    virtual void setProperties(const boost::property_tree::ptree& properties) = 0;

    /**
     * Start the renderer
     */
    virtual void start() = 0;

    /**
     * Stop the renderer
     */
    virtual void stop() = 0;

    /**
     * Is uplink address owned by renderer
     */
    virtual bool isUplinkAddressImplemented() {
        return false;
    }

    /**
     * Get uplink address from renderer
     */
    virtual boost::asio::ip::address getUplinkAddress() {
        boost::asio::ip::address addr;
        return addr;
    }

    /**
     * Get uplink mac from renderer
     */
    virtual std::string getUplinkMac() {
        std::string mac;
        return mac;
    }

    /**
     * Get the Agent object
     */
    Agent& getAgent() { return agent; }

    /**
     * Get the opflex framework object for this renderer
     */
    opflex::ofcore::OFFramework& getFramework() { return agent.getFramework(); }

    /**
     * Get the policy manager object for this renderer
     */
    PolicyManager& getPolicyManager() { return agent.getPolicyManager(); }

    /**
     * Get the endpoint manager object for this renderer
     */
    EndpointManager& getEndpointManager() { return agent.getEndpointManager(); }

private:
    Agent& agent;
};

/**
 * A dynamically loaded renderer plugin
 */
class RendererPlugin {
public:
    /**
     * Get a set of name aliases for the renderer.  All names must be
     * unique across all loaded renderers.
     */
    virtual std::unordered_set<std::string> getNames() const = 0;

    /**
     * Allocate a new renderer on the heap
     *
     * @return a pointer to the renderer.  The memory is owned by the
     * caller
     */
    virtual Renderer* create(Agent& agent) const = 0;
};

/**
 * typedef for renderer plugin init function.  Must be defined with
 * this type and named "init_renderer_plugin"
 */
typedef const RendererPlugin* (*renderer_init_func)();

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_RENDERER_H */
