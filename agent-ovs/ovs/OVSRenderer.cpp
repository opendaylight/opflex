/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OVSRenderer class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "OVSRenderer.h"
#include <opflexagent/logging.h>

#include <boost/asio/placeholders.hpp>
#include <boost/algorithm/string/split.hpp>
#include <openvswitch/vlog.h>

namespace opflexagent {

using std::bind;
using opflex::ofcore::OFFramework;
using boost::property_tree::ptree;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;

static const std::string ID_NMSPC_CONNTRACK("conntrack");
static const boost::posix_time::milliseconds CLEANUP_INTERVAL(3*60*1000);

OVSRendererPlugin::OVSRendererPlugin() {
    /* No good way to redirect OVS logs to our logs, suppress them for now */
    vlog_set_levels(NULL, VLF_ANY_DESTINATION, VLL_OFF);
}

std::unordered_set<std::string> OVSRendererPlugin::getNames() const {
    return {"stitched-mode", "openvswitch"};
}

Renderer* OVSRendererPlugin::create(Agent& agent) const {
    return new OVSRenderer(agent);
}

OVSRenderer::OVSRenderer(Agent& agent_)
    : Renderer(agent_), ctZoneManager(idGen),
      intSwitchManager(agent_, intFlowExecutor, intFlowReader, intPortMapper),
      intFlowManager(agent_, intSwitchManager, idGen,
                     ctZoneManager, pktInHandler, tunnelEpManager),
      accessSwitchManager(agent_, accessFlowExecutor,
                          accessFlowReader, accessPortMapper),
      accessFlowManager(agent_, accessSwitchManager, idGen, ctZoneManager),
      pktInHandler(agent_, intFlowManager),
      interfaceStatsManager(&agent_, intSwitchManager.getPortMapper(),
                            accessSwitchManager.getPortMapper()),
      contractStatsManager(&agent_, idGen, intSwitchManager),
      secGrpStatsManager(&agent_, idGen, accessSwitchManager),
      tunnelEpManager(&agent_), tunnelRemotePort(0), uplinkVlan(0),
      virtualRouter(true), routerAdv(true),
      connTrack(true), ctZoneRangeStart(0), ctZoneRangeEnd(0),
      ifaceStatsEnabled(true), ifaceStatsInterval(0),
      contractStatsEnabled(true), contractStatsInterval(0),
      secGroupStatsEnabled(true), secGroupStatsInterval(0),
      spanRenderer(agent_),
      started(false) {

}

OVSRenderer::~OVSRenderer() {

}

void OVSRenderer::start() {
    if (started) return;

    if (intBridgeName == "") {
        LOG(ERROR) << "OVS integration bridge name not set";
        return;
    }

    if (encapType == IntFlowManager::ENCAP_NONE)
        LOG(WARNING)
            << "No encapsulation type specified; only local traffic will work";
    if (flowIdCache == "")
        LOG(WARNING) << "No flow ID cache directory specified";
    if (mcastGroupFile == "")
        LOG(WARNING) << "No multicast group file specified";

    started = true;
    LOG(INFO) << "Starting stitched-mode renderer using"
              << " integration bridge " << intBridgeName
              << " and access bridge "
              << (accessBridgeName == "" ? "[none]" : accessBridgeName);

    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.setUplinkIface(uplinkIface);
        tunnelEpManager.setUplinkVlan(uplinkVlan);
        tunnelEpManager.setParentRenderer(this);
        tunnelEpManager.start();
    }

    if (!flowIdCache.empty())
        idGen.setPersistLocation(flowIdCache);

    if (connTrack) {
        ctZoneManager.setCtZoneRange(ctZoneRangeStart, ctZoneRangeEnd);
        ctZoneManager.enableNetLink(true);
        ctZoneManager.init(ID_NMSPC_CONNTRACK);

        intFlowManager.enableConnTrack();
        accessFlowManager.enableConnTrack();
    }

