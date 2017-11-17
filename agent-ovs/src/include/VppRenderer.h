/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for Renderer
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "IdGenerator.h"
#include "Renderer.h"
#include "VppInspect.h"
#include "VppManager.h"
#include <boost/property_tree/ptree.hpp>
#include <opflex/ofcore/OFFramework.h>

#include <vom/hw.hpp>

#pragma once
#ifndef OVSAGENT_VPP_RENDERER_H
#define OVSAGENT_VPP_RENDERER_H

namespace ovsagent {

/**
 * Vpp renderer is the component that actually enforces the policy.
 * For example, a renderer might enforce the policy to a local bridge, by
 * creating ACLs and route entries in Vpp.
 */
class VppRenderer : public Renderer {
public:
    /**
     * Instantiate a vpp renderer
     *
     * @param agent the agent object
     */
    VppRenderer(Agent& agent);

    /**
     * Destroy the renderer and clean up all state
     */
    virtual ~VppRenderer();

    /**
     * Allocate a vpp renderer on the heap
     *
     * @return a pointer to the renderer.  The memory is owned by the
     * caller
     */
    static Renderer* create(Agent& agent);

    /**
     * Configure the renderer with the property tree specified, which
     * will be a subtree in the base agent configuration
     *
     * @param properties the configuration properties to set for the
     * agent
     */
    virtual void setProperties(const boost::property_tree::ptree& properties);

    /**
     * Start the renderer
     */
    virtual void start();

    /**
     * Stop the renderer
     */
    virtual void stop();

private:
    /**
     * The socket used for inspecting the state built in VPP-manager
     */
    std::unique_ptr<VppInspect> inspector;

    /**
     * ID generator
     */
    IdGenerator idGen;

    /**
     * HW cmd_q
     */
    VOM::HW::cmd_q vppQ;

    /**
     * Single instance of the VPP manager
     */
    VppManager vppManager;

    /**
     * has this party started.
     */
    bool started;

};

} /* namespace ovsagent */

/*
 * Local Variables:
 * eval: (c-set-style "llvm.org")
 * End:
 */

#endif /* OVSAGENT_VPP_RENDERER_H */
