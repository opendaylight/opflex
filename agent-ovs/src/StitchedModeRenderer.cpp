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
#include "FlowConstants.h"
#include "logging.h"

#include <boost/asio/placeholders.hpp>
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/sub_range.hpp>

namespace ovsagent {

using std::string;
using opflex::ofcore::OFFramework;
using boost::property_tree::ptree;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::algorithm::make_split_iterator;
using boost::algorithm::token_finder;
using boost::algorithm::is_any_of;
using boost::copy_range;

static const boost::posix_time::milliseconds CLEANUP_INTERVAL(3*60*1000);

StitchedModeRenderer::StitchedModeRenderer(Agent& agent_)
    : Renderer(agent_),
      intSwitchManager(agent_, intFlowExecutor,
                       intFlowReader, intPortMapper, idGen),
      intFlowManager(agent_, intSwitchManager, idGen),
      accessSwitchManager(agent_, accessFlowExecutor,
                          accessFlowReader, accessPortMapper, idGen),
      accessFlowManager(agent_, accessSwitchManager, idGen),
      statsManager(&agent_, intSwitchManager.getPortMapper()),
      tunnelEpManager(&agent_), tunnelRemotePort(0), uplinkVlan(0),
      virtualRouter(true), routerAdv(true), started(false) {

}

StitchedModeRenderer::~StitchedModeRenderer() {

}

void StitchedModeRenderer::start() {
    if (started) return;

    if (intBridgeName == "") {
        LOG(ERROR) << "OVS integration bridge name not set";
        return;
    }
    if (accessBridgeName == "") {
        LOG(ERROR) << "OVS access bridge name not set";
        return;
    }

    started = true;
    LOG(INFO) << "Starting stitched-mode renderer using"
              << " integration bridge " << intBridgeName
              << " and access bridge " << accessBridgeName;

    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
        tunnelEpManager.setUplinkIface(uplinkIface);
        tunnelEpManager.setUplinkVlan(uplinkVlan);
        tunnelEpManager.start();
    }

    if (!flowIdCache.empty())
        idGen.setPersistLocation(flowIdCache);

