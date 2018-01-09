/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <boost/system/error_code.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/asio/placeholders.hpp>

#include <netinet/icmp6.h>

#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/BcastFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/Endpoint.h>
#include <opflexagent/EndpointManager.h>
#include <opflexagent/EndpointListener.h>
#include <opflexagent/Network.h>

#include "SwitchConnection.h"
#include "IntFlowManager.h"
#include "TableState.h"
#include "PacketInHandler.h"
#include "CtZoneManager.h"
#include "Packets.h"
#include "FlowUtils.h"
#include "FlowConstants.h"
#include "FlowBuilder.h"

#include "arp.h"
#include "eth.h"
#include "ovs-ofputil.h"

#include <openvswitch/list.h>

using std::string;
using std::vector;
using std::ostringstream;
using std::shared_ptr;
using std::unordered_set;
using std::unordered_map;
using std::bind;
using boost::algorithm::trim;
using boost::ref;
using boost::optional;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using boost::asio::placeholders::error;
using std::chrono::milliseconds;
using std::unique_lock;
using std::mutex;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::class_id_t;

namespace pt = boost::property_tree;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

namespace opflexagent {

static const char* ID_NAMESPACES[] =
    {"floodDomain", "bridgeDomain", "routingDomain",
     "externalNetwork", "l24classifierRule", "service"};

static const char* ID_NMSPC_FD            = ID_NAMESPACES[0];
static const char* ID_NMSPC_BD            = ID_NAMESPACES[1];
static const char* ID_NMSPC_RD            = ID_NAMESPACES[2];
static const char* ID_NMSPC_EXTNET        = ID_NAMESPACES[3];
static const char* ID_NMSPC_L24CLASS_RULE = ID_NAMESPACES[4];
static const char* ID_NMSPC_SERVICE       = ID_NAMESPACES[5];

IntFlowManager::IntFlowManager(Agent& agent_,
                               SwitchManager& switchManager_,
                               IdGenerator& idGen_,
                               CtZoneManager& ctZoneManager_,
                               PacketInHandler& pktInHandler_) :
    agent(agent_), switchManager(switchManager_), idGen(idGen_),
    ctZoneManager(ctZoneManager_), pktInHandler(pktInHandler_),
    taskQueue(agent.getAgentIOService()), encapType(ENCAP_NONE),
    floodScope(FLOOD_DOMAIN), tunnelPortStr("4789"),
    virtualRouterEnabled(false), routerAdv(false),
    virtualDHCPEnabled(false), conntrackEnabled(false),
    advertManager(agent, *this), isSyncing(false), stopping(false) {
    // set up flow tables
    switchManager.setMaxFlowTables(NUM_FLOW_TABLES);

    memset(routerMac, 0, sizeof(routerMac));
    memset(dhcpMac, 0, sizeof(dhcpMac));
    tunnelDst = address::from_string("127.0.0.1");

    agent.getFramework().registerPeerStatusListener(this);
}

void IntFlowManager::start() {
    // set up port mapper
    switchManager.getPortMapper().registerPortStatusListener(this);
    advertManager.setPortMapper(&switchManager.getPortMapper());

    // Register connection handlers
    SwitchConnection* conn = switchManager.getConnection();
    advertManager.registerConnection(conn);

    for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
        idGen.initNamespace(ID_NAMESPACES[i]);
    }

    initPlatformConfig();
    createStaticFlows();
}

void IntFlowManager::registerModbListeners() {
    // Initialize policy listeners
    agent.getEndpointManager().registerListener(this);
    agent.getServiceManager().registerListener(this);
    agent.getExtraConfigManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
}

void IntFlowManager::stop() {
    stopping = true;

    agent.getEndpointManager().unregisterListener(this);
    agent.getServiceManager().unregisterListener(this);
    agent.getExtraConfigManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);

    advertManager.stop();
    switchManager.getPortMapper().unregisterPortStatusListener(this);
}

void IntFlowManager::setEncapType(EncapType encapType) {
    this->encapType = encapType;
}

void IntFlowManager::setEncapIface(const string& encapIf) {
    if (encapIf.empty()) {
        LOG(ERROR) << "Ignoring empty encapsulation interface name";
        return;
    }
    encapIface = encapIf;
}

void IntFlowManager::setFloodScope(FloodScope fscope) {
    floodScope = fscope;
}

uint32_t IntFlowManager::getTunnelPort() {
    return switchManager.getPortMapper().FindPort(encapIface);
}

void IntFlowManager::setTunnel(const string& tunnelRemoteIp,
                               uint16_t tunnelRemotePort) {
    boost::system::error_code ec;
    address tunDst = address::from_string(tunnelRemoteIp, ec);
    if (ec) {
        LOG(ERROR) << "Invalid tunnel destination IP: "
                   << tunnelRemoteIp << ": " << ec.message();
    } else if (tunDst.is_v6()) {
        LOG(ERROR) << "IPv6 tunnel destinations are not supported";
    } else {
        tunnelDst = tunDst;
    }

    ostringstream ss;
    ss << tunnelRemotePort;
    tunnelPortStr = ss.str();
}

void IntFlowManager::setVirtualRouter(bool virtualRouterEnabled,
                                      bool routerAdv,
                                      const string& virtualRouterMac) {
    this->virtualRouterEnabled = virtualRouterEnabled;
    this->routerAdv = routerAdv;
    try {
        MAC(virtualRouterMac).toUIntArray(routerMac);
    } catch (std::invalid_argument) {
        LOG(ERROR) << "Invalid virtual router MAC: " << virtualRouterMac;
    }
    advertManager.enableRouterAdv(virtualRouterEnabled && routerAdv);
}

void IntFlowManager::setVirtualDHCP(bool dhcpEnabled,
                                    const string& mac) {
    this->virtualDHCPEnabled = dhcpEnabled;
    try {
        MAC(mac).toUIntArray(dhcpMac);
    } catch (std::invalid_argument) {
        LOG(ERROR) << "Invalid virtual DHCP server MAC: " << mac;
    }
}

void IntFlowManager::setEndpointAdv(AdvertManager::EndpointAdvMode mode) {
    if (mode != AdvertManager::EPADV_DISABLED)
        advertManager.enableEndpointAdv(mode);
}

void IntFlowManager::setMulticastGroupFile(const std::string& mcastGroupFile) {
    this->mcastGroupFile = mcastGroupFile;
}

void IntFlowManager::enableConnTrack() {
    conntrackEnabled = true;
}

address IntFlowManager::getEPGTunnelDst(const URI& epgURI) {
    if (encapType != IntFlowManager::ENCAP_VXLAN &&
        encapType != IntFlowManager::ENCAP_IVXLAN)
        return address();

    optional<string> epgMcastIp =
        agent.getPolicyManager().getMulticastIPForGroup(epgURI);
    if (epgMcastIp) {
        boost::system::error_code ec;
        address ip = address::from_string(epgMcastIp.get(), ec);
        if (ec || !ip.is_v4() || ! ip.is_multicast()) {
            LOG(WARNING) << "Ignoring invalid/unsupported group multicast "
                "IP: " << epgMcastIp.get();
            return getTunnelDst();
        }
        return ip;
    } else
        return getTunnelDst();
}

void IntFlowManager::endpointUpdated(const std::string& uuid) {
    if (stopping) return;

    advertManager.scheduleEndpointAdv(uuid);
    taskQueue.dispatch(uuid,
                       bind(&IntFlowManager::handleEndpointUpdate, this, uuid));
}

void IntFlowManager::serviceUpdated(const std::string& uuid) {
    if (stopping) return;

    advertManager.scheduleServiceAdv(uuid);
    taskQueue.dispatch(uuid,
                       bind(&IntFlowManager::handleServiceUpdate,
                            this, uuid));
}

void IntFlowManager::rdConfigUpdated(const opflex::modb::URI& rdURI) {
    domainUpdated(RoutingDomain::CLASS_ID, rdURI);
}

void IntFlowManager::egDomainUpdated(const opflex::modb::URI& egURI) {
    if (stopping) return;

    taskQueue.dispatch(egURI.toString(),
                       bind(&IntFlowManager::handleEndpointGroupDomainUpdate,
                            this, egURI));
}

void IntFlowManager::domainUpdated(class_id_t cid, const URI& domURI) {
    if (stopping) return;

    taskQueue.dispatch(domURI.toString(),
                       bind(&IntFlowManager::handleDomainUpdate,
                            this, cid, domURI));
}

void IntFlowManager::contractUpdated(const opflex::modb::URI& contractURI) {
    if (stopping) return;
    taskQueue.dispatch(contractURI.toString(),
                       bind(&IntFlowManager::handleContractUpdate,
                            this, contractURI));
}

void IntFlowManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    switchManager.enableSync();
    agent.getAgentIOService()
        .dispatch(bind(&IntFlowManager::handleConfigUpdate, this, configURI));
}

void IntFlowManager::portStatusUpdate(const string& portName,
                                      uint32_t portNo, bool fromDesc) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(bind(&IntFlowManager::handlePortStatusUpdate, this,
                       portName, portNo));
}

void IntFlowManager::peerStatusUpdated(const std::string&, int,
                                       PeerStatus peerStatus) {
    if (stopping || isSyncing) return;
    if (peerStatus == PeerStatusListener::READY) {
        advertManager.scheduleInitialEndpointAdv();
    }
}

bool IntFlowManager::getGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
                                            optional<URI>& rdURI,
                                            uint32_t& rdId,
                                            optional<URI>& bdURI,
                                            uint32_t& bdId,
                                            optional<URI>& fdURI,
                                            uint32_t& fdId) {
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<uint32_t> epgVnid = polMgr.getVnidForGroup(epgURI);
    if (!epgVnid) {
        return false;
    }
    vnid = epgVnid.get();

    optional<shared_ptr<RoutingDomain> > epgRd = polMgr.getRDForGroup(epgURI);
    optional<shared_ptr<BridgeDomain> > epgBd = polMgr.getBDForGroup(epgURI);
    optional<shared_ptr<FloodDomain> > epgFd = polMgr.getFDForGroup(epgURI);
    if (!epgRd && !epgBd && !epgFd) {
        return false;
    }

    bdId = 0;
    if (epgBd) {
        bdURI = epgBd.get()->getURI();
        bdId = getId(BridgeDomain::CLASS_ID, bdURI.get());
    }
    fdId = 0;
    if (epgFd) {    // FD present -> flooding is desired
        if (floodScope == ENDPOINT_GROUP) {
            fdURI = epgURI;
        } else  {
            fdURI = epgFd.get()->getURI();
        }
        fdId = getId(FloodDomain::CLASS_ID, fdURI.get());
    }
    rdId = 0;
    if (epgRd) {
        rdURI = epgRd.get()->getURI();
        rdId = getId(RoutingDomain::CLASS_ID, rdURI.get());
    }
    return true;
}

// Match helper functions
static FlowBuilder& matchEpg(FlowBuilder& fb,
                             IntFlowManager::EncapType encapType,
                             uint32_t epgId) {
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        fb.vlan(0xfff & epgId);
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        fb.tunId(epgId);
        break;
    }
    return fb;
}

static FlowBuilder& matchDestDom(FlowBuilder& fb, uint32_t bdId,
                                 uint32_t l3Id) {
    if (bdId != 0)
        fb.reg(4, bdId);
    if (l3Id != 0)
        fb.reg(6, l3Id);
    return fb;
}

static FlowBuilder& matchDestArp(FlowBuilder& fb, const address& ip,
                                 uint32_t bdId, uint32_t l3Id) {
    fb.arpDst(ip)
        .proto(arp::op::REQUEST)
        .ethDst(packets::MAC_ADDR_BROADCAST);
    return matchDestDom(fb, bdId, l3Id);
}

static FlowBuilder& matchDestNd(FlowBuilder& fb, const address* ip,
                                uint32_t bdId, uint32_t rdId,
                                uint8_t type = ND_NEIGHBOR_SOLICIT) {
    matchDestDom(fb, bdId, rdId)
        .ethType(eth::type::IPV6)
        .proto(58)
        .tpSrc(type)
        .tpDst(0)
        .ethDst(packets::MAC_ADDR_MULTICAST, packets::MAC_ADDR_MULTICAST);

    if (ip) {
        fb.ndTarget(type, *ip);
    }
    return fb;
}

static FlowBuilder& matchFd(FlowBuilder& fb,
                            uint32_t fgrpId, bool broadcast,
                            uint8_t* dstMac = NULL) {
    fb.reg(5, fgrpId);
    if (dstMac)
        fb.ethDst(dstMac);
    else if (broadcast)
        fb.ethDst(packets::MAC_ADDR_MULTICAST, packets::MAC_ADDR_MULTICAST);
    return fb;
}

static FlowBuilder& matchSubnet(FlowBuilder& fb, uint32_t rdId,
                                uint16_t prioBase,
                                address& ip, uint8_t prefixLen, bool src) {
    fb.priority(prioBase + prefixLen)
        .reg(6, rdId);
    if (src) fb.ipSrc(ip, prefixLen);
    else fb.ipDst(ip, prefixLen);
    return fb;
}