    intFlowManager.setEncapType(encapType);
    intFlowManager.setEncapIface(encapIface);
    intFlowManager.setFloodScope(IntFlowManager::ENDPOINT_GROUP);
    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        assert(tunnelRemotePort != 0);
        intFlowManager.setTunnel(tunnelRemoteIp, tunnelRemotePort);
    }
    intFlowManager.setVirtualRouter(virtualRouter, routerAdv, virtualRouterMac);
    intFlowManager.setVirtualDHCP(virtualDHCP, virtualDHCPMac);
    intFlowManager.setMulticastGroupFile(mcastGroupFile);
    intFlowManager.setEndpointAdv(endpointAdvMode, tunnelEndpointAdvMode,
            tunnelEndpointAdvIntvl);
    if(!dropLogIntIface.empty()) {
        intFlowManager.setDropLog(dropLogIntIface, dropLogRemoteIp,
                dropLogRemotePort);
    }
    if(!dropLogAccessIface.empty()) {
        accessFlowManager.setDropLog(dropLogAccessIface, dropLogRemoteIp,
                        dropLogRemotePort);
    }
    intSwitchManager.registerStateHandler(&intFlowManager);
    intSwitchManager.start(intBridgeName);
    if (accessBridgeName != "") {
        accessSwitchManager.registerStateHandler(&accessFlowManager);
        accessSwitchManager.start(accessBridgeName);
    }
    intFlowManager.start();
    intFlowManager.registerModbListeners();
    spanRenderer.start();

    if (accessBridgeName != "") {
        accessFlowManager.start();
    }

    pktInHandler.registerConnection(intSwitchManager.getConnection(),
                                    (accessBridgeName != "")
                                    ? accessSwitchManager.getConnection()
                                    : NULL);
    pktInHandler.setPortMapper(&intSwitchManager.getPortMapper(),
                               (accessBridgeName != "")
                               ? &accessSwitchManager.getPortMapper()
                               : NULL);
    pktInHandler.setFlowReader(&intSwitchManager.getFlowReader());
    pktInHandler.start();

    if (ifaceStatsEnabled) {
        interfaceStatsManager.setTimerInterval(ifaceStatsInterval);
        interfaceStatsManager.
            registerConnection(intSwitchManager.getConnection(),
                               (accessBridgeName != "")
                               ? accessSwitchManager.getConnection()
                               : NULL);
        interfaceStatsManager.start();
    }
    if (contractStatsEnabled) {
        contractStatsManager.setTimerInterval(contractStatsInterval);
        contractStatsManager.setAgentUUID(getAgent().getUuid());
        contractStatsManager.
            registerConnection(intSwitchManager.getConnection());
        contractStatsManager.start();
    }
    if (secGroupStatsEnabled && accessBridgeName != "") {
        secGrpStatsManager.setTimerInterval(secGroupStatsInterval);
        secGrpStatsManager.setAgentUUID(getAgent().getUuid());
        secGrpStatsManager.
            registerConnection(accessSwitchManager.getConnection());
        secGrpStatsManager.start();
    }

    intSwitchManager.connect();
    if (accessBridgeName != "") {
        accessSwitchManager.connect();
    }

    cleanupTimer.reset(new deadline_timer(getAgent().getAgentIOService()));
    cleanupTimer->expires_from_now(CLEANUP_INTERVAL);
    cleanupTimer->async_wait(bind(&OVSRenderer::onCleanupTimer,
                                  this, error));
}

void OVSRenderer::stop() {
    if (!started) return;
    started = false;

    LOG(DEBUG) << "Stopping stitched-mode renderer";

    if (cleanupTimer) {
        cleanupTimer->cancel();
    }

    if (ifaceStatsEnabled)
        interfaceStatsManager.stop();
    if (contractStatsEnabled)
        contractStatsManager.stop();
    if (secGroupStatsEnabled)
        secGrpStatsManager.stop();

    pktInHandler.stop();

    intFlowManager.stop();
    accessFlowManager.stop();

    intSwitchManager.stop();
    accessSwitchManager.stop();

    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.stop();
    }
}

#define DEF_FLOWID_CACHEDIR \
    LOCALSTATEDIR"/lib/opflex-agent-ovs/ids"
#define DEF_MCAST_GROUPFILE \
    LOCALSTATEDIR"/lib/opflex-agent-ovs/mcast/opflex-groups.json"

