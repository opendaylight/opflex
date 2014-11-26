/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for StitchedModeRenderer class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "StitchedModeRenderer.h"
#include "logging.h"

namespace ovsagent {

using opflex::ofcore::OFFramework;
using boost::property_tree::ptree;

StitchedModeRenderer::StitchedModeRenderer(Agent& agent_) 
    : Renderer(agent_), flowManager(agent_), connection(NULL), 
      statsManager(&agent_, portMapper), started(false) {
    flowManager.SetExecutor(&flowExecutor);
    flowManager.SetPortMapper(&portMapper);
}

StitchedModeRenderer::~StitchedModeRenderer() {
    if (connection)
        delete connection;
}

void StitchedModeRenderer::start() {
    if (started) return;

    if (ovsBridgeName == "") {
        LOG(ERROR) << "OVS Bridge name not set";
        return;
    }

    started = true;
    flowManager.SetTunnelType(tunnelType);
    flowManager.SetTunnelIface(tunnelIface);
    flowManager.SetTunnelRemoteIp(tunnelRemoteIp);

    LOG(INFO) << "Starting stitched-mode renderer on " << ovsBridgeName;
    connection = new opflex::enforcer::SwitchConnection(ovsBridgeName);
    portMapper.InstallListenersForConnection(connection);
    flowExecutor.InstallListenersForConnection(connection);
    connection->Connect(OFP13_VERSION);
    flowManager.Start();
    statsManager.registerConnection(connection);
    statsManager.start();
}

void StitchedModeRenderer::stop() {
    if (!started) return;
    started = false;

    LOG(DEBUG) << "Stopping stitched-mode renderer";

    statsManager.stop();
    flowManager.Stop();
    connection->Disconnect();
    flowExecutor.UninstallListenersForConnection(connection);
    portMapper.UninstallListenersForConnection(connection);
    delete connection;
    connection = NULL;
}

Renderer* StitchedModeRenderer::create(Agent& agent) {
    return new StitchedModeRenderer(agent);
}

void StitchedModeRenderer::setProperties(const ptree& properties) {
    static const std::string OVS_BRIDGE_NAME("ovs-bridge-name");
    static const std::string TUNNEL_VXLAN("tunnel.vxlan");
    static const std::string IFACE("iface");
    static const std::string REMOTE_IP("remote-ip");

    ovsBridgeName = properties.get<std::string>(OVS_BRIDGE_NAME, "");

    boost::optional<const ptree&> vxlan = 
        properties.get_child_optional(TUNNEL_VXLAN);
    if (vxlan) {
        tunnelType = "vxlan";
        tunnelIface = vxlan.get().get<std::string>(IFACE, "");
        tunnelRemoteIp = vxlan.get().get<std::string>(REMOTE_IP, "");
    }
}

} /* namespace ovsagent */