// Action helper functions
static FlowBuilder& actionSource(FlowBuilder& fb, uint32_t epgId, uint32_t bdId,
                                 uint32_t fgrpId,  uint32_t l3Id,
                                 uint8_t nextTable
                                 = IntFlowManager::SERVICE_REV_TABLE_ID,
                                 IntFlowManager::EncapType encapType
                                 = IntFlowManager::ENCAP_NONE,
                                 bool setPolicyApplied = false)
{
    if (encapType == IntFlowManager::ENCAP_VLAN) {
        fb.action().popVlan();
    }

    fb.action()
        .reg(MFF_REG0, epgId)
        .reg(MFF_REG4, bdId)
        .reg(MFF_REG5, fgrpId)
        .reg(MFF_REG6, l3Id);
    if (setPolicyApplied) {
        fb.action().metadata(flow::meta::POLICY_APPLIED,
                             flow::meta::POLICY_APPLIED);
    }
    fb.action().go(nextTable);
    return fb;
}

void IntFlowManager::actionTunnelMetadata(ActionBuilder& ab,
                                          IntFlowManager::EncapType type,
                                          const optional<address>& tunDst) {
    switch (type) {
    case IntFlowManager::ENCAP_VLAN:
        ab.pushVlan();
        ab.regMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case IntFlowManager::ENCAP_VXLAN:
        ab.regMove(MFF_REG0, MFF_TUN_ID);
        if (tunDst) {
            if (tunDst->is_v4()) {
                ab.reg(MFF_TUN_DST, tunDst->to_v4().to_ulong());
            } else {
                // this should be unreachable
                LOG(WARNING) << "Ipv6 tunnel destination unsupported";
            }
        } else {
            ab.regMove(MFF_REG7, MFF_TUN_DST);
        }
        // fall through
    case IntFlowManager::ENCAP_IVXLAN:
        break;
    default:
        break;
    }
}

static FlowBuilder& actionDestEpArp(FlowBuilder& fb,
                                    uint32_t epgId, uint32_t port,
                                    const uint8_t* dstMac) {
    fb.action().reg(MFF_REG2, epgId)
        .reg(MFF_REG7, port)
        .ethDst(dstMac)
        .go(IntFlowManager::POL_TABLE_ID);
    return fb;
}

static FlowBuilder& actionOutputToEPGTunnel(FlowBuilder& fb) {
    fb.action()
        .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK)
        .go(IntFlowManager::OUT_TABLE_ID);
    return fb;
}

static FlowBuilder& actionArpReply(FlowBuilder& fb, const uint8_t *mac,
                                   const address& ip,
                                   IntFlowManager::EncapType type
                                   = IntFlowManager::ENCAP_NONE) {
    fb.action()
        .regMove(MFF_ETH_SRC, MFF_ETH_DST)
        .reg(MFF_ETH_SRC, mac)
        .reg16(MFF_ARP_OP, arp::op::REPLY)
        .regMove(MFF_ARP_SHA, MFF_ARP_THA)
        .reg(MFF_ARP_SHA, mac)
        .regMove(MFF_ARP_SPA, MFF_ARP_TPA)
        .reg(MFF_ARP_SPA, ip.to_v4().to_ulong());
    switch (type) {
    case IntFlowManager::ENCAP_VLAN:
        fb.action()
            .pushVlan()
            .regMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
        fb.action()
            .regMove(MFF_TUN_SRC, MFF_TUN_DST);
        break;
    default:
        break;
    }
    fb.action().output(OFPP_IN_PORT);
    return fb;
}

static FlowBuilder& actionRevNatDest(FlowBuilder& fb, uint32_t epgVnid,
                                     uint32_t bdId, uint32_t fgrpId,
                                     uint32_t rdId, uint32_t ofPort) {
    fb.action()
        .reg(MFF_REG2, epgVnid)
        .reg(MFF_REG4, bdId)
        .reg(MFF_REG5, fgrpId)
        .reg(MFF_REG6, rdId)
        .reg(MFF_REG7, ofPort)
        .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
        .go(IntFlowManager::NAT_IN_TABLE_ID);
    return fb;
}

static FlowBuilder& actionController(FlowBuilder& fb, uint32_t epgId = 0,
                                     uint64_t metadata = 0) {
    if (epgId != 0)
        fb.action().reg(MFF_REG0, epgId);
    if (metadata)
        fb.action().reg64(MFF_METADATA, metadata);
    fb.action().controller();
    return fb;
}

static FlowBuilder& actionSecAllow(FlowBuilder& fb) {
    fb.action().go(IntFlowManager::SRC_TABLE_ID);
    return fb;
}

static void revNatICMPFlows(FlowEntryList& el, bool v4, uint8_t type) {
    FlowBuilder fb;
    fb.priority(10)
        .cookie(v4 ? flow::cookie::ICMP_ERROR_V4 : flow::cookie::ICMP_ERROR_V6)
        .metadata(flow::meta::out::REV_NAT, flow::meta::out::MASK)
        .action().controller();

    if (v4) {
        fb.ethType(eth::type::IP).proto(1 /* ICMP */).tpSrc(type);
    } else {
        fb.ethType(eth::type::IPV6).proto(58 /* ICMP */).tpSrc(type);
    }
    return fb.build(el);
}

// Flow creation helpers

static void flowsProxyDiscovery(FlowEntryList& el,
                                uint16_t priority,
                                const address& ipAddr,
                                const uint8_t* macAddr,
                                uint32_t epgVnid, uint32_t rdId,
                                uint32_t bdId,
                                bool router,
                                const uint8_t* matchSourceMac,
                                uint32_t tunPort,
                                IntFlowManager::EncapType encapType) {
    if (ipAddr.is_v4()) {
        if (tunPort != OFPP_NONE &&
            encapType != IntFlowManager::ENCAP_NONE) {
            FlowBuilder proxyArpTun;
            if (matchSourceMac)
                proxyArpTun.ethSrc(matchSourceMac);
            matchDestArp(proxyArpTun.priority(priority+1).inPort(tunPort),
                         ipAddr, bdId, rdId);
            actionArpReply(proxyArpTun, macAddr, ipAddr,
                           encapType)
                .build(el);
        }
        {
            FlowBuilder proxyArp;
            if (matchSourceMac)
                proxyArp.ethSrc(matchSourceMac);
            matchDestArp(proxyArp.priority(priority), ipAddr, bdId, rdId);
            actionArpReply(proxyArp, macAddr, ipAddr)
                .build(el);
        }
    } else {
        // pass MAC address in flow metadata
        uint64_t metadata = 0;
        memcpy(&metadata, macAddr, 6);
        ((uint8_t*)&metadata)[7] = 1;
        if (router)
            ((uint8_t*)&metadata)[6] = 1;

        FlowBuilder proxyND;
        if (matchSourceMac)
            proxyND.ethSrc(matchSourceMac);
        matchDestNd(proxyND.priority(priority).cookie(flow::cookie::NEIGH_DISC),
                    &ipAddr, bdId, rdId);
        actionController(proxyND, epgVnid, metadata);
        proxyND.build(el);
    }
}

static void flowsProxyDiscovery(IntFlowManager& flowMgr,
                                FlowEntryList& el,
                                uint16_t priority,
                                const address& ipAddr,
                                const uint8_t* macAddr,
                                uint32_t epgVnid, uint32_t rdId,
                                uint32_t bdId,
                                bool router = false,
                                const uint8_t* matchSourceMac = NULL) {
    flowsProxyDiscovery(el, priority, ipAddr, macAddr, epgVnid, rdId,
                        bdId, router, matchSourceMac, flowMgr.getTunnelPort(),
                        (epgVnid != 0)
                        ? flowMgr.getEncapType() : IntFlowManager::ENCAP_NONE);
}

static void flowsIpm(IntFlowManager& flowMgr,
                     FlowEntryList& elSrc, FlowEntryList& elBridgeDst,
                     FlowEntryList& elRouteDst,FlowEntryList& elOutput,
                     const uint8_t* macAddr, uint32_t ofPort,
                     uint32_t epgVnid, uint32_t rdId,
                     uint32_t bdId, uint32_t fgrpId,
                     uint32_t fepgVnid, uint32_t frdId,
                     uint32_t fbdId, uint32_t ffdId,
                     address& mappedIp, address& floatingIp,
                     uint32_t nextHopPort, const uint8_t* nextHopMac) {
    const uint8_t* effNextHopMac =
        nextHopMac ? nextHopMac : flowMgr.getRouterMacAddr();

    if (!floatingIp.is_unspecified()) {
        {
            // floating IP destination within the same EPG
            // Set up reverse DNAT
            FlowBuilder ipmRoute;
            matchDestDom(ipmRoute.priority(452)
                         .ipDst(floatingIp).reg(0, fepgVnid),
                         0, frdId)
                .action()
                .ethSrc(flowMgr.getRouterMacAddr()).ethDst(macAddr)
                .ipDst(mappedIp).decTtl();

            actionRevNatDest(ipmRoute, epgVnid, bdId,
                             fgrpId, rdId, ofPort);
            ipmRoute.build(elRouteDst);
        }
        {
            // Floating IP destination across EPGs
            // Apply policy for source EPG -> floating IP EPG
            // then resubmit with source EPG set to floating
            // IP EPG
            FlowBuilder ipmResub;
            matchDestDom(ipmResub.priority(450).ipDst(floatingIp),
                         0, frdId);
            ipmResub.action()
                .reg(MFF_REG2, fepgVnid)
                .reg(MFF_REG7, fepgVnid)
                .metadata(flow::meta::out::RESUBMIT_DST,
                          flow::meta::out::MASK)
                .go(IntFlowManager::POL_TABLE_ID);
            ipmResub.build(elRouteDst);
        }
        // Reply to discovery requests for the floating IP
        // address
        flowsProxyDiscovery(flowMgr, elBridgeDst, 20, floatingIp, macAddr,
                            fepgVnid, frdId, fbdId);
    }
    {
        // Apply NAT action in output table
        FlowBuilder ipmNatOut;
        ipmNatOut.priority(10)
            .metadata(flow::meta::out::NAT, flow::meta::out::MASK)
            .reg(6, rdId)
            .reg(7, fepgVnid)
            .ipSrc(mappedIp);
        ActionBuilder& ab = ipmNatOut.action();
        ab.ethSrc(macAddr).ethDst(effNextHopMac);
        if (!floatingIp.is_unspecified()) {
            ab.ipSrc(floatingIp);
        }
        ab.decTtl();

        if (nextHopPort == OFPP_NONE) {
            ab.reg(MFF_REG0, fepgVnid)
                .reg(MFF_REG4, fbdId)
                .reg(MFF_REG5, ffdId)
                .reg(MFF_REG6, frdId)
                .reg(MFF_REG7, (uint32_t)0)
                .reg64(MFF_METADATA, flow::meta::ROUTED)
                .resubmit(OFPP_IN_PORT, IntFlowManager::BRIDGE_TABLE_ID);
        } else {
            ab.reg(MFF_PKT_MARK, rdId)
                .output(nextHopPort);
        }
        ipmNatOut.build(elOutput);
    }

    // Handle traffic returning from a next hop interface
    if (nextHopPort != OFPP_NONE) {
        if (!floatingIp.is_unspecified()) {
            // reverse traffic from next hop interface where we
            // delivered with a DNAT to a floating IP.  We assume that
            // the destination IP is unique for a given next hop
            // interface.
            FlowBuilder ipmNextHopRev;
            ipmNextHopRev.priority(201).inPort(nextHopPort)
                .ethSrc(effNextHopMac).ipDst(floatingIp)
                .action()
                .ethSrc(flowMgr.getRouterMacAddr()).ethDst(macAddr)
                .ipDst(mappedIp).decTtl();

            actionRevNatDest(ipmNextHopRev, epgVnid, bdId,
                             fgrpId, rdId, ofPort);
            ipmNextHopRev.build(elSrc);
        }
        {
            // Reverse traffic from next hop interface where we
            // delivered with an SKB mark.  The SKB mark must be set
            // to the routing domain for the mapped destination IP
            // address
            FlowBuilder ipmNextHopRevMark;
            ipmNextHopRevMark.priority(200).inPort(nextHopPort)
                .ethSrc(effNextHopMac).mark(rdId).ipDst(mappedIp);
            if (nextHopMac)
                ipmNextHopRevMark.action().ethSrc(flowMgr.getRouterMacAddr());
            actionRevNatDest(ipmNextHopRevMark, epgVnid, bdId,
                             fgrpId, rdId, ofPort);
            ipmNextHopRevMark.build(elSrc);
        }
    }
}