void OVSRenderer::setProperties(const ptree& properties) {
    static const std::string OVS_BRIDGE_NAME("ovs-bridge-name");
    static const std::string INT_BRIDGE_NAME("int-bridge-name");
    static const std::string ACCESS_BRIDGE_NAME("access-bridge-name");

    static const std::string ENCAP_VXLAN("encap.vxlan");
    static const std::string ENCAP_IVXLAN("encap.ivxlan");
    static const std::string ENCAP_VLAN("encap.vlan");

    static const std::string UPLINK_IFACE("uplink-iface");
    static const std::string UPLINK_VLAN("uplink-vlan");
    static const std::string ENCAP_IFACE("encap-iface");
    static const std::string REMOTE_IP("remote-ip");
    static const std::string REMOTE_PORT("remote-port");
    static const std::string INT_BR_IFACE("int-br-iface");
    static const std::string ACC_BR_IFACE("access-br-iface");

    static const std::string VIRTUAL_ROUTER("forwarding"
                                            ".virtual-router.enabled");
    static const std::string VIRTUAL_ROUTER_MAC("forwarding"
                                                ".virtual-router.mac");

    static const std::string VIRTUAL_ROUTER_RA("forwarding.virtual-router"
                                               ".ipv6.router-advertisement");

    static const std::string VIRTUAL_DHCP("forwarding.virtual-dhcp.enabled");
    static const std::string VIRTUAL_DHCP_MAC("forwarding.virtual-dhcp.mac");

    static const std::string ENDPOINT_ADV("forwarding."
                                          "endpoint-advertisements.enabled");
    static const std::string ENDPOINT_ADV_MODE("forwarding."
                                               "endpoint-advertisements.mode");
    static const std::string ENDPOINT_TNL_ADV_MODE("forwarding."
                               "endpoint-advertisements.tunnel-endpoint-mode");
    static const std::string ENDPOINT_TNL_ADV_INTVL("forwarding."
                                   "endpoint-advertisements.tunnel-endpoint-interval");

    static const std::string FLOWID_CACHE_DIR("flowid-cache-dir");
    static const std::string MCAST_GROUP_FILE("mcast-group-file");

    static const std::string CONN_TRACK("forwarding.connection-tracking."
                                        "enabled");
    static const std::string CONN_TRACK_RANGE_START("forwarding."
                                                    "connection-tracking."
                                                    "zone-range.start");
    static const std::string CONN_TRACK_RANGE_END("forwarding."
                                                  "connection-tracking."
                                                  "zone-range.end");

    static const std::string STATS_INTERFACE_ENABLED("statistics"
                                                     ".interface.enabled");
    static const std::string STATS_INTERFACE_INTERVAL("statistics"
                                                      ".interface.interval");
    static const std::string STATS_CONTRACT_ENABLED("statistics"
                                                    ".contract.enabled");
    static const std::string STATS_CONTRACT_INTERVAL("statistics"
                                                    ".contract.interval");
    static const std::string STATS_SECGROUP_ENABLED("statistics"
                                                    ".security-group.enabled");
    static const std::string STATS_SECGROUP_INTERVAL("statistics"
                                                     ".security-group"
                                                     ".interval");
    static const std::string DROP_LOG_ENCAP_GENEVE("drop-log.geneve");

    intBridgeName =
        properties.get<std::string>(OVS_BRIDGE_NAME, "br-int");
    intBridgeName =
        properties.get<std::string>(INT_BRIDGE_NAME, intBridgeName);
    accessBridgeName =
        properties.get<std::string>(ACCESS_BRIDGE_NAME, "");

    boost::optional<const ptree&> ivxlan =
        properties.get_child_optional(ENCAP_IVXLAN);
    boost::optional<const ptree&> vxlan =
        properties.get_child_optional(ENCAP_VXLAN);
    boost::optional<const ptree&> vlan =
        properties.get_child_optional(ENCAP_VLAN);

    boost::optional<const ptree&> dropLogEncapGeneve =
            properties.get_child_optional(DROP_LOG_ENCAP_GENEVE);


    encapType = IntFlowManager::ENCAP_NONE;
    int count = 0;
    if (ivxlan) {
        LOG(ERROR) << "Encapsulation type ivxlan unsupported";
        count += 1;
    }
    if (vlan) {
        encapType = IntFlowManager::ENCAP_VLAN;
        encapIface = vlan.get().get<std::string>(ENCAP_IFACE, "");
        count += 1;
    }
    if (vxlan) {
        encapType = IntFlowManager::ENCAP_VXLAN;
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
    }

    if(dropLogEncapGeneve) {
        dropLogIntIface = dropLogEncapGeneve.get().get<std::string>(INT_BR_IFACE, "");
        dropLogAccessIface = dropLogEncapGeneve.get().get<std::string>(ACC_BR_IFACE, "");
        dropLogRemoteIp = dropLogEncapGeneve.get().get<std::string>(REMOTE_IP, "");
        dropLogRemotePort = dropLogEncapGeneve.get().get<uint16_t>(REMOTE_PORT, 6081);
    }

    virtualRouter = properties.get<bool>(VIRTUAL_ROUTER, true);
    virtualRouterMac =
        properties.get<std::string>(VIRTUAL_ROUTER_MAC, "00:22:bd:f8:19:ff");
    routerAdv = properties.get<bool>(VIRTUAL_ROUTER_RA, false);
    virtualDHCP = properties.get<bool>(VIRTUAL_DHCP, true);
    virtualDHCPMac =
        properties.get<std::string>(VIRTUAL_DHCP_MAC, "00:22:bd:f8:19:ff");

    if (properties.get<bool>(ENDPOINT_ADV, true) == false) {
        endpointAdvMode = AdvertManager::EPADV_DISABLED;
    } else {
        std::string epAdvStr =
            properties.get<std::string>(ENDPOINT_ADV_MODE,
                                        "gratuitous-broadcast");
        if (epAdvStr == "gratuitous-unicast") {
            endpointAdvMode = AdvertManager::EPADV_GRATUITOUS_UNICAST;
        } else if (epAdvStr == "router-request") {
            endpointAdvMode = AdvertManager::EPADV_ROUTER_REQUEST;
        } else {
            endpointAdvMode = AdvertManager::EPADV_GRATUITOUS_BROADCAST;
        }
    }

    std::string tnlEpAdvStr =
        properties.get<std::string>(ENDPOINT_TNL_ADV_MODE,
                                    "rarp-broadcast");
    if (tnlEpAdvStr == "gratuitous-broadcast") {
        tunnelEndpointAdvMode = AdvertManager::EPADV_GRATUITOUS_BROADCAST;
    } else if(tnlEpAdvStr == "disabled") {
        tunnelEndpointAdvMode = AdvertManager::EPADV_DISABLED;
    } else {
        tunnelEndpointAdvMode = AdvertManager::EPADV_RARP_BROADCAST;
    }

    tunnelEndpointAdvIntvl =
        properties.get<uint64_t>(ENDPOINT_TNL_ADV_INTVL,
                                    300);

    connTrack = properties.get<bool>(CONN_TRACK, true);
    ctZoneRangeStart = properties.get<uint16_t>(CONN_TRACK_RANGE_START, 1);
    ctZoneRangeEnd = properties.get<uint16_t>(CONN_TRACK_RANGE_END, 65534);

    flowIdCache = properties.get<std::string>(FLOWID_CACHE_DIR,
                                              DEF_FLOWID_CACHEDIR);

    mcastGroupFile = properties.get<std::string>(MCAST_GROUP_FILE,
                                                 DEF_MCAST_GROUPFILE);

    ifaceStatsEnabled = properties.get<bool>(STATS_INTERFACE_ENABLED, true);
    contractStatsEnabled = properties.get<bool>(STATS_CONTRACT_ENABLED, true);
    secGroupStatsEnabled = properties.get<bool>(STATS_SECGROUP_ENABLED, true);
    ifaceStatsInterval = properties.get<long>(STATS_INTERFACE_INTERVAL, 30000);
    contractStatsInterval =
        properties.get<long>(STATS_CONTRACT_INTERVAL, 10000);
    secGroupStatsInterval =
        properties.get<long>(STATS_SECGROUP_INTERVAL, 10000);
    if (ifaceStatsInterval <= 0) {
        ifaceStatsEnabled = false;
    }
    if (contractStatsInterval <= 0) {
        contractStatsEnabled = false;
    }
    if (secGroupStatsInterval <= 0) {
        secGroupStatsEnabled = false;
    }
}