    for (size_t i = 0; i < flow::id::NUM_NAMESPACES; i++) {
        idGen.initNamespace(flow::id::NAMESPACES[i]);
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
    intFlowManager.setEndpointAdv(endpointAdv);

    intSwitchManager.registerStateHandler(&intFlowManager);
    intSwitchManager.start(intBridgeName);
    accessSwitchManager.registerStateHandler(&accessFlowManager);
    accessSwitchManager.start(accessBridgeName);
    intFlowManager.start();
    intFlowManager.registerModbListeners();
    accessFlowManager.start();
    intSwitchManager.connect();
    accessSwitchManager.connect();

    statsManager.registerConnection(intSwitchManager.getConnection());
    statsManager.start();

    cleanupTimer.reset(new deadline_timer(getAgent().getAgentIOService()));
    cleanupTimer->expires_from_now(CLEANUP_INTERVAL);
    cleanupTimer->async_wait(bind(&StitchedModeRenderer::onCleanupTimer,
                                  this, error));
}

void StitchedModeRenderer::stop() {
    if (!started) return;
    started = false;

    LOG(DEBUG) << "Stopping stitched-mode renderer";

    if (cleanupTimer) {
        cleanupTimer->cancel();
    }

    statsManager.stop();

    intFlowManager.stop();
    accessFlowManager.stop();

    intSwitchManager.stop();
    accessSwitchManager.stop();

    if (encapType == IntFlowManager::ENCAP_VXLAN ||
        encapType == IntFlowManager::ENCAP_IVXLAN) {
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

    static const std::string VIRTUAL_ROUTER("forwarding"
                                            ".virtual-router.enabled");
    static const std::string VIRTUAL_ROUTER_MAC("forwarding"
                                                ".virtual-router.mac");

    static const std::string VIRTUAL_ROUTER_RA("forwarding.virtual-router"
                                               ".ipv6.router-advertisement");

    static const std::string VIRTUAL_DHCP("forwarding.virtual-dhcp.enabled");
    static const std::string VIRTUAL_DHCP_MAC("forwarding.virtual-dhcp.mac");

    static const std::string ENDPOINT_ADV("forwarding.endpoint-advertisements"
                                          ".enabled");

    static const std::string FLOWID_CACHE_DIR("flowid-cache-dir");
    static const std::string MCAST_GROUP_FILE("mcast-group-file");

    intBridgeName =
        properties.get<std::string>(OVS_BRIDGE_NAME, "br-int");
    intBridgeName =
        properties.get<std::string>(INT_BRIDGE_NAME, intBridgeName);
    accessBridgeName =
        properties.get<std::string>(ACCESS_BRIDGE_NAME, "br-access");

    boost::optional<const ptree&> ivxlan =
        properties.get_child_optional(ENCAP_IVXLAN);
    boost::optional<const ptree&> vxlan =
        properties.get_child_optional(ENCAP_VXLAN);
    boost::optional<const ptree&> vlan =
        properties.get_child_optional(ENCAP_VLAN);

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

template <class MO>
static bool uriIdGarbageCb(opflex::ofcore::OFFramework& framework,
                           const std::string&, const std::string& str) {
    return MO::resolve(framework, opflex::modb::URI(str));
}

typedef boost::function<bool(opflex::ofcore::OFFramework&,
                             const string&,
                             const string&)> uri_garbage_cb_t;
static const uri_garbage_cb_t ID_NAMESPACE_CB[] =
    {uriIdGarbageCb<modelgbp::gbp::EpGroup>,
     uriIdGarbageCb<modelgbp::gbp::FloodDomain>,
     uriIdGarbageCb<modelgbp::gbp::BridgeDomain>,
     uriIdGarbageCb<modelgbp::gbp::RoutingDomain>,
     uriIdGarbageCb<modelgbp::gbp::Contract>,
     uriIdGarbageCb<modelgbp::gbp::L3ExternalNetwork>,
     uriIdGarbageCb<modelgbp::gbp::Subnet>,
     uriIdGarbageCb<modelgbp::gbp::SecGroup>};
static const size_t NUM_ID_NAMESPACE_CB =
    (sizeof(ID_NAMESPACE_CB)/sizeof(uri_garbage_cb_t));

static bool secGrpSetIdGarbageCb(EndpointManager& endpointManager,
                                 const string&, const string& str) {
    EndpointListener::uri_set_t secGrps;

    typedef boost::algorithm::split_iterator<string::const_iterator> ssi;
    ssi it = make_split_iterator(str, token_finder(is_any_of(",")));

    for(; it != ssi(); ++it) {
        string uri(copy_range<string>(*it));
        if (!uri.empty())
            secGrps.insert(opflex::modb::URI(uri));
    }

    if (secGrps.empty()) return true;
    return !endpointManager.secGrpSetEmpty(secGrps);
}

static bool endpointGarbageCb(EndpointManager& endpointManager,
                              const string&, const string& uuid) {
    return endpointManager.getEndpoint(uuid);
}

static bool serviceGarbageCb(ServiceManager& serviceManager,
                             const string&, const string& uuid) {
    return serviceManager.getAnycastService(uuid);
}

void StitchedModeRenderer::onCleanupTimer(const boost::system::error_code& ec) {
    if (ec) return;

    idGen.cleanup();
    // Clean up URI-based ID namespaces
    for (size_t i = 0; i < NUM_ID_NAMESPACE_CB; i++) {
        string ns(flow::id::NAMESPACES[i]);
        IdGenerator::garbage_cb_t gcb =
            boost::bind(ID_NAMESPACE_CB[i], boost::ref(getFramework()), _1, _2);
        getAgent().getAgentIOService()
            .dispatch(boost::bind(&IdGenerator::collectGarbage,
                                  boost::ref(idGen), ns, gcb));
    }
    // Clean up security group sets
    {
        IdGenerator::garbage_cb_t gcb =
            boost::bind(secGrpSetIdGarbageCb,
                        boost::ref(getEndpointManager()), _1, _2);
        idGen.collectGarbage(flow::id::SECGROUP_SET, gcb);
    }
    // Clean up endpoints
    {
        IdGenerator::garbage_cb_t gcb =
            boost::bind(endpointGarbageCb,
                        boost::ref(getEndpointManager()), _1, _2);
        idGen.collectGarbage(flow::id::ENDPOINT, gcb);
    }

    // Clean up anycast services
    {
        IdGenerator::garbage_cb_t gcb =
            boost::bind(serviceGarbageCb,
                        boost::ref(getAgent().getServiceManager()), _1, _2);
        idGen.collectGarbage(flow::id::SERVICE, gcb);
    }

    if (started) {
        cleanupTimer->expires_from_now(CLEANUP_INTERVAL);
        cleanupTimer->async_wait(bind(&StitchedModeRenderer::onCleanupTimer,
                                      this, error));
    }
}

} /* namespace ovsagent */