static void flowsEndpointPortSec(FlowEntryList& elPortSec,
                                 const Endpoint& endPoint,
                                 uint32_t ofPort,
                                 bool hasMac,
                                 uint8_t* macAddr,
                                 const std::vector<address>& ipAddresses) {
    if (ofPort == OFPP_NONE)
        return;

    if (endPoint.isPromiscuousMode()) {
        // allow all packets from port
        actionSecAllow(FlowBuilder().priority(50).inPort(ofPort))
            .build(elPortSec);
    } else if (hasMac) {
        // allow L2 packets from port with EP MAC address
        actionSecAllow(FlowBuilder().priority(20)
                       .inPort(ofPort).ethSrc(macAddr))
            .build(elPortSec);

        for (const address& ipAddr : ipAddresses) {
            // Allow IPv4/IPv6 packets from port with EP IP address
            actionSecAllow(FlowBuilder().priority(30)
                           .inPort(ofPort).ethSrc(macAddr)
                           .ipSrc(ipAddr))
                .build(elPortSec);

            if (ipAddr.is_v4()) {
                // Allow ARP with correct source address
                actionSecAllow(FlowBuilder().priority(40)
                               .inPort(ofPort).ethSrc(macAddr)
                               .arpSrc(ipAddr))
                    .build(elPortSec);
            } else {
                // Allow neighbor advertisements with correct
                // source address
                actionSecAllow(FlowBuilder().priority(40)
                               .inPort(ofPort).ethSrc(macAddr)
                               .ndTarget(ND_NEIGHBOR_ADVERT, ipAddr))
                    .build(elPortSec);
            }
        }
    }

    for (const Endpoint::virt_ip_t& vip : endPoint.getVirtualIPs()) {
        network::cidr_t vip_cidr;
        if (!network::cidr_from_string(vip.second, vip_cidr)) {
            LOG(WARNING) << "Invalid endpoint VIP (CIDR): " << vip.second;
            continue;
        }
        uint8_t vmac[6];
        vip.first.toUIntArray(vmac);

        // Handle ARP/ND from "active" virtual IPs normally, that is
        // without generating a packet-in
        for (const address& ipAddr : ipAddresses) {
            if (!network::cidr_contains(vip_cidr, ipAddr)) {
                continue;
            }
            FlowBuilder active_vip;
            active_vip.priority(61).inPort(ofPort).ethSrc(vmac);
            if (ipAddr.is_v4()) {
                active_vip.arpSrc(ipAddr);
            } else {
                active_vip.ndTarget(ND_NEIGHBOR_ADVERT, ipAddr);
            }
            actionSecAllow(active_vip).build(elPortSec);
        }

        FlowBuilder vf;
        vf.priority(60).inPort(ofPort).ethSrc(vmac);

        if (vip_cidr.first.is_v4()) {
            vf.cookie(flow::cookie::VIRTUAL_IP_V4)
                .arpSrc(vip_cidr.first, vip_cidr.second);
        } else {
            vf.cookie(flow::cookie::VIRTUAL_IP_V6)
                .ndTarget(ND_NEIGHBOR_ADVERT,
                          vip_cidr.first, vip_cidr.second);
        }
        vf.action().controller().go(IntFlowManager::SRC_TABLE_ID)
            .parent().build(elPortSec);
    }
}

static void flowsVirtualDhcp(FlowEntryList& elSrc, uint32_t ofPort,
                             uint8_t* macAddr, bool v4) {
    FlowBuilder fb;
    flowutils::match_dhcp_req(fb, v4);
    actionController(fb);
    fb.priority(35)
        .cookie(v4 ? flow::cookie::DHCP_V4 : flow::cookie::DHCP_V6)
        .inPort(ofPort)
        .ethSrc(macAddr)
        .build(elSrc);
}

static void flowsEndpointDHCPSource(IntFlowManager& flowMgr,
                                    FlowEntryList& elPortSec,
                                    FlowEntryList& elBridgeDst,
                                    const Endpoint& endPoint,
                                    uint32_t ofPort,
                                    bool hasMac,
                                    uint8_t* macAddr,
                                    bool virtualDHCPEnabled,
                                    bool hasForwardingInfo,
                                    uint32_t epgVnid,
                                    uint32_t rdId,
                                    uint32_t bdId) {
    if (ofPort == OFPP_NONE)
        return;

    if (virtualDHCPEnabled && hasMac) {
        optional<Endpoint::DHCPv4Config> v4c = endPoint.getDHCPv4Config();
        optional<Endpoint::DHCPv6Config> v6c = endPoint.getDHCPv6Config();

        if (v4c) {
            flowsVirtualDhcp(elPortSec, ofPort, macAddr, true);

            if (hasForwardingInfo) {
                address_v4 serverIp(packets::LINK_LOCAL_DHCP);
                if (v4c.get().getServerIp()) {
                    boost::system::error_code ec;
                    address_v4 sip =
                        address_v4::from_string(v4c.get().getServerIp().get(),
                                                ec);
                    if (ec) {
                        LOG(WARNING) << "Invalid DHCP server IP: "
                                     << v4c.get().getServerIp().get();
                    } else  {
                        serverIp = sip;
                    }
                }

                flowsProxyDiscovery(elBridgeDst, 51,
                                    serverIp, flowMgr.getDHCPMacAddr(),
                                    epgVnid, rdId, bdId, false,
                                    NULL, OFPP_NONE,
                                    IntFlowManager::ENCAP_NONE);
            }
        }
        if (v6c) {
            flowsVirtualDhcp(elPortSec, ofPort, macAddr, false);

            if (hasForwardingInfo) {
                // IPv6 link-local address made from the DHCP MAC
                address_v6 serverIp = network::
                    construct_auto_ip_addr(address_v6::from_string("fe80::"),
                                           flowMgr.getDHCPMacAddr());
                flowsProxyDiscovery(elBridgeDst, 51,
                                    serverIp, flowMgr.getDHCPMacAddr(),
                                    epgVnid, rdId, bdId, false,
                                    NULL, OFPP_NONE,
                                    IntFlowManager::ENCAP_NONE);
            }
        }

        for (const Endpoint::virt_ip_t& vip :
                 endPoint.getVirtualIPs()) {
            if (endPoint.getMAC().get() == vip.first) continue;
            network::cidr_t vip_cidr;
            if (!network::cidr_from_string(vip.second, vip_cidr)) {
                continue;
            }
            address& addr = vip_cidr.first;
            uint8_t vmacAddr[6];
            vip.first.toUIntArray(vmacAddr);

            if (v4c && addr.is_v4())
                flowsVirtualDhcp(elPortSec, ofPort, vmacAddr, true);
            else if (v6c && addr.is_v6())
                flowsVirtualDhcp(elPortSec, ofPort, vmacAddr, false);
        }
    }
}

static void flowsEndpointSource(FlowEntryList& elSrc,
                                FlowEntryList& elEpLearn,
                                const Endpoint& endPoint,
                                uint32_t ofPort,
                                bool hasMac,
                                uint8_t* macAddr,
                                uint8_t unkFloodMode,
                                uint8_t bcastFloodMode,
                                uint32_t epgVnid,
                                uint32_t bdId,
                                uint32_t fgrpId,
                                uint32_t rdId) {
    if (ofPort == OFPP_NONE)
        return;

    if (hasMac) {
        actionSource(FlowBuilder().priority(140)
                     .inPort(ofPort).ethSrc(macAddr),
                     epgVnid, bdId, fgrpId, rdId)
            .build(elSrc);

        if (bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL &&
            unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
            // Prepopulate a stage1 learning entry for known EPs
            matchFd(FlowBuilder().priority(101)
                    .cookie(flow::cookie::PROACTIVE_LEARN),
                    fgrpId, true, macAddr)
                .action()
                .reg(MFF_REG7, ofPort)
                .output(ofPort)
                .controller()
                .parent().build(elEpLearn);
        }
    }

    if (endPoint.isPromiscuousMode()) {
        // if the source is unknown, but the interface is
        // promiscuous we allow the traffic into the learning
        // table
        actionSource(FlowBuilder().priority(138).inPort(ofPort),
                     epgVnid, bdId, fgrpId, rdId)
            .build(elSrc);
    }
}

static void matchActionServiceProto(FlowBuilder& flow, uint8_t proto,
                                    const Service::ServiceMapping& sm,
                                    bool forward, bool applyAction) {
    if (!proto) return;
    flow.proto(proto);

    if (!sm.getServicePort()) return;

    uint16_t s_port = sm.getServicePort().get();
    uint16_t nh_port = s_port;
    if (sm.getNextHopPort())
        nh_port = sm.getNextHopPort().get();

    if (nh_port == s_port) applyAction = false;

    if (forward) {
        flow.tpDst(s_port);
        if (applyAction)
            flow.action().l4Dst(nh_port, proto);
    } else {
        flow.tpSrc(nh_port);
        if (applyAction)
            flow.action().l4Src(s_port, proto);
    }
}

