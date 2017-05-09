/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "VppRenderer.h"
#include "logging.h"

#include <boost/asio/placeholders.hpp>
#include <boost/algorithm/string/split.hpp>

namespace ovsagent {

    using std::bind;
    using opflex::ofcore::OFFramework;
    using boost::property_tree::ptree;
    using boost::asio::placeholders::error;

    VppRenderer::VppRenderer(Agent& agent_):
        Renderer(agent_),
        vppManager(agent_, idGen),
        started(false) {
        LOG(INFO) << "Vpp Renderer";
    }

    VppRenderer::~VppRenderer() {

    }

    void VppRenderer::start() {
        if (started) return;
        started = true;
        vppManager.start(bridgeName);
        vppManager.registerModbListeners();
        LOG(INFO) << "Starting vpp renderer using";
    }

    void VppRenderer::stop() {
        if (!started) return;
        started = false;
        LOG(DEBUG) << "Stopping vpp renderer";

        vppManager.stop();
    }

    Renderer* VppRenderer::create(Agent& agent) {
        return new VppRenderer(agent);
    }

    void VppRenderer::setProperties(const ptree& properties) {
        static const std::string VPP_BRIDGE_NAME("opflex-vpp");
        bridgeName = properties.get<std::string>(VPP_BRIDGE_NAME, "vpp");

        LOG(INFO) << "Bridge Name " << bridgeName;
    }

}