static bool connTrackIdGarbageCb(EndpointManager& endpointManager,
                                 opflex::ofcore::OFFramework& framework,
                                 const std::string& nmspc,
                                 const std::string& str) {
    if (str.empty()) return false;
    if (str[0] == '/') {
        // a URI means a routing domain (in IntFlowManager)
        return IdGenerator::uriIdGarbageCb
            <modelgbp::gbp::RoutingDomain>(framework, nmspc, str);
    } else {
        // a UUID means an endpoint UUID (in AccessFlowManager)
        return (bool)endpointManager.getEndpoint(str);
    }
}

void OVSRenderer::onCleanupTimer(const boost::system::error_code& ec) {
    if (ec) return;

    idGen.cleanup();
    intFlowManager.cleanup();
    accessFlowManager.cleanup();

    IdGenerator::garbage_cb_t gcb =
        bind(connTrackIdGarbageCb,
             std::ref(getEndpointManager()),
             std::ref(getFramework()), _1, _2);
    idGen.collectGarbage(ID_NMSPC_CONNTRACK, gcb);

    if (started) {
        cleanupTimer->expires_from_now(CLEANUP_INTERVAL);
        cleanupTimer->async_wait(bind(&OVSRenderer::onCleanupTimer,
                                      this, error));
    }
}

} /* namespace opflexagent */

extern "C" const opflexagent::RendererPlugin* init_renderer_plugin() {
    static const opflexagent::OVSRendererPlugin smrPlugin;

    return &smrPlugin;
}