void IntFlowManager::handleEndpointUpdate(const string& uuid) {

    LOG(DEBUG) << "Updating endpoint " << uuid;

    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        switchManager.clearFlows(uuid, SEC_TABLE_ID);
        switchManager.clearFlows(uuid, SRC_TABLE_ID);
        switchManager.clearFlows(uuid, BRIDGE_TABLE_ID);
        switchManager.clearFlows(uuid, ROUTE_TABLE_ID);
        switchManager.clearFlows(uuid, LEARN_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_DST_TABLE_ID);
        switchManager.clearFlows(uuid, OUT_TABLE_ID);
        removeEndpointFromFloodGroup(uuid);
        return;
    }
    const Endpoint& endPoint = *epWrapper.get();
    uint8_t macAddr[6];
    bool hasMac = endPoint.getMAC() != boost::none;
    if (hasMac)
        endPoint.getMAC().get().toUIntArray(macAddr);

    /* check and parse the IP-addresses */
    boost::system::error_code ec;

    std::vector<address> ipAddresses;
    for (const string& ipStr : endPoint.getIPs()) {
        address addr = address::from_string(ipStr, ec);
        if (ec) {
            LOG(WARNING) << "Invalid endpoint IP: "
                         << ipStr << ": " << ec.message();
        } else {
            ipAddresses.push_back(addr);
        }
    }
    if (hasMac) {
        address_v6 linkLocalIp(network::construct_link_local_ip_addr(macAddr));
        if (endPoint.getIPs().find(linkLocalIp.to_string()) ==
            endPoint.getIPs().end())
            ipAddresses.push_back(linkLocalIp);
    }

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = endPoint.getInterfaceName();
    if (ofPortName) {
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());
    }

    FlowEntryList elPortSec;
    FlowEntryList elSrc;
    FlowEntryList elEpLearn;
    FlowEntryList elBridgeDst;
    FlowEntryList elRouteDst;
    FlowEntryList elServiceMap;
    FlowEntryList elOutput;

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    bool hasForwardingInfo = false;
    uint32_t epgVnid = 0, rdId = 0, bdId = 0, fgrpId = 0;
    optional<URI> fgrpURI, bdURI, rdURI;
    optional<shared_ptr<FloodDomain> > fd;

    uint8_t arpMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t ndMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;

    if (epgURI && getGroupForwardingInfo(epgURI.get(), epgVnid, rdURI,
                                         rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        hasForwardingInfo = true;
    }

    if (hasForwardingInfo)
        fd = agent.getPolicyManager().getFDForGroup(epgURI.get());
    if (fd) {
        // Irrespective of flooding scope (epg vs. flood-domain), the
        // properties of the flood-domain object decide how flooding
        // is done.

        arpMode = fd.get()
            ->getArpMode(AddressResModeEnumT::CONST_UNICAST);
        ndMode = fd.get()
            ->getNeighborDiscMode(AddressResModeEnumT::CONST_UNICAST);
        unkFloodMode = fd.get()
            ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
        bcastFloodMode = fd.get()
            ->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);
    }

    // Virtual DHCP is allowed even without forwarding resolution
    flowsEndpointDHCPSource(*this, elPortSec, elBridgeDst, endPoint, ofPort,
                            hasMac, macAddr, virtualDHCPEnabled,
                            hasForwardingInfo, epgVnid, rdId, bdId);

    if (hasForwardingInfo) {
        /* Port security flows */
        flowsEndpointPortSec(elPortSec, endPoint, ofPort,
                             hasMac, macAddr, ipAddresses);

        /* Source Table flows; applicable only to local endpoints */
        flowsEndpointSource(elSrc, elEpLearn, endPoint, ofPort,
                            hasMac, macAddr, unkFloodMode, bcastFloodMode,
                            epgVnid, bdId, fgrpId, rdId);

        /* Bridge, route, and output flows */
        if (bdId != 0 && hasMac && ofPort != OFPP_NONE) {
            FlowBuilder().priority(10).ethDst(macAddr).reg(4, bdId)
                .action()
                .reg(MFF_REG2, epgVnid)
                .reg(MFF_REG7, ofPort)
                .go(POL_TABLE_ID)
                .parent().build(elBridgeDst);
        }

        if (rdId != 0 && bdId != 0 && ofPort != OFPP_NONE) {
            uint8_t routingMode =
                agent.getPolicyManager().getEffectiveRoutingMode(epgURI.get());

            if (virtualRouterEnabled && hasMac &&
                routingMode == RoutingModeEnumT::CONST_ENABLED) {
                for (const address& ipAddr : ipAddresses) {
                    if (endPoint.isDiscoveryProxyMode()) {
                        // Auto-reply to ARP and NDP requests for endpoint
                        // IP addresses
                        flowsProxyDiscovery(*this, elBridgeDst, 20, ipAddr,
                                            macAddr, epgVnid, rdId, bdId);
                    } else {
                        if (arpMode != AddressResModeEnumT::CONST_FLOOD &&
                            ipAddr.is_v4()) {
                            FlowBuilder e1;
                            matchDestArp(e1, ipAddr, bdId, rdId);
                            if (arpMode == AddressResModeEnumT::CONST_UNICAST) {
                                // ARP optimization: broadcast -> unicast
                                actionDestEpArp(e1, epgVnid, ofPort, macAddr);
                            }
                            // else drop the ARP packet
                            e1.priority(20)
                                .build(elBridgeDst);
                        }

                        if (ndMode != AddressResModeEnumT::CONST_FLOOD &&
                            ipAddr.is_v6()) {
                            FlowBuilder e1;
                            matchDestNd(e1, &ipAddr, bdId, rdId);
                            if (ndMode == AddressResModeEnumT::CONST_UNICAST) {
                                // neighbor discovery optimization:
                                // broadcast -> unicast
                                actionDestEpArp(e1, epgVnid, ofPort, macAddr);
                            }
                            // else drop the ND packet
                            e1.priority(20)
                                .build(elBridgeDst);
                        }
                    }

                    if (network::is_link_local(ipAddr))
                        continue;

                    {
                        FlowBuilder e0;
                        matchDestDom(e0, 0, rdId);
                        e0.priority(500)
                            .ethDst(getRouterMacAddr())
                            .ipDst(ipAddr)
                            .action()
                            .reg(MFF_REG2, epgVnid)
                            .reg(MFF_REG7, ofPort)
                            .ethSrc(getRouterMacAddr())
                            .ethDst(macAddr)
                            .decTtl()
                            .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
                            .go(POL_TABLE_ID)
                            .parent().build(elRouteDst);
                    }

                }

                // IP address mappings
                for(const Endpoint::IPAddressMapping& ipm :
                        endPoint.getIPAddressMappings()) {
                    if (!ipm.getMappedIP() || !ipm.getEgURI())
                        continue;

                    address mappedIp =
                        address::from_string(ipm.getMappedIP().get(), ec);
                    if (ec) continue;

                    address floatingIp;
                    if (ipm.getFloatingIP()) {
                        floatingIp =
                            address::from_string(ipm.getFloatingIP().get(), ec);
                        if (ec) continue;
                        if (floatingIp.is_v4() != mappedIp.is_v4()) continue;
                    }

                    uint32_t fepgVnid, frdId, fbdId, ffdId;
                    optional<URI> ffdURI, fbdURI, frdURI;
                    if (!getGroupForwardingInfo(ipm.getEgURI().get(),
                                                fepgVnid, frdURI, frdId,
                                                fbdURI, fbdId, ffdURI, ffdId))
                        continue;

                    uint32_t nextHop = OFPP_NONE;
                    if (ipm.getNextHopIf()) {
                        nextHop = switchManager.getPortMapper()
                            .FindPort(ipm.getNextHopIf().get());
                        if (nextHop == OFPP_NONE) continue;
                    }
                    uint8_t nextHopMac[6];
                    const uint8_t* nextHopMacp = NULL;
                    if (ipm.getNextHopMAC()) {
                        ipm.getNextHopMAC().get().toUIntArray(nextHopMac);
                        nextHopMacp = nextHopMac;
                    }

                    flowsIpm(*this, elSrc, elBridgeDst, elRouteDst,
                             elOutput, macAddr, ofPort,
                             epgVnid, rdId, bdId, fgrpId,
                             fepgVnid, frdId, fbdId, ffdId,
                             mappedIp, floatingIp, nextHop,
                             nextHopMacp);
                }
            }

            // When traffic returns from a service interface, we have a
            // special table to map the return traffic that bypasses
            // normal network semantics and policy.  The service map table
            // is reachable only for traffic originating from service
            // interfaces.
            if (hasMac) {
                std::vector<address> anycastReturnIps;
                for (const string& ipStr : endPoint.getAnycastReturnIPs()) {
                    address addr = address::from_string(ipStr, ec);
                    if (ec) {
                        LOG(WARNING) << "Invalid anycast return IP: "
                                     << ipStr << ": " << ec.message();
                    } else {
                        anycastReturnIps.push_back(addr);
                    }
                }
                if (anycastReturnIps.size() == 0) {
                    anycastReturnIps = ipAddresses;
                }

                for (const address& ipAddr : anycastReturnIps) {
                    {
                        // Deliver packets sent to service address
                        FlowBuilder serviceDest;
                        matchDestDom(serviceDest, 0, rdId);
                        serviceDest
                            .priority(50)
                            .ipDst(ipAddr)
                            .action()
                            .ethSrc(getRouterMacAddr()).ethDst(macAddr)
                            .decTtl()
                            .output(ofPort)
                            .parent().build(elServiceMap);
                    }
                    flowsProxyDiscovery(*this, elServiceMap,
                                        51, ipAddr, macAddr, 0, rdId, 0);
                }
            }
        }

        if (ofPort != OFPP_NONE) {
            // If a packet has a routing action applied, we'll allow it to
            // hairpin for ordinary default output action or reverse NAT
            // output
            vector<uint64_t> metadata {
                flow::meta::ROUTED,
                    flow::meta::ROUTED | flow::meta::out::REV_NAT
                    };
            for (auto m : metadata) {
                FlowBuilder()
                    .priority(2)
                    .inPort(ofPort)
                    .metadata(m, flow::meta::ROUTED | flow::meta::out::MASK)
                    .reg(7, ofPort)
                    .action()
                    .output(OFPP_IN_PORT)
                    .parent().build(elOutput);
            }
        }
    }

    switchManager.writeFlow(uuid, SEC_TABLE_ID, elPortSec);
    switchManager.writeFlow(uuid, SRC_TABLE_ID, elSrc);
    switchManager.writeFlow(uuid, LEARN_TABLE_ID, elEpLearn);
    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, elBridgeDst);
    switchManager.writeFlow(uuid, ROUTE_TABLE_ID, elRouteDst);
    switchManager.writeFlow(uuid, SERVICE_DST_TABLE_ID, elServiceMap);
    switchManager.writeFlow(uuid, OUT_TABLE_ID, elOutput);

    if (fgrpURI && ofPort != OFPP_NONE) {
        updateEndpointFloodGroup(fgrpURI.get(), endPoint, ofPort,
                                 endPoint.isPromiscuousMode(),
                                 fd);
    } else {
        removeEndpointFromFloodGroup(uuid);
    }
}

