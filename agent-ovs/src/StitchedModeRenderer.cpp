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
      statsManager(&agent_, portMapper), tunnelEpManager(&agent_),
      uplinkVlan(0), virtualRouter(true), started(false) {
    flowManager.SetFlowReader(&flowReader);
    flowManager.SetExecutor(&flowExecutor);
    flowManager.SetPortMapper(&portMapper);
    flowManager.setJsonCmdExecutor(&jsonCmdExecutor);
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
    LOG(INFO) << "Starting stitched-mode renderer on " << ovsBridgeName;

    if (encapType == FlowManager::ENCAP_VXLAN ||
        encapType == FlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.setUplinkIface(uplinkIface);
        tunnelEpManager.setUplinkVlan(uplinkVlan);
        tunnelEpManager.start();
    }

    flowManager.SetFallbackMode(FlowManager::FALLBACK_PROXY);
    flowManager.SetEncapType(encapType);
    flowManager.SetEncapIface(encapIface);
    flowManager.SetFloodScope(FlowManager::ENDPOINT_GROUP);
    if (encapType == FlowManager::ENCAP_VXLAN ||
        encapType == FlowManager::ENCAP_IVXLAN)
        flowManager.SetTunnelRemoteIp(tunnelRemoteIp);
    flowManager.SetVirtualRouter(virtualRouter);
    flowManager.SetVirtualRouterMac(virtualRouterMac);
    flowManager.SetFlowIdCache(flowIdCache);

    connection = new SwitchConnection(ovsBridgeName);
    portMapper.InstallListenersForConnection(connection);
    flowExecutor.InstallListenersForConnection(connection);
    flowReader.installListenersForConnection(connection);
    jsonCmdExecutor.installListenersForConnection(connection);
    flowManager.registerConnection(connection);
    flowManager.registerModbListeners();
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
    flowManager.unregisterModbListeners();
    flowManager.unregisterConnection(connection);
    jsonCmdExecutor.uninstallListenersForConnection(connection);
    flowReader.uninstallListenersForConnection(connection);
    flowExecutor.UninstallListenersForConnection(connection);
    portMapper.UninstallListenersForConnection(connection);
    delete connection;
    connection = NULL;

    if (encapType == FlowManager::ENCAP_VXLAN ||
        encapType == FlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.stop();
    }
}

Renderer* StitchedModeRenderer::create(Agent& agent) {
    return new StitchedModeRenderer(agent);
}

void StitchedModeRenderer::setProperties(const ptree& properties) {
    static const std::string OVS_BRIDGE_NAME("ovs-bridge-name");

    static const std::string ENCAP_VXLAN("encap.vxlan");
    static const std::string ENCAP_IVXLAN("encap.ivxlan");
    static const std::string ENCAP_VLAN("encap.vlan");

    static const std::string UPLINK_IFACE("uplink-iface");
    static const std::string UPLINK_VLAN("uplink-vlan");
    static const std::string ENCAP_IFACE("encap-iface");
    static const std::string REMOTE_IP("remote-ip");

    static const std::string VIRTUAL_ROUTER("forwarding.virtual-router");
    static const std::string VIRTUAL_ROUTER_MAC("forwarding.virtual-router-mac");

    static const std::string FLOWID_CACHE_DIR("flowid-cache-dir");

    ovsBridgeName = properties.get<std::string>(OVS_BRIDGE_NAME, "");

    boost::optional<const ptree&> ivxlan =
        properties.get_child_optional(ENCAP_IVXLAN);
    boost::optional<const ptree&> vxlan =
        properties.get_child_optional(ENCAP_VXLAN);
    boost::optional<const ptree&> vlan =
        properties.get_child_optional(ENCAP_VLAN);

    encapType = FlowManager::ENCAP_NONE;
    int count = 0;
    if (ivxlan) {
        LOG(ERROR) << "Encapsulation type ivxlan unsupported";
        count += 1;
    }
    if (vlan) {
        encapType = FlowManager::ENCAP_VLAN;
        encapIface = vlan.get().get<std::string>(ENCAP_IFACE, "");
        count += 1;
    }
    if (vxlan) {
        encapType = FlowManager::ENCAP_VXLAN;
        encapIface = vxlan.get().get<std::string>(ENCAP_IFACE, "");
        uplinkIface = vxlan.get().get<std::string>(UPLINK_IFACE, "");
        uplinkVlan = vxlan.get().get<uint16_t>(UPLINK_VLAN, 0);
        tunnelRemoteIp = vxlan.get().get<std::string>(REMOTE_IP, "");
        count += 1;
    }

    if (count > 1) {
        LOG(WARNING) << "Multiple encapsulation types specified for "
                     << "stitched-mode renderer";
    }

    virtualRouter = properties.get<bool>(VIRTUAL_ROUTER, true);
    virtualRouterMac =
        properties.get<std::string>(VIRTUAL_ROUTER_MAC, "88:f0:31:b5:12:b5");

    flowIdCache = properties.get<std::string>(FLOWID_CACHE_DIR, "");
}

} /* namespace ovsagent */

