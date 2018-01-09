/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SampleRenderer
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef SAMPLERENDERER_H
#define SAMPLERENDERER_H

#include <opflexagent/Renderer.h>

namespace samplerenderer {

/**
 * The sample renderer demonstrates how to create a renderer plugin
 * for OpFlex agent.
 */
class SampleRenderer : public opflexagent::Renderer {
public:
    /**
     * Instantiate a stitched-mode renderer
     *
     * @param agent the agent object
     */
    SampleRenderer(opflexagent::Agent& agent);

    /**
     * Destroy the renderer and clean up all state
     */
    virtual ~SampleRenderer();

    // ********
    // Renderer
    // ********

    virtual void setProperties(const boost::property_tree::ptree& properties);
    virtual void start();
    virtual void stop();
};

/**
 * Plugin implementation for dynamically loading sample
 * renderer.
 */
class SampleRendererPlugin : public opflexagent::RendererPlugin {
public:
    SampleRendererPlugin();

    // **************
    // RendererPlugin
    // **************
    virtual std::unordered_set<std::string> getNames() const;
    virtual opflexagent::Renderer* create(opflexagent::Agent& agent) const;
};

} /* namespace samplerenderer */

/**
 * Return a non-owning pointer to the renderer plugin instance.
 */
extern "C" const opflexagent::RendererPlugin* init_renderer_plugin();

#endif /* SAMPLERENDERER_H */