void IntFlowManager::handleServiceUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating service " << uuid;

    ServiceManager& srvMgr = agent.getServiceManager();
    shared_ptr<const Service> asWrapper = srvMgr.getService(uuid);

    if (!asWrapper || !asWrapper->getDomainURI()) {
        switchManager.clearFlows(uuid, SEC_TABLE_ID);
        switchManager.clearFlows(uuid, BRIDGE_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_REV_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_DST_TABLE_ID);
        switchManager.clearFlows(uuid, SERVICE_NEXTHOP_TABLE_ID);
        idGen.erase(ID_NMSPC_SERVICE, uuid);
        return;
    }

    const Service& as = *asWrapper;

    FlowEntryList secFlows;
    FlowEntryList bridgeFlows;
    FlowEntryList serviceRevFlows;
    FlowEntryList serviceDstFlows;
    FlowEntryList serviceNextHopFlows;

    boost::system::error_code ec;

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = as.getInterfaceName();
    if (ofPortName)
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());

    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(),
                               as.getDomainURI().get());

    if (rd) {
        uint8_t smacAddr[6];
        const uint8_t* macAddr = smacAddr;
        if (as.getServiceMAC()) {
            as.getServiceMAC().get().toUIntArray(smacAddr);
        } else {
            macAddr = getRouterMacAddr();
        }

        uint32_t rdId = getId(RoutingDomain::CLASS_ID, as.getDomainURI().get());
        uint32_t ctMark = idGen.getId(ID_NMSPC_SERVICE, uuid);
        if (as.getInterfaceName())
            ctMark |= 1 << 31;

        for (auto const& sm : as.getServiceMappings()) {
            if (!sm.getServiceIP())
                continue;

            uint16_t zoneId = -1;
            if (conntrackEnabled && sm.isConntrackMode()) {
                zoneId = ctZoneManager.getId(as.getDomainURI()->toString());
                if (zoneId == static_cast<uint16_t>(-1))
                    LOG(ERROR) << "Could not allocate connection tracking"
                               << " zone for "
                               << as.getDomainURI().get();
            }

            address serviceAddr =
                address::from_string(sm.getServiceIP().get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid service IP: "
                             << sm.getServiceIP().get()
                             << ": " << ec.message();
                continue;
            }

            vector<address> nextHopAddrs;
            for (const string& ipstr : sm.getNextHopIPs()) {
                auto nextHopAddr = address::from_string(ipstr, ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service next hop IP: "
                                 << ipstr << ": " << ec.message();
                } else {
                    nextHopAddrs.push_back(nextHopAddr);
                }
            }

            uint8_t proto = 0;
            if (sm.getServiceProto()) {
                string protoStr = sm.getServiceProto().get();
                if ("udp" == protoStr)
                    proto = 17;
                else
                    proto = 6;
            }

            // Traffic sent to services is intercepted in the bridge
            // table, despite the fact that it is effectively
            // performing a routing action, to allow it to work with
            // flood domains configured to use mac learning.
            {
                FlowBuilder serviceDest;
                matchDestDom(serviceDest, 0, rdId);
                matchActionServiceProto(serviceDest, proto, sm, true, false);
                if (as.getServiceMAC() &&
                    as.getServiceMode() == Service::LOADBALANCER)
                    serviceDest.ethDst(macAddr);

                serviceDest.priority(50).ipDst(serviceAddr);
                if (as.getServiceMode() == Service::LOCAL_ANYCAST &&
                    encapType == ENCAP_VLAN) {
                    // Flows for load-balanced services add a VLAN tag
                    // to egress traffic in VLAN mode. Remove it if
                    // destination is a local anycast service.
                    serviceDest.action().popVlan();
                }
                serviceDest.action().ethSrc(getRouterMacAddr());

                if (!nextHopAddrs.empty()) {
                    // map traffic to service to the next hop IPs
                    // using DNAT semantics
                    if (as.getServiceMode() == Service::LOCAL_ANYCAST) {
                        serviceDest.action().ethDst(macAddr);
                    } else {
                        serviceDest.action().ethDst(getRouterMacAddr());
                    }
                    serviceDest.action()
                        .multipath(NX_HASH_FIELDS_SYMMETRIC_L3L4_UDP,
                                   1024,
                                   ActionBuilder::NX_MP_ALG_ITER_HASH,
                                   static_cast<uint16_t>(nextHopAddrs.size()-1),
                                   32, MFF_REG7)
                        .go(SERVICE_NEXTHOP_TABLE_ID);
                } else if (as.getServiceMode() == Service::LOCAL_ANYCAST &&
                           ofPort != OFPP_NONE) {
                    serviceDest.action()
                        .ethDst(macAddr).decTtl().output(ofPort);
                }

                serviceDest.build(bridgeFlows);
            }

            uint16_t link = 0;
            for (const address& nextHopAddr : nextHopAddrs) {
                {
                    FlowBuilder ipMap;
                    matchDestDom(ipMap, 0, rdId);
                    matchActionServiceProto(ipMap, proto, sm, true, true);
                    ipMap.ipDst(serviceAddr);

                    // use the first address as a "default" so that
                    // there is no transient case where there is no
                    // match while flows are updated.
                    if (link == 0) {
                        ipMap.priority(99);
                    } else {
                        ipMap.priority(100)
                            .reg(7, link);
                    }
                    ipMap.action().ipDst(nextHopAddr).decTtl();

                    if (as.getServiceMode() == Service::LOADBALANCER) {
                        if (zoneId != static_cast<uint16_t>(-1)) {
                            uint32_t metav = as.getInterfaceName()
                                ? flow::meta::FROM_SERVICE_INTERFACE
                                : 0;

                            ipMap.metadata(metav,
                                           flow::meta::FROM_SERVICE_INTERFACE);

                            ActionBuilder setMark;
                            setMark.reg(MFF_CT_MARK, ctMark);
                            ipMap.action()
                                .conntrack(ActionBuilder::CT_COMMIT,
                                           static_cast<mf_field_id>(0),
                                           zoneId, 0xff, 0, setMark);
                        }

                        ipMap.action()
                            .metadata(flow::meta::ROUTED, flow::meta::ROUTED)
                            .go(ROUTE_TABLE_ID);

                    } else if (as.getServiceMode() == Service::LOCAL_ANYCAST &&
                               ofPort != OFPP_NONE) {
                        ipMap.action().output(ofPort);
                    }

                    ipMap.build(serviceNextHopFlows);
                }
                if (as.getServiceMode() == Service::LOADBALANCER) {
                    // For load balanced services reverse traffic is
                    // handled with normal policy semantics
                    if (zoneId != static_cast<uint16_t>(-1)) {
                        FlowBuilder ipRevMapCt;
                        matchDestDom(ipRevMapCt, 0, rdId);
                        matchActionServiceProto(ipRevMapCt, proto, sm,
                                                false, false);
                        ipRevMapCt.conntrackState(0, FlowBuilder::CT_TRACKED);
                        ipRevMapCt.priority(100)
                            .ipSrc(nextHopAddr);
                        if (encapType == ENCAP_VLAN) {
                            ipRevMapCt.action()
                                .pushVlan()
                                .regMove(MFF_REG0, MFF_VLAN_VID);
                        }
                        ipRevMapCt.action()
                            .conntrack(0, static_cast<mf_field_id>(0),
                                       zoneId, SRC_TABLE_ID);
                        ipRevMapCt.build(serviceRevFlows);
                    }
                    {
                        FlowBuilder ipRevMap;
                        matchDestDom(ipRevMap, 0, rdId);
                        matchActionServiceProto(ipRevMap, proto, sm,
                                                false, true);

                        ipRevMap.priority(100)
                            .ipSrc(nextHopAddr)
                            .action()
                            .ethSrc(macAddr)
                            .ipSrc(serviceAddr)
                            .decTtl();
                        if (zoneId != static_cast<uint16_t>(-1)) {
                            ipRevMap
                                .conntrackState(FlowBuilder::CT_TRACKED |
                                                FlowBuilder::CT_ESTABLISHED,
                                                FlowBuilder::CT_TRACKED |
                                                FlowBuilder::CT_ESTABLISHED |
                                                FlowBuilder::CT_INVALID |
                                                FlowBuilder::CT_NEW);

                            ipRevMap.ctMark(ctMark);
                        }
                        if (!as.getInterfaceName()) {
                            ipRevMap.action()
                                .metadata(flow::meta::ROUTED,
                                          flow::meta::ROUTED)
                                .go(BRIDGE_TABLE_ID);
                        } else if (ofPort != OFPP_NONE) {
                            if (as.getIfaceVlan()) {
                                ipRevMap.action()
                                    .pushVlan()
                                    .setVlanVid(as.getIfaceVlan().get());
                            }
                            ipRevMap.action()
                                .ethDst(getRouterMacAddr())
                                .output(ofPort);
                        }
                        ipRevMap.build(serviceRevFlows);
                    }
                }

                link += 1;
            }

            if (ofPort != OFPP_NONE) {
                if (as.getServiceMode() == Service::LOCAL_ANYCAST) {
                    // For anycast services with a service interface,
                    // traffic sent from the interface is intercepted in
                    // the security table to prevent normal processing
                    // semantics, since there is otherwise no way for the
                    // policy to allow the traffic
                    if (nextHopAddrs.empty())
                        nextHopAddrs.emplace_back();

                    for (const address& nextHopAddr : nextHopAddrs) {
                        FlowBuilder svcIp;
                        svcIp.priority(100)
                            .inPort(ofPort)
                            .ethSrc(macAddr)
                            .action()
                            .reg(MFF_REG6, rdId);

                        if (nextHopAddr != address()) {
                            // If there is a next hop mapping, map the return
                            // traffic from service interface using DNAT
                            // semantics
                            svcIp.ipSrc(nextHopAddr)
                                .action()
                                .ipSrc(serviceAddr)
                                .decTtl()
                                .metadata(flow::meta::ROUTED,
                                          flow::meta::ROUTED);
                        } else {
                            svcIp.ipSrc(serviceAddr);
                        }

                        svcIp.action()
                            .go(SERVICE_DST_TABLE_ID);
                        svcIp.build(secFlows);

                        if (serviceAddr.is_v4()) {
                            // Note that v6 neighbor discovery is
                            // handled by the regular IP rules
                            FlowBuilder().priority(100)
                                .inPort(ofPort)
                                .ethSrc(macAddr)
                                .arpSrc(nextHopAddr != address()
                                        ? nextHopAddr : serviceAddr)
                                .action()
                                .reg(MFF_REG6, rdId)
                                .go(SERVICE_DST_TABLE_ID)
                                .parent().build(secFlows);
                        }
                    }

                    // Reply to ARP/ND requests for the service address
                    flowsProxyDiscovery(*this, bridgeFlows,
                                        51, serviceAddr, macAddr,
                                        0, rdId, 0);

                    if (sm.getGatewayIP()) {
                        address gwAddr =
                            address::from_string(sm.getGatewayIP().get(),
                                                 ec);
                        if (ec) {
                            LOG(WARNING) << "Invalid service gateway IP: "
                                         << sm.getGatewayIP().get()
                                         << ": " << ec.message();
                        } else {
                            flowsProxyDiscovery(*this, serviceDstFlows, 31,
                                                gwAddr, getRouterMacAddr(),
                                                0, rdId, 0, true, macAddr);
                        }
                    }
                }
            }
        }

        if (as.getServiceMode() == Service::LOADBALANCER &&
            ofPort != OFPP_NONE) {
            // For load balancer services with a service interface, we
            // allow traffic from the service interface, strip any
            // VLAN tag, and set the policy applied bit.  Because
            // these packets are already allowed by a service policy,
            // we bypass regular policy semantics for these packets,
            // but use the regular forwarding pipeline.

            IntFlowManager::EncapType serviceEncapType =
                IntFlowManager::ENCAP_NONE;
            uint32_t proxyVnid = 0;
            if (as.getIfaceVlan()) {
                proxyVnid = as.getIfaceVlan().get();
                serviceEncapType = IntFlowManager::ENCAP_VLAN;
            }

            FlowBuilder svcIface;
            svcIface.priority(90)
                .inPort(ofPort);
            if (as.getIfaceVlan()) {
                svcIface.vlan(as.getIfaceVlan().get());
                svcIface.action().popVlan();
            }
            svcIface.action()
                .reg(MFF_REG0, proxyVnid)
                .reg(MFF_REG6, rdId)
                .metadata(flow::meta::POLICY_APPLIED |
                          flow::meta::FROM_SERVICE_INTERFACE,
                          flow::meta::POLICY_APPLIED|
                          flow::meta::FROM_SERVICE_INTERFACE)
                .go(BRIDGE_TABLE_ID);
            svcIface.build(secFlows);

            if (as.getIfaceIP()) {
                // Reply to ARP/ND requests for the iface address
                address ifaceAddr =
                    address::from_string(as.getIfaceIP().get(), ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service interface address: "
                                 << as.getIfaceIP().get()
                                 << ":" << ec.message();
                } else {
                    // XXX TODO this won't work for ipv6 with packet-in
                    flowsProxyDiscovery(bridgeFlows,
                                        51, ifaceAddr, macAddr,
                                        proxyVnid, rdId, 0, false, NULL,
                                        ofPort, serviceEncapType);
                }
            }
        }
    }

    switchManager.writeFlow(uuid, SEC_TABLE_ID, secFlows);
    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, bridgeFlows);
    switchManager.writeFlow(uuid, SERVICE_REV_TABLE_ID, serviceRevFlows);
    switchManager.writeFlow(uuid, SERVICE_NEXTHOP_TABLE_ID,
                            serviceNextHopFlows);
    switchManager.writeFlow(uuid, SERVICE_DST_TABLE_ID, serviceDstFlows);
}

void IntFlowManager::updateEPGFlood(const URI& epgURI, uint32_t epgVnid,
                                    uint32_t fgrpId, address epgTunDst) {
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
    optional<shared_ptr<FloodDomain> > fd =
        agent.getPolicyManager().getFDForGroup(epgURI);
    if (fd) {
        bcastFloodMode =
            fd.get()->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);
    }

    FlowEntryList grpDst;
    {
        // deliver broadcast/multicast traffic to the group table
        FlowBuilder mcast;
        matchFd(mcast, fgrpId, true);
        mcast.priority(10)
            .reg(0, epgVnid);
        if (bcastFloodMode == BcastFloodModeEnumT::CONST_ISOLATED) {
            // In isolated mode deliver only if policy has already
            // been applied (i.e. it comes from the tunnel uplink)
            mcast.metadata(flow::meta::POLICY_APPLIED,
                           flow::meta::POLICY_APPLIED);
        }
        switch (getEncapType()) {
        case ENCAP_VLAN:
            break;
        case ENCAP_VXLAN:
        case ENCAP_IVXLAN:
        default:
            mcast.action().reg(MFF_REG7, epgTunDst.to_v4().to_ulong());
            break;
        }
        mcast.action()
            .metadata(flow::meta::out::FLOOD, flow::meta::out::MASK)
            .go(IntFlowManager::OUT_TABLE_ID);
        mcast.build(grpDst);
    }
    switchManager.writeFlow(epgURI.toString(), BRIDGE_TABLE_ID, grpDst);
}

void IntFlowManager::createStaticFlows() {
    uint32_t tunPort = getTunnelPort();

    LOG(DEBUG) << "Writing static flows";

    {
        // static port security flows
        FlowEntryList portSec;
        {
            // Drop IP traffic that doesn't have the correct source
            // address
            FlowBuilder().priority(25).ethType(eth::type::ARP).build(portSec);
            FlowBuilder().priority(25).ethType(eth::type::IP).build(portSec);
            FlowBuilder().priority(25).ethType(eth::type::IPV6).build(portSec);
        }
        {
            // Allow DHCP requests but not replies
            actionSecAllow(flowutils::match_dhcp_req(FlowBuilder().priority(27),
                                                     true))
                .build(portSec);
        }
        {
            // Allow IPv6 autoconfiguration (DHCP & router solicitiation)
            // requests but not replies
            actionSecAllow(flowutils::match_dhcp_req(FlowBuilder().priority(27),
                                                     false))
                .build(portSec);
            actionSecAllow(FlowBuilder().priority(27)
                           .ethType(eth::type::IPV6)
                           .proto(58)
                           .tpSrc(ND_ROUTER_SOLICIT)
                           .tpDst(0))
                .build(portSec);
        }
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // allow all traffic from the tunnel uplink through the port
            // security table
            actionSecAllow(FlowBuilder().priority(50).inPort(tunPort))
                .build(portSec);
        }
        switchManager.writeFlow("static", SEC_TABLE_ID, portSec);
    }
    {
        // For non-service flows, proceed to the bridge table
        FlowBuilder nonServiceFlow;
        nonServiceFlow.priority(1).action().go(BRIDGE_TABLE_ID);
        switchManager.writeFlow("static", SERVICE_REV_TABLE_ID,
                                nonServiceFlow);
    }
    {
        FlowEntryList policyStatic;

        // Bypass policy for flows that have the bypass policy bit set
        FlowBuilder().priority(PolicyManager::MAX_POLICY_RULE_PRIORITY + 51)
            .metadata(flow::meta::FROM_SERVICE_INTERFACE,
                      flow::meta::FROM_SERVICE_INTERFACE)
            .action().go(IntFlowManager::OUT_TABLE_ID)
            .parent().build(policyStatic);

        // Block flows from the uplink when not allowed by
        // higher-priority per-EPG rules to allow them.
        FlowBuilder().priority(PolicyManager::MAX_POLICY_RULE_PRIORITY + 50)
            .metadata(flow::meta::POLICY_APPLIED, flow::meta::POLICY_APPLIED)
            .build(policyStatic);

        // Implicitly allow ARP and neighbor discovery unless a rule
        // blocks them.
        FlowBuilder().priority(10)
            .ethType(eth::type::ARP)
            .action().go(IntFlowManager::OUT_TABLE_ID)
            .parent().build(policyStatic);
        FlowBuilder().priority(10)
            .ethType(eth::type::IPV6)
            .proto(58)
            .tpSrc(ND_NEIGHBOR_SOLICIT)
            .tpDst(0)
            .action().go(IntFlowManager::OUT_TABLE_ID)
            .parent().build(policyStatic);
        FlowBuilder().priority(10)
            .ethType(eth::type::IPV6)
            .proto(58)
            .tpSrc(ND_NEIGHBOR_ADVERT)
            .tpDst(0)
            .action().go(IntFlowManager::OUT_TABLE_ID)
            .parent().build(policyStatic);

        switchManager.writeFlow("static", POL_TABLE_ID, policyStatic);
    }
    {
        FlowEntryList unknownTunnelBr;
        FlowEntryList unknownTunnelRt;
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // Output to the tunnel interface, bypassing policy
            // note that if the flood domain is set to flood unknown, then
            // there will be a higher-priority rule installed for that
            // flood domain.
            actionOutputToEPGTunnel(FlowBuilder().priority(1))
                .build(unknownTunnelBr);

            if (virtualRouterEnabled) {
                actionOutputToEPGTunnel(FlowBuilder().priority(1))
                    .build(unknownTunnelRt);
            }
        }
        switchManager.writeFlow("static", BRIDGE_TABLE_ID, unknownTunnelBr);
        switchManager.writeFlow("static", ROUTE_TABLE_ID, unknownTunnelRt);
    }
    {
        FlowEntryList outFlows;
        outFlows.push_back(flowutils::default_out_flow());
        {
            FlowBuilder().priority(1)
                .metadata(flow::meta::out::REV_NAT,
                          flow::meta::out::MASK)
                .action().outputReg(MFF_REG7)
                .parent().build(outFlows);
        }
        {
            // send reverse NAT ICMP error packets to controller
            revNatICMPFlows(outFlows, true, 3 ); // unreachable
            revNatICMPFlows(outFlows, true, 11); // time exceeded
            revNatICMPFlows(outFlows, true, 12); // param
        }

        switchManager.writeFlow("static", OUT_TABLE_ID, outFlows);
    }
}

