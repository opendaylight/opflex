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
      tunnelRemotePort(0), uplinkVlan(0),
      virtualRouter(true), routerAdv(true), started(false) {
    flowManager.SetFlowReader(&flowReader);
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
        encapType == FlowManager::ENCAP_IVXLAN) {
        flowManager.SetTunnelRemoteIp(tunnelRemoteIp);
        assert(tunnelRemotePort != 0);
        flowManager.setTunnelRemotePort(tunnelRemotePort);
    }
    flowManager.SetVirtualRouter(virtualRouter, routerAdv, virtualRouterMac);
    flowManager.SetVirtualDHCP(virtualDHCP, virtualDHCPMac);
    flowManager.SetFlowIdCache(flowIdCache);
    flowManager.SetMulticastGroupFile(mcastGroupFile);
    flowManager.SetEndpointAdv(endpointAdv);

    connection = new SwitchConnection(ovsBridgeName);
    portMapper.InstallListenersForConnection(connection);
    flowExecutor.InstallListenersForConnection(connection);
    flowReader.installListenersForConnection(connection);
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

#define DEF_FLOWID_CACHEDIR \
    LOCALSTATEDIR"/lib/opflex-agent-ovs/ids"
#define DEF_MCAST_GROUPFILE \
    LOCALSTATEDIR"/lib/opflex-agent-ovs/mcast/opflex-groups.json"

void StitchedModeRenderer::setProperties(const ptree& properties) {
    static const std::string OVS_BRIDGE_NAME("ovs-bridge-name");

    static const std::string ENCAP_VXLAN("encap.vxlan");
    static const std::string ENCAP_IVXLAN("encap.ivxlan");
    static const std::string ENCAP_VLAN("encap.vlan");

    static const std::string UPLINK_IFACE("uplink-iface");
    static const std::string UPLINK_VLAN("uplink-vlan");
    static const std::string ENCAP_IFACE("encap-iface");
    static const std::string REMOTE_IP("remote-ip");
    static const std::string REMOTE_PORT("remote-port");

    static const std::string VIRTUAL_ROUTER("forwarding.virtual-router.enabled");
    static const std::string VIRTUAL_ROUTER_MAC("forwarding.virtual-router.mac");

    static const std::string VIRTUAL_ROUTER_RA("forwarding.virtual-router.ipv6.router-advertisement");

    static const std::string VIRTUAL_DHCP("forwarding.virtual-dhcp.enabled");
    static const std::string VIRTUAL_DHCP_MAC("forwarding.virtual-dhcp.mac");

    static const std::string ENDPOINT_ADV("forwarding.endpoint-advertisements.enabled");

    static const std::string FLOWID_CACHE_DIR("flowid-cache-dir");
    static const std::string MCAST_GROUP_FILE("mcast-group-file");

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
        tunnelRemotePort = vxlan.get().get<uint16_t>(REMOTE_PORT, 4789);
        count += 1;
    }

    if (count > 1) {
        LOG(WARNING) << "Multiple encapsulation types specified for "
                     << "stitched-mode renderer";
    } else if (count == 0) {
        LOG(WARNING)
            << "No encapsulation types specified; only local traffic will work";
    }

    virtualRouter = properties.get<bool>(VIRTUAL_ROUTER, true);
    virtualRouterMac =
        properties.get<std::string>(VIRTUAL_ROUTER_MAC, "00:22:bd:f8:19:ff");
    routerAdv = properties.get<bool>(VIRTUAL_ROUTER_RA, false);
    virtualDHCP = properties.get<bool>(VIRTUAL_DHCP, true);
    virtualDHCPMac =
        properties.get<std::string>(VIRTUAL_DHCP_MAC, "00:22:bd:f8:19:ff");
    endpointAdv = properties.get<bool>(ENDPOINT_ADV, true);

    flowIdCache = properties.get<std::string>(FLOWID_CACHE_DIR,
                                              DEF_FLOWID_CACHEDIR);
    if (flowIdCache == "")
        LOG(WARNING) << "No flow ID cache directory specified";

    mcastGroupFile = properties.get<std::string>(MCAST_GROUP_FILE,
                                                 DEF_MCAST_GROUPFILE);
    if (mcastGroupFile == "")
        LOG(WARNING) << "No multicast group file specified";
}

} /* namespace ovsagent */

