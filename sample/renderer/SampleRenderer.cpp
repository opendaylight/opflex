/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SampleRenderer class
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/logging.h>

#include "SampleRenderer.h"

namespace samplerenderer {

SampleRendererPlugin::SampleRendererPlugin() {
}


std::unordered_set<std::string> SampleRendererPlugin::getNames() const {
    return {"sample"};
}

opflexagent::Renderer*
SampleRendererPlugin::create(opflexagent::Agent& agent) const {
    return new SampleRenderer(agent);
}

SampleRenderer::SampleRenderer(opflexagent::Agent& agent)
    : opflexagent::Renderer(agent) {
}

SampleRenderer::~SampleRenderer() {
}

void SampleRenderer::
setProperties(const boost::property_tree::ptree& properties) {
    // Set configuration from property tree.  This configuration will
    // be from a "renderers": { "sample" { } } block from the agent
    // configuration.  Multiple calls are possible; later calls are
    // merged with prior calls, overwriting any previously-set values.
    LOG(opflexagent::INFO) << "Setting configuration for sample renderer";
}

void SampleRenderer::start() {
    // Called during agent startup
    LOG(opflexagent::INFO) << "Starting sample renderer plugin";
}

void SampleRenderer::stop() {
    // Called during agent shutdown
    LOG(opflexagent::INFO) << "Stopping sample renderer plugin";
}

} /* namespace samplerenderer */

extern "C" const opflexagent::RendererPlugin* init_renderer_plugin() {
    // Return a plugin implementation, which can ini
    static const samplerenderer::SampleRendererPlugin samplePlugin;

    return &samplePlugin;
}