void IntFlowManager::handleEndpointGroupDomainUpdate(const URI& epgURI) {
    LOG(DEBUG) << "Updating endpoint-group " << epgURI;

    const string& epgId = epgURI.toString();

    uint32_t tunPort = getTunnelPort();
    address epgTunDst = getEPGTunnelDst(epgURI);

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {  // EPG removed
        switchManager.clearFlows(epgId, SRC_TABLE_ID);
        switchManager.clearFlows(epgId, POL_TABLE_ID);
        switchManager.clearFlows(epgId, OUT_TABLE_ID);
        switchManager.clearFlows(epgId, BRIDGE_TABLE_ID);
        updateMulticastList(boost::none, epgURI);
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!getGroupForwardingInfo(epgURI, epgVnid, rdURI, rdId,
                                bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    FlowEntryList uplinkMatch;
    if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
        // In flood mode we send all traffic from the uplink to the
        // learning table.  Otherwise move to the destination mapper
        // table as normal.

        uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
        uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
        optional<shared_ptr<FloodDomain> > epgFd = polMgr.getFDForGroup(epgURI);
        if (epgFd) {
            unkFloodMode = epgFd.get()
                ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
            bcastFloodMode = epgFd.get()
                ->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);
        }

        uint8_t nextTable = IntFlowManager::SERVICE_REV_TABLE_ID;
        if (unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD &&
            bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL)
            nextTable = IntFlowManager::LEARN_TABLE_ID;

        // Assign the source registers based on the VNID from the
        // tunnel uplink
        actionSource(matchEpg(FlowBuilder().priority(149).inPort(tunPort),
                              encapType, epgVnid),
                     epgVnid, bdId, fgrpId, rdId,
                     nextTable, encapType, true)
            .build(uplinkMatch);
    }
    switchManager.writeFlow(epgId, SRC_TABLE_ID, uplinkMatch);

    {
        uint8_t intraGroup = IntraGroupPolicyEnumT::CONST_ALLOW;
        optional<shared_ptr<EpGroup> > epg =
            EpGroup::resolve(agent.getFramework(), epgURI);
        if (epg && epg.get()->isIntraGroupPolicySet()) {
            intraGroup = epg.get()->getIntraGroupPolicy().get();
        }

        FlowBuilder intraGroupFlow;
        uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY + 100;
        switch (intraGroup) {
        case IntraGroupPolicyEnumT::CONST_DENY:
            prio = PolicyManager::MAX_POLICY_RULE_PRIORITY + 200;
            break;
        case IntraGroupPolicyEnumT::CONST_REQUIRE_CONTRACT:
            // Only automatically allow intra-EPG traffic if its come
            // from the uplink and therefore already had policy
            // applied.
            intraGroupFlow.metadata(flow::meta::POLICY_APPLIED,
                                    flow::meta::POLICY_APPLIED);
            /* fall through */
        case IntraGroupPolicyEnumT::CONST_ALLOW:
        default:
            intraGroupFlow.action().go(IntFlowManager::OUT_TABLE_ID);
            break;
        }
        flowutils::match_group(intraGroupFlow, prio, epgVnid, epgVnid);
        switchManager.writeFlow(epgId, POL_TABLE_ID, intraGroupFlow);
    }

    if (virtualRouterEnabled && rdId != 0 && bdId != 0) {
        updateGroupSubnets(epgURI, bdId, rdId);

        FlowEntryList bridgel;
        uint8_t routingMode =
            agent.getPolicyManager().getEffectiveRoutingMode(epgURI);

        if (routingMode == RoutingModeEnumT::CONST_ENABLED) {
            FlowBuilder().priority(2)
                .reg(4, bdId)
                .action()
                .go(ROUTE_TABLE_ID)
                .parent().build(bridgel);

            if (routerAdv) {
                FlowBuilder r;
                matchDestNd(r, NULL, bdId, rdId, ND_ROUTER_SOLICIT);
                r.priority(20).cookie(flow::cookie::NEIGH_DISC);
                r.action().controller();
                r.build(bridgel);

                if (!isSyncing)
                    advertManager.scheduleInitialRouterAdv();
            }
        }
        switchManager.writeFlow(bdURI.get().toString(),
                                BRIDGE_TABLE_ID, bridgel);
    }

    updateEPGFlood(epgURI, epgVnid, fgrpId, epgTunDst);

    FlowEntryList egOutFlows;
    {
        // Output table action to resubmit the flow to the bridge
        // table with source registers set to the current EPG
        FlowBuilder().priority(10)
            .reg(7, epgVnid)
            .metadata(flow::meta::out::RESUBMIT_DST, flow::meta::out::MASK)
            .action()
            .reg(MFF_REG0, epgVnid)
            .reg(MFF_REG4, bdId)
            .reg(MFF_REG5, fgrpId)
            .reg(MFF_REG6, rdId)
            .reg(MFF_REG7, (uint32_t)0)
            .reg64(MFF_METADATA, flow::meta::ROUTED)
            .resubmit(OFPP_IN_PORT, BRIDGE_TABLE_ID)
            .parent().build(egOutFlows);
    }
    if (encapType != ENCAP_NONE && tunPort != OFPP_NONE) {
        {
            // Output table action to output to the tunnel appropriate for
            // the source EPG
            FlowBuilder tunnelOut;
            tunnelOut.priority(10)
                .reg(0, epgVnid)
                .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK);
            actionTunnelMetadata(tunnelOut.action(), encapType, epgTunDst);
            tunnelOut.action().output(tunPort);
            tunnelOut.build(egOutFlows);
        }
        if (encapType != ENCAP_VLAN) {
            // If destination is the router mac, override EPG tunnel
            // and send to unicast tunnel
            FlowBuilder tunnelOutRtr;
            tunnelOutRtr.priority(11)
                .reg(0, epgVnid)
                .ethDst(getRouterMacAddr())
                .metadata(flow::meta::out::TUNNEL, flow::meta::out::MASK);
            actionTunnelMetadata(tunnelOutRtr.action(),
                                 encapType, getTunnelDst());
            tunnelOutRtr.action().output(tunPort);
            tunnelOutRtr.build(egOutFlows);
        }
    }
    switchManager.writeFlow(epgId, OUT_TABLE_ID, egOutFlows);

    unordered_set<string> epUuids;
    EndpointManager& epMgr = agent.getEndpointManager();
    epMgr.getEndpointsForIPMGroup(epgURI, epUuids);
    std::unordered_set<URI> ipmRds;
    for (const string& uuid : epUuids) {
        std::shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
        if (!ep) continue;
        const boost::optional<opflex::modb::URI>& egURI = ep->getEgURI();
        if (!egURI) continue;
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > rd =
            polMgr.getRDForGroup(egURI.get());
        if (rd)
            ipmRds.insert(rd.get()->getURI());
    }
    for (const URI& rdURI : ipmRds) {
        // update routing domains that have references to the
        // IP-mapping EPG to ensure external subnets are correctly
        // mapped.
        rdConfigUpdated(rdURI);
    }

    // note this combines with the IPM group endpoints from above:
    epMgr.getEndpointsForGroup(epgURI, epUuids);
    for (const string& uuid : epUuids) {
        advertManager.scheduleEndpointAdv(uuid);
        endpointUpdated(uuid);
    }

    PolicyManager::uri_set_t contractURIs;
    polMgr.getContractsForGroup(epgURI, contractURIs);
    for (const URI& contract : contractURIs) {
        contractUpdated(contract);
    }

    optional<string> epgMcastIp = polMgr.getMulticastIPForGroup(epgURI);
    updateMulticastList(epgMcastIp, epgURI);
    optional<string> fdcMcastIp;
    optional<shared_ptr<FloodContext> > fdCtx =
        polMgr.getFloodContextForGroup(epgURI);
    if (fdCtx) {
        if (fdCtx.get()->getMulticastGroupIP())
            fdcMcastIp = fdCtx.get()->getMulticastGroupIP().get();
        updateMulticastList(fdcMcastIp, fdCtx.get()->getURI());
    }
}

void IntFlowManager::updateGroupSubnets(const URI& egURI, uint32_t bdId,
                                        uint32_t rdId) {
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(egURI, subnets);

    uint32_t tunPort = getTunnelPort();

    for (shared_ptr<Subnet>& sn : subnets) {
        FlowEntryList el;

        optional<address> routerIp =
            PolicyManager::getRouterIpForSubnet(*sn);

        // Reply to ARP/ND requests for the router IP only from local
        // endpoints.
        if (routerIp) {
            if (routerIp.get().is_v4()) {
                if (tunPort != OFPP_NONE) {
                    FlowBuilder e0;
                    e0.priority(22).inPort(tunPort);
                    matchDestArp(e0, routerIp.get(), bdId, rdId);
                    e0.build(el);
                }

                FlowBuilder e1;
                e1.priority(20);
                matchDestArp(e1, routerIp.get(), bdId, rdId);
                actionArpReply(e1, getRouterMacAddr(), routerIp.get());
                e1.build(el);
            } else {
                address lladdr =
                    network::construct_link_local_ip_addr(routerMac);
                if (tunPort != OFPP_NONE) {
                    FlowBuilder e0;
                    e0.priority(22)
                        .inPort(tunPort).cookie(flow::cookie::NEIGH_DISC);
                    matchDestNd(e0, &lladdr, bdId, rdId);
                    e0.build(el);
                }

                FlowBuilder e1;
                e1.priority(20).cookie(flow::cookie::NEIGH_DISC);
                matchDestNd(e1, &lladdr, bdId, rdId);
                e1.action().controller();
                e1.build(el);
            }
        }
        switchManager.writeFlow(sn->getURI().toString(), BRIDGE_TABLE_ID, el);
    }
}

void IntFlowManager::handleRoutingDomainUpdate(const URI& rdURI) {
    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(), rdURI);

    if (!rd) {
        LOG(DEBUG) << "Cleaning up for RD: " << rdURI;
        switchManager.clearFlows(rdURI.toString(), NAT_IN_TABLE_ID);
        switchManager.clearFlows(rdURI.toString(), ROUTE_TABLE_ID);
        switchManager.clearFlows(rdURI.toString(), POL_TABLE_ID);
        idGen.erase(getIdNamespace(RoutingDomain::CLASS_ID), rdURI.toString());
        ctZoneManager.erase(rdURI.toString());
        return;
    }
    LOG(DEBUG) << "Updating routing domain " << rdURI;

    FlowEntryList rdRouteFlows;
    FlowEntryList rdNatFlows;
    boost::system::error_code ec;
    uint32_t tunPort = getTunnelPort();
    uint32_t rdId = getId(RoutingDomain::CLASS_ID, rdURI);

    // For subnets internal to a routing domain, we want to perform
    // ordinary routing without mapping to external network.  These
    // rules are lower priority than the rules that will handle
    // routing to endpoints that are local to this vswitch, so the
    // action is to output to the uplink tunnel.  Match using
    // longest-prefix.

    network::subnets_t intSubnets;

    vector<shared_ptr<RoutingDomainToIntSubnetsRSrc> > subnets_list;
    rd.get()->resolveGbpRoutingDomainToIntSubnetsRSrc(subnets_list);
    for (shared_ptr<RoutingDomainToIntSubnetsRSrc>& subnets_ref :
             subnets_list) {
        optional<URI> subnets_uri = subnets_ref->getTargetURI();
        PolicyManager::resolveSubnets(agent.getFramework(),
                                      subnets_uri, intSubnets);
    }
    std::shared_ptr<const RDConfig> rdConfig =
        agent.getExtraConfigManager().getRDConfig(rdURI);
    if (rdConfig) {
        for (const std::string& cidrSn :
                 rdConfig->getInternalSubnets()) {
            network::cidr_t cidr;
            if (network::cidr_from_string(cidrSn, cidr)) {
                intSubnets.insert(make_pair(cidr.first.to_string(),
                                            cidr.second));
            } else {
                LOG(ERROR) << "Invalid CIDR subnet: " << cidrSn;
            }
        }
    }
    for (const network::subnet_t& sn : intSubnets) {
        address addr = address::from_string(sn.first, ec);
        if (ec) continue;

        FlowBuilder snr;
        matchSubnet(snr, rdId, 300, addr, sn.second, false);
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            actionOutputToEPGTunnel(snr);
        }
        snr.build(rdRouteFlows);
    }

    // If we miss the local endpoints and the internal subnets, check
    // each of the external layer 3 networks.  Match using
    // longest-prefix.
    vector<shared_ptr<L3ExternalDomain> > extDoms;
    rd.get()->resolveGbpL3ExternalDomain(extDoms);
    for (shared_ptr<L3ExternalDomain>& extDom : extDoms) {
        vector<shared_ptr<L3ExternalNetwork> > extNets;
        extDom->resolveGbpL3ExternalNetwork(extNets);

        for (shared_ptr<L3ExternalNetwork> net : extNets) {
            uint32_t netVnid = getExtNetVnid(net->getURI());
            vector<shared_ptr<ExternalSubnet> > extSubs;
            net->resolveGbpExternalSubnet(extSubs);
            optional<shared_ptr<L3ExternalNetworkToNatEPGroupRSrc> > natRef =
                net->resolveGbpL3ExternalNetworkToNatEPGroupRSrc();
            optional<uint32_t> natEpgVnid = boost::none;
            if (natRef) {
                optional<URI> natEpg = natRef.get()->getTargetURI();
                if (natEpg)
                    natEpgVnid =
                        agent.getPolicyManager().getVnidForGroup(natEpg.get());
            }

            for (shared_ptr<ExternalSubnet> extsub : extSubs) {
                if (!extsub->isAddressSet() || !extsub->isPrefixLenSet())
                    continue;
                address addr =
                    address::from_string(extsub->getAddress().get(), ec);
                if (ec) continue;

                {
                    FlowBuilder snr;
                    matchSubnet(snr, rdId, 150, addr,
                                extsub->getPrefixLen(0), false);

                    if (natRef) {
                        // For external networks mapped to a NAT EPG,
                        // set the next hop action to NAT_OUT
                        if (natEpgVnid) {
                            snr.action()
                                .reg(MFF_REG2, netVnid)
                                .reg(MFF_REG7, natEpgVnid.get())
                                .metadata(flow::meta::out::NAT,
                                          flow::meta::out::MASK)
                                .go(POL_TABLE_ID);
                        }
                        // else drop until resolved
                    } else if (tunPort != OFPP_NONE &&
                               encapType != ENCAP_NONE) {
                        // For other external networks, output to the tunnel
                        actionOutputToEPGTunnel(snr);
                    }
                    // else drop the packets
                    snr.build(rdRouteFlows);
                }
                {
                    FlowBuilder snn;
                    matchSubnet(snn, rdId, 150, addr,
                                extsub->getPrefixLen(0), true);
                    snn.action()
                        .reg(MFF_REG0, netVnid)
                        // We want to ensure that on the final
                        // delivery of the packet we perform
                        // protocol-specific reverse mapping.  This
                        // doesn't let us do hop-by-hop translations
                        // however.
                        //
                        // Also remove policy applied since we're
                        // changing the effective EPG and need to
                        // apply policy again.
                        .metadata(flow::meta::out::REV_NAT,
                                  flow::meta::out::MASK |
                                  flow::meta::POLICY_APPLIED)
                        .go(POL_TABLE_ID)
                        .parent().build(rdNatFlows);
                }
            }
        }
    }

    switchManager.writeFlow(rdURI.toString(), NAT_IN_TABLE_ID, rdNatFlows);
    switchManager.writeFlow(rdURI.toString(), ROUTE_TABLE_ID, rdRouteFlows);

    std::unordered_set<string> uuids;
    agent.getServiceManager().getServicesByDomain(rdURI, uuids);
    for (const string& uuid : uuids) {
        serviceUpdated(uuid);
    }

    // create drop entry in POL_TABLE_ID for each routingDomain
    // this entry is needed to count all dropped packets per
    // routingDomain and summed up for all the routingDomain to
    // calculate per tenant drop counter.
    switchManager.writeFlow(rdURI.toString(), POL_TABLE_ID,
             FlowBuilder().priority(1).reg(6, rdId));
}

void
IntFlowManager::handleDomainUpdate(class_id_t cid, const URI& domURI) {

    switch (cid) {
    case RoutingDomain::CLASS_ID:
        handleRoutingDomainUpdate(domURI);
        break;
    case Subnet::CLASS_ID:
        if (!Subnet::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for Subnet: " << domURI;
            switchManager.clearFlows(domURI.toString(), BRIDGE_TABLE_ID);
        }
        break;
    case BridgeDomain::CLASS_ID:
        if (!BridgeDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for BD: " << domURI;
            switchManager.clearFlows(domURI.toString(), BRIDGE_TABLE_ID);
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    case FloodDomain::CLASS_ID:
        if (!FloodDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for FD: " << domURI;
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    case FloodContext::CLASS_ID:
        if (!FloodContext::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for FloodContext: " << domURI;
            if (removeFromMulticastList(domURI))
                multicastGroupsUpdated();
        }
        break;
    case L3ExternalNetwork::CLASS_ID:
        if (!L3ExternalNetwork::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for L3ExtNet: " << domURI;
            idGen.erase(getIdNamespace(cid), domURI.toString());
        }
        break;
    }
}

/**
 * Construct a bucket object with the specified bucket ID.
 */
static
ofputil_bucket *createBucket(uint32_t bucketId) {
    ofputil_bucket *bkt = (ofputil_bucket *)malloc(sizeof(ofputil_bucket));
    bkt->weight = 0;
    bkt->bucket_id = bucketId;
    bkt->watch_port = OFPP_ANY;
    bkt->watch_group = OFPG_ANY;
    return bkt;
}

GroupEdit::Entry
IntFlowManager::createGroupMod(uint16_t type, uint32_t groupId,
                            const Ep2PortMap& ep2port, bool onlyPromiscuous) {
    GroupEdit::Entry entry(new GroupEdit::GroupMod());
    entry->mod->command = type;
    entry->mod->group_id = groupId;

    for (const Ep2PortMap::value_type& kv : ep2port) {
        if (onlyPromiscuous && !kv.second.second)
            continue;

        ofputil_bucket *bkt = createBucket(kv.second.first);
        ActionBuilder ab;
        ab.output(kv.second.first)
            .build(bkt);
        ovs_list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    uint32_t tunPort = getTunnelPort();
    if (type != OFPGC11_DELETE && tunPort != OFPP_NONE &&
        encapType != ENCAP_NONE) {
        ofputil_bucket *bkt = createBucket(tunPort);
        ActionBuilder ab;
        actionTunnelMetadata(ab, encapType);
        ab.output(tunPort)
            .build(bkt);
        ovs_list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    return entry;
}

uint32_t IntFlowManager::getPromId(uint32_t fgrpId) {
    return ((1<<31) | fgrpId);
}

void
IntFlowManager::updateEndpointFloodGroup(const opflex::modb::URI& fgrpURI,
                                      const Endpoint& endPoint, uint32_t epPort,
                                      bool isPromiscuous,
                                      optional<shared_ptr<FloodDomain> >& fd) {
    const std::string& epUUID = endPoint.getUUID();
    std::pair<uint32_t, bool> epPair(epPort, isPromiscuous);
    uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
    string fgrpStrId = "fd:" + fgrpURI.toString();
    FloodGroupMap::iterator fgrpItr = floodGroupMap.find(fgrpURI);

    uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
    if (fd) {
        unkFloodMode =
            fd.get()->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
        bcastFloodMode =
            fd.get()->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);
    }

    if (fgrpItr != floodGroupMap.end()) {
        Ep2PortMap& epMap = fgrpItr->second;
        Ep2PortMap::iterator epItr = epMap.find(epUUID);

        if (epItr == epMap.end()) {
            /* EP not attached to this flood-group, check and remove
             * if it was attached to a different one */
            removeEndpointFromFloodGroup(epUUID);
        }
        if (epItr == epMap.end() || epItr->second != epPair) {
            epMap[epUUID] = epPair;
            GroupEdit::Entry e = createGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
            switchManager.writeGroupMod(e);
            GroupEdit::Entry e2 =
                createGroupMod(OFPGC11_MODIFY, getPromId(fgrpId), epMap, true);
            switchManager.writeGroupMod(e2);
        }
    } else {
        /* Remove EP attachment to old floodgroup, if any */
        removeEndpointFromFloodGroup(epUUID);
        floodGroupMap[fgrpURI][epUUID] = epPair;
        GroupEdit::Entry e =
            createGroupMod(OFPGC11_ADD, fgrpId, floodGroupMap[fgrpURI]);
        switchManager.writeGroupMod(e);
        GroupEdit::Entry e2 = createGroupMod(OFPGC11_ADD, getPromId(fgrpId),
                                             floodGroupMap[fgrpURI], true);
        switchManager.writeGroupMod(e2);
    }

    FlowEntryList fdOutput;
    {
        // Output table action to output to the flood group appropriate
        // for the source EPG.
        FlowBuilder().priority(10).reg(5, fgrpId)
            .metadata(flow::meta::out::FLOOD, flow::meta::out::MASK)
            .action()
            .group(fgrpId)
            .parent().build(fdOutput);
    }
    switchManager.writeFlow(fgrpStrId, OUT_TABLE_ID, fdOutput);

    FlowEntryList grpDst;
    FlowEntryList learnDst;
    if (bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL &&
        unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
        // go to the learning table on an unknown unicast
        // destination in flood mode
        matchFd(FlowBuilder().priority(5), fgrpId, false)
            .action()
            .go(IntFlowManager::LEARN_TABLE_ID)
            .parent().build(grpDst);

        // Deliver unknown packets in the flood domain when
        // learning to the controller for reactive flow setup.
        matchFd(FlowBuilder().priority(5).cookie(flow::cookie::PROACTIVE_LEARN),
                fgrpId, false)
            .action().controller()
            .parent().build(learnDst);
    }
    switchManager.writeFlow(fgrpStrId, BRIDGE_TABLE_ID, grpDst);
    switchManager.writeFlow(fgrpStrId, LEARN_TABLE_ID, learnDst);
}

void IntFlowManager::removeEndpointFromFloodGroup(const std::string& epUUID) {
    for (FloodGroupMap::iterator itr = floodGroupMap.begin();
         itr != floodGroupMap.end();
         ++itr) {
        const URI& fgrpURI = itr->first;
        Ep2PortMap& epMap = itr->second;
        if (epMap.erase(epUUID) == 0) {
            continue;
        }
        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        uint16_t type = epMap.empty() ?
                OFPGC11_DELETE : OFPGC11_MODIFY;
        GroupEdit::Entry e0 =
                createGroupMod(type, fgrpId, epMap);
        GroupEdit::Entry e1 =
                createGroupMod(type, getPromId(fgrpId), epMap, true);
        if (epMap.empty()) {
            string fgrpStrId = "fd:" + fgrpURI.toString();
            switchManager.clearFlows(fgrpStrId, OUT_TABLE_ID);
            switchManager.clearFlows(fgrpStrId, BRIDGE_TABLE_ID);
            switchManager.clearFlows(fgrpStrId, LEARN_TABLE_ID);
            floodGroupMap.erase(fgrpURI);
        }
        switchManager.writeGroupMod(e0);
        switchManager.writeGroupMod(e1);
        break;
    }
}

void IntFlowManager::addContractRules(FlowEntryList& entryList,
                             const uint32_t pvnid,
                             const uint32_t cvnid,
                             bool allowBidirectional,
                             const PolicyManager::rule_list_t& rules) {
    for (const shared_ptr<PolicyRule>& pc : rules) {
        uint8_t dir = pc->getDirection();
        const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
        const opflex::modb::URI& ruleURI = cls.get()->getURI();
        uint64_t cookie = getId(L24Classifier::CLASS_ID, ruleURI);
        flowutils::ClassAction act = flowutils::CA_DENY;
        if (pc->getAllow())
            act = flowutils::CA_ALLOW;

        if (dir == DirectionEnumT::CONST_BIDIRECTIONAL &&
            !allowBidirectional) {
            dir = DirectionEnumT::CONST_IN;
        }
        if (dir == DirectionEnumT::CONST_IN ||
            dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
            flowutils::add_classifier_entries(*cls, act,
                                              boost::none,
                                              boost::none,
                                              IntFlowManager::OUT_TABLE_ID,
                                              pc->getPriority(),
                                              OFPUTIL_FF_SEND_FLOW_REM,
                                              cookie,
                                              cvnid, pvnid,
                                              entryList);
        }
        if (dir == DirectionEnumT::CONST_OUT ||
            dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
            flowutils::add_classifier_entries(*cls, act,
                                              boost::none,
                                              boost::none,
                                              IntFlowManager::OUT_TABLE_ID,
                                              pc->getPriority(),
                                              OFPUTIL_FF_SEND_FLOW_REM,
                                              cookie,
                                              pvnid, cvnid,
                                              entryList);
        }
    }
}

void
IntFlowManager::handleContractUpdate(const opflex::modb::URI& contractURI) {
    LOG(DEBUG) << "Updating contract " << contractURI;

    const string& contractId = contractURI.toString();
    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.contractExists(contractURI)) {  // Contract removed
        switchManager.clearFlows(contractId, POL_TABLE_ID);
        return;
    }
    PolicyManager::uri_set_t provURIs;
    PolicyManager::uri_set_t consURIs;
    PolicyManager::uri_set_t intraURIs;
    polMgr.getContractProviders(contractURI, provURIs);
    polMgr.getContractConsumers(contractURI, consURIs);
    polMgr.getContractIntra(contractURI, intraURIs);

    typedef unordered_set<uint32_t> id_set_t;
    id_set_t provIds;
    id_set_t consIds;
    id_set_t intraIds;
    getGroupVnid(provURIs, provIds);
    getGroupVnid(consURIs, consIds);
    getGroupVnid(intraURIs, intraIds);

    PolicyManager::rule_list_t rules;
    polMgr.getContractRules(contractURI, rules);

    LOG(DEBUG) << "Update for contract " << contractURI
               << ", #prov=" << provIds.size()
               << ", #cons=" << consIds.size()
               << ", #intra=" << intraIds.size()
               << ", #rules=" << rules.size();

    FlowEntryList entryList;

    for (const uint32_t& pvnid : provIds) {
        for (const uint32_t& cvnid : consIds) {
            if (pvnid == cvnid)
                continue;

            /*
             * Collapse bidirectional rules - if consumer 'cvnid' is
             * also a provider and provider 'pvnid' is also a
             * consumer, then add entry for cvnid to pvnid traffic
             * only.
             */
            bool allowBidirectional =
                provIds.find(cvnid) == provIds.end() ||
                consIds.find(pvnid) == consIds.end();

            addContractRules(entryList, pvnid, cvnid,
                             allowBidirectional,
                             rules);
        }
    }
    for (const uint32_t& ivnid : intraIds) {
        addContractRules(entryList, ivnid, ivnid, false, rules);
    }

    switchManager.writeFlow(contractId, POL_TABLE_ID, entryList);
}

void IntFlowManager::initPlatformConfig() {

    using namespace modelgbp::platform;

    optional<shared_ptr<Config> > config =
        Config::resolve(agent.getFramework(),
                        agent.getPolicyManager().getOpflexDomain());
    mcastTunDst = boost::none;
    if (config) {
        optional<const string&> ipStr =
            config.get()->getMulticastGroupIP();
        if (ipStr) {
            boost::system::error_code ec;
            address ip(address::from_string(ipStr.get(), ec));
            if (ec) {
                LOG(ERROR) << "Invalid multicast tunnel destination: "
                           << ipStr.get() << ": " << ec.message();
            } else if (!ip.is_v4()) {
                LOG(ERROR) << "Multicast tunnel destination must be IPv4: "
                           << ipStr.get();
            } else {
                mcastTunDst = ip;
            }
        }
        updateMulticastList(
            ipStr ? optional<string>(ipStr.get()) : optional<string>(),
            config.get()->getURI());
    }
}

void IntFlowManager::handleConfigUpdate(const opflex::modb::URI& configURI) {
    LOG(DEBUG) << "Updating platform config " << configURI;
    initPlatformConfig();

    /* Directly update the group-table */
    updateGroupTable();
}

void IntFlowManager::updateGroupTable() {
    for (FloodGroupMap::value_type& kv : floodGroupMap) {
        const URI& fgrpURI = kv.first;
        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        Ep2PortMap& epMap = kv.second;

        GroupEdit::Entry e1 = createGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
        switchManager.writeGroupMod(e1);
        GroupEdit::Entry e2 = createGroupMod(OFPGC11_MODIFY,
                                             getPromId(fgrpId), epMap, true);
        switchManager.writeGroupMod(e2);
    }
}

void IntFlowManager::handlePortStatusUpdate(const string& portName,
                                            uint32_t) {
    LOG(DEBUG) << "Port-status update for " << portName;
    if (portName == encapIface) {
        initPlatformConfig();
        createStaticFlows();

        PolicyManager::uri_set_t epgURIs;
        agent.getPolicyManager().getGroups(epgURIs);
        for (const URI& epg : epgURIs) {
            egDomainUpdated(epg);
        }
        PolicyManager::uri_set_t rdURIs;
        agent.getPolicyManager().getRoutingDomains(rdURIs);
        for (const URI& rd : rdURIs) {
            rdConfigUpdated(rd);
        }
        /* Directly update the group-table */
        updateGroupTable();
    } else {
        {
            unordered_set<string> uuids;
            agent.getEndpointManager().getEndpointsByIface(portName, uuids);
            agent.getEndpointManager().getEndpointsByIpmNextHopIf(portName,
                                                                  uuids);
            for (const string& uuid : uuids) {
                endpointUpdated(uuid);
            }
        }
        {
            unordered_set<string> uuids;
            agent.getServiceManager()
                .getServicesByIface(portName, uuids);
            for (const string& uuid : uuids) {
                serviceUpdated(uuid);
            }
        }
    }
}

void IntFlowManager::getGroupVnid(const unordered_set<URI>& uris,
    /* out */unordered_set<uint32_t>& ids) {
    PolicyManager& pm = agent.getPolicyManager();
    for (const URI& u : uris) {
        optional<uint32_t> vnid = pm.getVnidForGroup(u);
        optional<shared_ptr<RoutingDomain> > rd;
        if (vnid) {
            rd = pm.getRDForGroup(u);
        } else {
            rd = pm.getRDForL3ExtNet(u);
            if (rd) {
                vnid = getExtNetVnid(u);
            }
        }
        if (vnid && rd) {
            ids.insert(vnid.get());
        }
    }
}

typedef std::function<bool(opflex::ofcore::OFFramework&,
                           const string&,
                           const string&)> IdCb;

static const IdCb ID_NAMESPACE_CB[] =
    {IdGenerator::uriIdGarbageCb<FloodDomain>,
     IdGenerator::uriIdGarbageCb<BridgeDomain>,
     IdGenerator::uriIdGarbageCb<RoutingDomain>,
     IdGenerator::uriIdGarbageCb<L3ExternalNetwork>,
     IdGenerator::uriIdGarbageCb<L24Classifier>};

static bool serviceIdGarbageCb(ServiceManager& serviceManager,
                               const std::string& nmspc,
                               const std::string& str) {
    return (bool)serviceManager.getService(str);
}

void IntFlowManager::cleanup() {
    for (size_t i = 0; i < sizeof(ID_NAMESPACE_CB)/sizeof(IdCb); i++) {
        string ns(ID_NAMESPACES[i]);
        IdGenerator::garbage_cb_t gcb =
            bind(ID_NAMESPACE_CB[i], ref(agent.getFramework()), _1, _2);
        agent.getAgentIOService()
            .dispatch(bind(&IdGenerator::collectGarbage, ref(idGen),
                           ns, gcb));
    }

    IdGenerator::garbage_cb_t sgcb =
        bind(serviceIdGarbageCb, std::ref(agent.getServiceManager()), _1, _2);
    agent.getAgentIOService()
        .dispatch(bind(&IdGenerator::collectGarbage, ref(idGen),
                       ID_NMSPC_SERVICE, sgcb));
}

const char * IntFlowManager::getIdNamespace(class_id_t cid) {
    const char *nmspc = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   nmspc = ID_NMSPC_RD; break;
    case BridgeDomain::CLASS_ID:    nmspc = ID_NMSPC_BD; break;
    case FloodDomain::CLASS_ID:     nmspc = ID_NMSPC_FD; break;
    case L3ExternalNetwork::CLASS_ID: nmspc = ID_NMSPC_EXTNET; break;
    case L24Classifier::CLASS_ID: nmspc = ID_NMSPC_L24CLASS_RULE; break;
    default:
        assert(false);
    }
    return nmspc;
}


uint32_t IntFlowManager::getId(class_id_t cid, const URI& uri) {
    return idGen.getId(getIdNamespace(cid), uri.toString());
}

uint32_t IntFlowManager::getExtNetVnid(const opflex::modb::URI& uri) {
    // External networks are assigned private VNIDs that have bit 31 (MSB)
    // set to 1. This is fine because legal VNIDs are 24-bits or less.
    return (getId(L3ExternalNetwork::CLASS_ID, uri) | (1 << 31));
}

void IntFlowManager::updateMulticastList(const optional<string>& mcastIp,
                                      const URI& uri) {
    bool update = false;
    if ((encapType != ENCAP_VXLAN && encapType != ENCAP_IVXLAN) ||
        getTunnelPort() == OFPP_NONE ||
        !mcastIp) {
        update |= removeFromMulticastList(uri);

    } else {
        boost::system::error_code ec;
        address ip(address::from_string(mcastIp.get(), ec));
        if (ec || !ip.is_multicast()) {
            LOG(WARNING) << "Ignoring invalid/unsupported multicast "
                "subscription IP: " << mcastIp.get();
            return;
        }
        MulticastMap::iterator itr = mcastMap.find(mcastIp.get());
        if (itr != mcastMap.end()) {
            UriSet& uris = itr->second;
            UriSet::iterator jtr = uris.find(uri);
            if (jtr == uris.end()) {
                // remove old association, if any
                update |= removeFromMulticastList(uri);
                uris.insert(uri);
            }
        } else {
            // remove old association, if any
            update |= removeFromMulticastList(uri);
            mcastMap[mcastIp.get()].insert(uri);
            update |= !isSyncing;
        }
    }

    if (update)
        multicastGroupsUpdated();
}

bool IntFlowManager::removeFromMulticastList(const URI& uri) {
    for (MulticastMap::value_type& kv : mcastMap) {
        UriSet& uris = kv.second;
        if (uris.erase(uri) > 0 && uris.empty()) {
            mcastMap.erase(kv.first);
            return !isSyncing;
        }
    }
    return false;
}

static const std::string MCAST_QUEUE_ITEM("mcast-groups");

void IntFlowManager::multicastGroupsUpdated() {
    taskQueue.dispatch(MCAST_QUEUE_ITEM,
                       bind(&IntFlowManager::writeMulticastGroups, this));
}

void IntFlowManager::writeMulticastGroups() {
    if (mcastGroupFile == "") return;

    pt::ptree tree;
    pt::ptree groups;
    for (MulticastMap::value_type& kv : mcastMap)
        groups.push_back(std::make_pair("", pt::ptree(kv.first)));
    tree.add_child("multicast-groups", groups);

    try {
        pt::write_json(mcastGroupFile, tree);
    } catch (pt::json_parser_error e) {
        LOG(ERROR) << "Could not write multicast group file "
                   << e.what();
    }
}

std::vector<FlowEdit>
IntFlowManager::reconcileFlows(std::vector<TableState> flowTables,
                               std::vector<FlowEntryList>& recvFlows) {
    // special handling for learning table - reconcile using
    // PacketInHandler reactive reconciler
    FlowEntryList learnFlows;
    recvFlows[IntFlowManager::LEARN_TABLE_ID].swap(learnFlows);
    for (const FlowEntryPtr& fe : learnFlows) {
        if (!pktInHandler.reconcileReactiveFlow(fe)) {
            recvFlows[IntFlowManager::LEARN_TABLE_ID].push_back(fe);
        }
    }

    return SwitchStateHandler::reconcileFlows(flowTables, recvFlows);
}

void IntFlowManager::checkGroupEntry(GroupMap& recvGroups,
                                     uint32_t groupId,
                                     const Ep2PortMap& epMap,
                                     bool prom, GroupEdit& ge) {
    GroupMap::iterator itr;
    itr = recvGroups.find(groupId);
    uint16_t comm = OFPGC11_ADD;
    GroupEdit::Entry recv;
    if (itr != recvGroups.end()) {
        comm = OFPGC11_MODIFY;
        recv = itr->second;
    }
    GroupEdit::Entry e0 = createGroupMod(comm, groupId, epMap, prom);
    if (!GroupEdit::groupEq(e0, recv)) {
        ge.edits.push_back(e0);
    }
    if (itr != recvGroups.end()) {
        recvGroups.erase(itr);
    }
}

GroupEdit IntFlowManager::reconcileGroups(GroupMap& recvGroups) {
    GroupEdit ge;
    for (FloodGroupMap::value_type& kv : floodGroupMap) {
        const URI& fgrpURI = kv.first;
        Ep2PortMap& epMap = kv.second;

        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        checkGroupEntry(recvGroups, fgrpId, epMap, false, ge);

        uint32_t promFdId = getPromId(fgrpId);
        checkGroupEntry(recvGroups, promFdId, epMap, true, ge);
    }
    Ep2PortMap tmp;
    for (const GroupMap::value_type& kv : recvGroups) {
        GroupEdit::Entry e0 = createGroupMod(OFPGC11_DELETE, kv.first, tmp);
        ge.edits.push_back(e0);
    }
    return ge;
}

void IntFlowManager::completeSync() {
    writeMulticastGroups();
    advertManager.start();
}

} // namespace opflexagent
