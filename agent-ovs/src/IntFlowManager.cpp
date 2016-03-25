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
#include <boost/foreach.hpp>
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

#include "logging.h"
#include "Endpoint.h"
#include "EndpointManager.h"
#include "EndpointListener.h"
#include "SwitchConnection.h"
#include "IntFlowManager.h"
#include "TableState.h"
#include "PacketInHandler.h"
#include "Packets.h"
#include "FlowUtils.h"
#include "FlowConstants.h"

using std::string;
using std::vector;
using std::ostringstream;
using boost::bind;
using boost::algorithm::trim;
using boost::ref;
using boost::optional;
using boost::shared_ptr;
using boost::unordered_set;
using boost::unordered_map;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::unique_lock;
using boost::mutex;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::class_id_t;

namespace pt = boost::property_tree;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

namespace ovsagent {

static const char* ID_NAMESPACES[] =
    {"floodDomain", "bridgeDomain", "routingDomain",
     "contract", "externalNetwork"};

static const char* ID_NMSPC_FD     = ID_NAMESPACES[0];
static const char* ID_NMSPC_BD     = ID_NAMESPACES[1];
static const char* ID_NMSPC_RD     = ID_NAMESPACES[2];
static const char* ID_NMSPC_CON    = ID_NAMESPACES[3];
static const char* ID_NMSPC_EXTNET = ID_NAMESPACES[4];

IntFlowManager::IntFlowManager(Agent& agent_,
                               SwitchManager& switchManager_,
                               IdGenerator& idGen_) :
    agent(agent_), switchManager(switchManager_), idGen(idGen_),
    taskQueue(agent.getAgentIOService()),
    encapType(ENCAP_NONE),
    floodScope(FLOOD_DOMAIN), tunnelPortStr("4789"),
    virtualRouterEnabled(false), routerAdv(false),
    virtualDHCPEnabled(false),
    pktInHandler(agent, *this),
    advertManager(agent, *this), stopping(false) {
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
    pktInHandler.setPortMapper(&switchManager.getPortMapper());
    advertManager.setPortMapper(&switchManager.getPortMapper());

    // set up flow reader
    pktInHandler.setFlowReader(&switchManager.getFlowReader());

    // Register connection handlers
    SwitchConnection* conn = switchManager.getConnection();
    conn->RegisterMessageHandler(OFPTYPE_PACKET_IN, &pktInHandler);
    pktInHandler.registerConnection(conn);
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
    SwitchConnection* conn = switchManager.getConnection();
    conn->UnregisterMessageHandler(OFPTYPE_PACKET_IN, &pktInHandler);

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

void IntFlowManager::setEndpointAdv(bool endpointAdv) {
    advertManager.enableEndpointAdv(endpointAdv);
}

void IntFlowManager::setMulticastGroupFile(const std::string& mcastGroupFile) {
    this->mcastGroupFile = mcastGroupFile;
}

/** Source table helper functions */
static void
setSourceAction(FlowEntry *fe, uint32_t epgId,
                uint32_t bdId,  uint32_t fgrpId,  uint32_t l3Id,
                uint8_t nextTable = IntFlowManager::BRIDGE_TABLE_ID,
                IntFlowManager::EncapType encapType = IntFlowManager::ENCAP_NONE,
                bool setPolicyApplied = false)
{
    ActionBuilder ab;
    if (encapType == IntFlowManager::ENCAP_VLAN)
        ab.SetPopVlan();
    ab.SetRegLoad(MFF_REG0, epgId);
    ab.SetRegLoad(MFF_REG4, bdId);
    ab.SetRegLoad(MFF_REG5, fgrpId);
    ab.SetRegLoad(MFF_REG6, l3Id);
    if (setPolicyApplied) {
        ab.SetWriteMetadata(flow::meta::POLICY_APPLIED,
                            flow::meta::POLICY_APPLIED);
    }
    ab.SetGotoTable(nextTable);

    ab.Build(fe->entry);
}

static void
setSourceMatchEp(FlowEntry *fe, uint16_t prio, uint32_t ofPort,
        const uint8_t *mac) {
    fe->entry->table_id = IntFlowManager::SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, ofPort);
    if (mac)
        match_set_dl_src(&fe->entry->match, mac);
}

static void
setSourceMatchEpg(FlowEntry *fe, IntFlowManager::EncapType encapType,
                  uint16_t prio, uint32_t tunPort, uint32_t epgId) {
    fe->entry->table_id = IntFlowManager::SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, tunPort);
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        match_set_dl_vlan(&fe->entry->match, htons(0xfff & epgId));
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        match_set_tun_id(&fe->entry->match, htonll(epgId));
        break;
    }
}

/** Destination table helper functions */
static void
setDestMatchEpMac(FlowEntry *fe, uint16_t prio, const uint8_t *mac,
        uint32_t bdId) {
    fe->entry->table_id = IntFlowManager::BRIDGE_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_dst(&fe->entry->match, mac);
    match_set_reg(&fe->entry->match, 4 /* REG4 */, bdId);
}

static inline void fill_in6_addr(struct in6_addr& addr, const address_v6& ip) {
    address_v6::bytes_type bytes = ip.to_bytes();
    std::memcpy(&addr, bytes.data(), bytes.size());
}

static void
addMatchIp(FlowEntry *fe, const address& ip, bool src = false) {
    if (ip.is_v4()) {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
        if (src)
            match_set_nw_src(&fe->entry->match, htonl(ip.to_v4().to_ulong()));
        else
            match_set_nw_dst(&fe->entry->match, htonl(ip.to_v4().to_ulong()));
    } else {
        struct in6_addr addr;
        fill_in6_addr(addr, ip.to_v6());

        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
        if (src)
            match_set_ipv6_src(&fe->entry->match, &addr);
        else
            match_set_ipv6_dst(&fe->entry->match, &addr);
    }
}

static void
setDestMatchEp(FlowEntry *fe, uint16_t prio, const uint8_t *mac,
               const address& ip, uint32_t l3Id) {
    fe->entry->table_id = IntFlowManager::ROUTE_TABLE_ID;
    fe->entry->priority = prio;
    if (mac)
        match_set_dl_dst(&fe->entry->match, mac);
    addMatchIp(fe, ip);
    match_set_reg(&fe->entry->match, 6 /* REG6 */, l3Id);
}

static void
setDestMatchArp(FlowEntry *fe, uint16_t prio, const address& ip,
                uint32_t bdId, uint32_t l3Id) {
    fe->entry->table_id = IntFlowManager::BRIDGE_TABLE_ID;
    fe->entry->priority = prio;
    match *m = &fe->entry->match;
    match_set_dl_type(m, htons(ETH_TYPE_ARP));
    match_set_nw_proto(m, ARP_OP_REQUEST);
    match_set_dl_dst(m, packets::MAC_ADDR_BROADCAST);
    match_set_nw_dst(m, htonl(ip.to_v4().to_ulong()));
    if (bdId != 0)
        match_set_reg(m, 4 /* REG4 */, bdId);
    match_set_reg(m, 6 /* REG6 */, l3Id);
}

static void
setDestMatchNd(FlowEntry *fe, uint16_t prio, const address* ip,
               uint32_t bdId, uint32_t rdId, uint64_t cookie = 0,
               uint8_t type = ND_NEIGHBOR_SOLICIT) {
    fe->entry->table_id = IntFlowManager::BRIDGE_TABLE_ID;
    if (cookie != 0)
        fe->entry->cookie = cookie;
    fe->entry->priority = prio;
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
    match_set_nw_proto(&fe->entry->match, 58 /* ICMP */);
    // OVS uses tp_src for ICMPV6_TYPE
    match_set_tp_src(&fe->entry->match, htons(type));
    // OVS uses tp_dst for ICMPV6_CODE
    match_set_tp_dst(&fe->entry->match, 0);
    if (bdId != 0)
        match_set_reg(&fe->entry->match, 4 /* REG4 */, bdId);
    match_set_reg(&fe->entry->match, 6 /* REG6 */, rdId);
    match_set_dl_dst_masked(&fe->entry->match,
                            packets::MAC_ADDR_MULTICAST,
                            packets::MAC_ADDR_MULTICAST);

    if (ip) {
        struct in6_addr addr;
        fill_in6_addr(addr, ip->to_v6());
        match_set_nd_target(&fe->entry->match, &addr);
    }

}

static void
setMatchFd(FlowEntry *fe, uint16_t prio, uint32_t fgrpId, bool broadcast,
           uint8_t tableId, uint8_t* dstMac = NULL, uint64_t cookie = 0) {
    fe->entry->table_id = tableId;
    if (cookie != 0)
        fe->entry->cookie = cookie;
    fe->entry->priority = prio;
    match *m = &fe->entry->match;
    match_set_reg(&fe->entry->match, 5 /* REG5 */, fgrpId);
    if (broadcast)
        match_set_dl_dst_masked(m,
                                packets::MAC_ADDR_MULTICAST,
                                packets::MAC_ADDR_MULTICAST);
    if (dstMac)
        match_set_dl_dst(m, dstMac);
}

static void
addMatchSubnet(FlowEntry *fe, address& ip, uint8_t prefixLen, bool src) {
   if (ip.is_v4()) {
       uint32_t mask = (prefixLen != 0)
           ? (~((uint32_t)0) << (32 - prefixLen))
           : 0;
       uint32_t addr = ip.to_v4().to_ulong() & mask;
       match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
       if (src)
           match_set_nw_src_masked(&fe->entry->match, htonl(addr), htonl(mask));
       else
           match_set_nw_dst_masked(&fe->entry->match, htonl(addr), htonl(mask));
   } else {
       struct in6_addr mask;
       struct in6_addr addr;
       packets::compute_ipv6_subnet(ip.to_v6(), prefixLen, &mask, &addr);

       match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
       if (src)
           match_set_ipv6_src_masked(&fe->entry->match, &addr, &mask);
       else
           match_set_ipv6_dst_masked(&fe->entry->match, &addr, &mask);
   }
}

static void
setMatchSubnet(FlowEntry *fe, uint32_t rdId,
               uint16_t prioBase, uint8_t tableId,
               address& ip, uint8_t prefixLen, bool src) {
   fe->entry->table_id = tableId;

   if (ip.is_v4() && prefixLen > 32) {
       prefixLen = 32;
   } else if (prefixLen > 128) {
       prefixLen = 128;
   }
   fe->entry->priority = prioBase + prefixLen;
   match_set_reg(&fe->entry->match, 6 /* REG6 */, rdId);
   addMatchSubnet(fe, ip, prefixLen, src);
}

void IntFlowManager::setActionTunnelMetadata(ActionBuilder& ab,
                                             IntFlowManager::EncapType type,
                                             const optional<address>& tunDst) {
    switch (type) {
    case IntFlowManager::ENCAP_VLAN:
        ab.SetPushVlan();
        ab.SetRegMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case IntFlowManager::ENCAP_VXLAN:
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        if (tunDst) {
            if (tunDst->is_v4()) {
                ab.SetRegLoad(MFF_TUN_DST, tunDst->to_v4().to_ulong());
            } else {
                // this should be unreachable
                LOG(WARNING) << "Ipv6 tunnel destination unsupported";
            }
        } else {
            ab.SetRegMove(MFF_REG7, MFF_TUN_DST);
        }
        // fall through
    case IntFlowManager::ENCAP_IVXLAN:
        break;
    default:
        break;
    }
}

static void
setDestActionEpMac(FlowEntry *fe, uint32_t epgId, uint32_t port) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    ab.SetGotoTable(IntFlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
setDestActionEp(FlowEntry *fe, uint32_t epgId, uint32_t port,
                const uint8_t *specialMac, const uint8_t *dstMac,
                uint8_t nextTable = IntFlowManager::POL_TABLE_ID) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    ab.SetEthSrcDst(specialMac, dstMac);
    ab.SetDecNwTtl();
    ab.SetGotoTable(nextTable);

    ab.Build(fe->entry);
}

static void
setDestActionEpArp(FlowEntry *fe, uint32_t epgId, uint32_t port,
                   const uint8_t *dstMac) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    ab.SetEthSrcDst(NULL, dstMac);
    ab.SetGotoTable(IntFlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
setDestActionArpReply(FlowEntry *fe, const uint8_t *mac, const address& ip,
                      uint32_t outPort = OFPP_IN_PORT,
                      IntFlowManager::EncapType type= IntFlowManager::ENCAP_NONE,
                      uint32_t replyEpg = 0) {
    ActionBuilder ab;
    ab.SetRegMove(MFF_ETH_SRC, MFF_ETH_DST);
    ab.SetRegLoad(MFF_ETH_SRC, mac);
    ab.SetRegLoad16(MFF_ARP_OP, ARP_OP_REPLY);
    ab.SetRegMove(MFF_ARP_SHA, MFF_ARP_THA);
    ab.SetRegLoad(MFF_ARP_SHA, mac);
    ab.SetRegMove(MFF_ARP_SPA, MFF_ARP_TPA);
    ab.SetRegLoad(MFF_ARP_SPA, ip.to_v4().to_ulong());
    if (type != IntFlowManager::ENCAP_NONE) {
        ab.SetRegLoad(MFF_REG0, replyEpg);
        ab.SetWriteMetadata(flow::meta::out::TUNNEL,
                            flow::meta::out::MASK);
        ab.SetGotoTable(IntFlowManager::OUT_TABLE_ID);
    } else {
        ab.SetOutputToPort(outPort);
    }

    ab.Build(fe->entry);
}

static void
addActionRevNatDest(ActionBuilder& ab, uint32_t epgVnid, uint32_t bdId,
                    uint32_t fgrpId, uint32_t rdId, uint32_t ofPort) {
    ab.SetRegLoad(MFF_REG2, epgVnid);
    ab.SetRegLoad(MFF_REG4, bdId);
    ab.SetRegLoad(MFF_REG5, fgrpId);
    ab.SetRegLoad(MFF_REG6, rdId);
    ab.SetRegLoad(MFF_REG7, ofPort);
    ab.SetGotoTable(IntFlowManager::NAT_IN_TABLE_ID);
}

static void
setActionRevNatDest(FlowEntry *fe, uint32_t epgVnid, uint32_t bdId,
                    uint32_t fgrpId, uint32_t rdId, uint32_t ofPort,
                    const uint8_t* routerMac) {
    ActionBuilder ab;
    ab.SetEthSrcDst(routerMac, NULL);
    addActionRevNatDest(ab, epgVnid, bdId, fgrpId, rdId, ofPort);
    ab.Build(fe->entry);
}

static void
setActionRevDNatDest(FlowEntry *fe, uint32_t epgVnid, uint32_t bdId,
                     uint32_t fgrpId, uint32_t rdId, uint32_t ofPort,
                     const uint8_t* routerMac, const uint8_t* macAddr,
                     address& mappedIp) {
    ActionBuilder ab;
    ab.SetEthSrcDst(routerMac, macAddr);
    ab.SetIpDst(mappedIp);
    ab.SetDecNwTtl();
    addActionRevNatDest(ab, epgVnid, bdId, fgrpId, rdId, ofPort);
    ab.Build(fe->entry);
}

static void
setActionEPGFdBroadcast(FlowEntry *fe, IntFlowManager::EncapType encapType,
                        address epgTunDst) {
    ActionBuilder ab;
    switch (encapType) {
    case IntFlowManager::ENCAP_VLAN:
        break;
    case IntFlowManager::ENCAP_VXLAN:
    case IntFlowManager::ENCAP_IVXLAN:
    default:
        ab.SetRegLoad(MFF_REG7, epgTunDst.to_v4().to_ulong());
        break;
    }
    ab.SetWriteMetadata(flow::meta::out::FLOOD,
                        flow::meta::out::MASK);
    ab.SetGotoTable(IntFlowManager::OUT_TABLE_ID);
    ab.Build(fe->entry);
}

static void
setDestActionOutputToEPGTunnel(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetWriteMetadata(flow::meta::out::TUNNEL,
                        flow::meta::out::MASK);
    ab.SetGotoTable(IntFlowManager::OUT_TABLE_ID);
    ab.Build(fe->entry);
}

static void
setActionGotoLearn(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetGotoTable(IntFlowManager::LEARN_TABLE_ID);
    ab.Build(fe->entry);
}

static void
setActionController(FlowEntry *fe, uint32_t epgId = 0,
                    uint16_t max_len = 0xffff,
                    uint64_t metadata = 0) {
    ActionBuilder ab;
    if (epgId != 0)
        ab.SetRegLoad(MFF_REG0, epgId);
    if (metadata)
        ab.SetRegLoad64(MFF_METADATA, metadata);
    ab.SetController(max_len);
    ab.Build(fe->entry);
}

/** Security table */

static void
setSecurityMatchEpMac(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac) {
    fe->entry->table_id = IntFlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
}

static void
setSecurityMatchIP(FlowEntry *fe, uint16_t prio, bool v4) {
    fe->entry->table_id = IntFlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (v4)
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
    else
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
}

static void
setSecurityMatchEp(FlowEntry *fe, uint16_t prio, uint32_t port,
                   const uint8_t *epMac, const address& epIp) {
    fe->entry->table_id = IntFlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac != NULL)
        match_set_dl_src(&fe->entry->match, epMac);
    if (epIp.is_v4()) {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
        match_set_nw_src(&fe->entry->match, htonl(epIp.to_v4().to_ulong()));
    } else {
        struct in6_addr addr;
        fill_in6_addr(addr, epIp.to_v6());

        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
        match_set_ipv6_src(&fe->entry->match, &addr);
    }
}

static void
setSecurityMatchEpArp(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac, const address* epIp,
        uint8_t prefixLen = 32) {
    fe->entry->table_id = IntFlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_ARP));
    if (epIp != NULL)
        match_set_nw_src_masked(&fe->entry->match,
                                htonl(epIp->to_v4().to_ulong()),
                                htonl(packets::get_subnet_mask_v4(prefixLen)));
}

static void
setSecurityMatchEpND(FlowEntry *fe, uint16_t prio, uint32_t port,
                     const uint8_t *epMac, const address* epIp,
                     bool routerAdv, uint8_t prefixLen = 128) {
    fe->entry->table_id = IntFlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
    match_set_nw_proto(&fe->entry->match, 58 /* ICMP */);
    // OVS uses tp_src for ICMPV6_TYPE
    if (routerAdv)
        match_set_tp_src(&fe->entry->match, htons(ND_ROUTER_ADVERT));
    else
        match_set_tp_src(&fe->entry->match, htons(ND_NEIGHBOR_ADVERT));

    if (epIp != NULL) {
        struct in6_addr addr;
        struct in6_addr mask;
        fill_in6_addr(addr, epIp->to_v6());
        packets::get_subnet_mask_v6(prefixLen, &mask);
        match_set_nd_target_masked(&fe->entry->match, &addr, &mask);

        // OVS uses tp_dst for ICMPV6_CODE
        match_set_tp_dst(&fe->entry->match, 0);
    }
}

static void
setSecurityMatchRouterSolit(FlowEntry *fe, uint16_t prio) {
    fe->entry->table_id = IntFlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
    match_set_nw_proto(&fe->entry->match, 58);
    // OVS uses tp_src for ICMPV6_TYPE
    match_set_tp_src(&fe->entry->match, htons(ND_ROUTER_SOLICIT));
    // OVS uses tp_dst for ICMPV6_CODE
    match_set_tp_dst(&fe->entry->match, 0);
}

static void
setMatchDHCP(FlowEntry *fe, uint8_t tableId, uint16_t prio, bool v4,
             uint64_t cookie = 0) {
    fe->entry->table_id = tableId;
    fe->entry->priority = prio;
    if (cookie != 0)
        fe->entry->cookie = cookie;
    match_set_nw_proto(&fe->entry->match, 17);
    if (v4) {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
        match_set_tp_src(&fe->entry->match, htons(68));
        match_set_tp_dst(&fe->entry->match, htons(67));
    } else {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
        match_set_tp_src(&fe->entry->match, htons(546));
        match_set_tp_dst(&fe->entry->match, htons(547));
    }
}

static void
setSecurityActionAllow(FlowEntry *fe,
                       uint8_t nextTableId = IntFlowManager::SRC_TABLE_ID) {
    ActionBuilder ab;
    ab.SetGotoTable(nextTableId);
    ab.Build(fe->entry);
}

static void
setOutRevNatICMP(FlowEntry *fe, bool v4, uint8_t type) {
    fe->entry->table_id = IntFlowManager::OUT_TABLE_ID;
    fe->entry->priority = 10;
    fe->entry->cookie =
        v4 ? flow::cookie::ICMP_ERROR_V4 : flow::cookie::ICMP_ERROR_V6;
    match_set_metadata_masked(&fe->entry->match,
                              htonll(flow::meta::out::REV_NAT),
                              htonll(flow::meta::out::MASK));

    if (v4) {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
        match_set_nw_proto(&fe->entry->match, 1 /* ICMP */);
        match_set_tp_src(&fe->entry->match, htons(type));
    } else {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
        match_set_nw_proto(&fe->entry->match, 58 /* ICMP */);
        match_set_tp_src(&fe->entry->match, htons(type));
    }
    setActionController(fe);
}

address IntFlowManager::getEPGTunnelDst(const URI& epgURI) {
    optional<string> epgMcastIp =
        agent.getPolicyManager().getMulticastIPForGroup(epgURI);
    if (epgMcastIp) {
        boost::system::error_code ec;
        address ip = address::from_string(epgMcastIp.get(), ec);
        if (ec || !ip.is_v4() || ! ip.is_multicast()) {
            LOG(WARNING) << "Ignoring invalid/unsupported group multicast "
                " IP: " << epgMcastIp.get();
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

void IntFlowManager::anycastServiceUpdated(const std::string& uuid) {
    if (stopping) return;

    taskQueue.dispatch(uuid,
                       bind(&IntFlowManager::handleAnycastServiceUpdate,
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
    pktInHandler.portStatusUpdate(portName, portNo, fromDesc);
}

void IntFlowManager::peerStatusUpdated(const std::string&, int,
                                       PeerStatus peerStatus) {
    if (stopping || isSyncing) return;
    if (peerStatus == PeerStatusListener::READY) {
        advertManager.scheduleInitialEndpointAdv();
    }
}

bool
IntFlowManager::getGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
                                       optional<URI>& rdURI, uint32_t& rdId,
                                       optional<URI>& bdURI, uint32_t& bdId,
                                       optional<URI>& fdURI, uint32_t& fdId) {
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

static void proxyDiscovery(IntFlowManager& flowMgr, FlowEntryList& elBridgeDst,
                           const address& ipAddr,
                           const uint8_t* macAddr,
                           uint32_t epgVnid, uint32_t rdId,
                           uint32_t bdId) {
    if (ipAddr.is_v4()) {
        uint32_t tunPort = flowMgr.getTunnelPort();
        if (tunPort != OFPP_NONE &&
            flowMgr.getEncapType() != IntFlowManager::ENCAP_NONE) {
            FlowEntry* proxyArpTun = new FlowEntry();
            setDestMatchArp(proxyArpTun, 21, ipAddr,
                            bdId, rdId);
            match_set_in_port(&proxyArpTun->entry->match, tunPort);
            setDestActionArpReply(proxyArpTun, macAddr, ipAddr,
                                  OFPP_IN_PORT, flowMgr.getEncapType(),
                                  epgVnid);
            elBridgeDst.push_back(FlowEntryPtr(proxyArpTun));
        }
        {
            FlowEntry* proxyArp = new FlowEntry();
            setDestMatchArp(proxyArp, 20, ipAddr, bdId, rdId);
            setDestActionArpReply(proxyArp, macAddr, ipAddr);
            elBridgeDst.push_back(FlowEntryPtr(proxyArp));
        }
    } else {
        FlowEntry* proxyND = new FlowEntry();
        // pass MAC address in flow metadata
        uint64_t metadata = 0;
        memcpy(&metadata, macAddr, 6);
        ((uint8_t*)&metadata)[7] = 1;
        setDestMatchNd(proxyND, 20, &ipAddr, bdId, rdId,
                       flow::cookie::NEIGH_DISC);
        setActionController(proxyND, epgVnid, 0xffff, metadata);
        elBridgeDst.push_back(FlowEntryPtr(proxyND));
    }
}

static void computeIpmFlows(IntFlowManager& flowMgr,
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
            FlowEntry* ipmRoute = new FlowEntry();
            setDestMatchEp(ipmRoute, 452, NULL, floatingIp, frdId);
            match_set_reg(&ipmRoute->entry->match, 0 /* REG0 */, fepgVnid);
            setActionRevDNatDest(ipmRoute, epgVnid, bdId,
                                 fgrpId, rdId, ofPort,
                                 flowMgr.getRouterMacAddr(),
                                 macAddr, mappedIp);

            elRouteDst.push_back(FlowEntryPtr(ipmRoute));
        }
        {
            // Floating IP destination across EPGs
            // Apply policy for source EPG -> floating IP EPG
            // then resubmit with source EPG set to floating
            // IP EPG

            FlowEntry* ipmResub = new FlowEntry();
            setDestMatchEp(ipmResub, 450, NULL, floatingIp, frdId);
            ActionBuilder ab;
            ab.SetRegLoad(MFF_REG2, fepgVnid);
            ab.SetRegLoad(MFF_REG7, fepgVnid);
            ab.SetWriteMetadata(flow::meta::out::RESUBMIT_DST,
                                flow::meta::out::MASK);
            ab.SetGotoTable(IntFlowManager::POL_TABLE_ID);
            ab.Build(ipmResub->entry);

            elRouteDst.push_back(FlowEntryPtr(ipmResub));
        }
        // Reply to discovery requests for the floating IP
        // address
        proxyDiscovery(flowMgr, elBridgeDst, floatingIp, macAddr,
                       fepgVnid, frdId, fbdId);
    }
    {
        // Apply NAT action in output table
        FlowEntry* ipmNatOut = new FlowEntry();
        ipmNatOut->entry->table_id = IntFlowManager::OUT_TABLE_ID;
        ipmNatOut->entry->priority = 10;
        match_set_metadata_masked(&ipmNatOut->entry->match,
                                  htonll(flow::meta::out::NAT),
                                  htonll(flow::meta::out::MASK));
        match_set_reg(&ipmNatOut->entry->match, 6 /* REG6 */, rdId);
        match_set_reg(&ipmNatOut->entry->match, 7 /* REG7 */, fepgVnid);
        addMatchIp(ipmNatOut, mappedIp, true);

        ActionBuilder ab;
        ab.SetEthSrcDst(macAddr, effNextHopMac);
        if (!floatingIp.is_unspecified()) {
            ab.SetIpSrc(floatingIp);
        }
        ab.SetDecNwTtl();

        if (nextHopPort == OFPP_NONE) {
            ab.SetRegLoad(MFF_REG0, fepgVnid);
            ab.SetRegLoad(MFF_REG4, fbdId);
            ab.SetRegLoad(MFF_REG5, ffdId);
            ab.SetRegLoad(MFF_REG6, frdId);
            ab.SetRegLoad(MFF_REG7, (uint32_t)0);
            ab.SetRegLoad64(MFF_METADATA, 0);
            ab.SetResubmit(OFPP_IN_PORT, IntFlowManager::BRIDGE_TABLE_ID);
        } else {
            ab.SetRegLoad(MFF_PKT_MARK, rdId);
            ab.SetOutputToPort(nextHopPort);
        }
        ab.Build(ipmNatOut->entry);

        elOutput.push_back(FlowEntryPtr(ipmNatOut));
    }

    // Handle traffic returning from a next hop interface
    if (nextHopPort != OFPP_NONE) {
        if (!floatingIp.is_unspecified()) {
            // reverse traffic from next hop interface where we
            // delivered with a DNAT to a floating IP.  We assume that
            // the destination IP is unique for a given next hop
            // interface.
            FlowEntry* ipmNextHopRev = new FlowEntry();
            setSourceMatchEp(ipmNextHopRev, 201,
                             nextHopPort, effNextHopMac);
            addMatchIp(ipmNextHopRev, floatingIp);
            setActionRevDNatDest(ipmNextHopRev, epgVnid, bdId,
                                 fgrpId, rdId, ofPort,
                                 flowMgr.getRouterMacAddr(),
                                 macAddr, mappedIp);
            elSrc.push_back(FlowEntryPtr(ipmNextHopRev));
        }
        {
            // Reverse traffic from next hop interface where we
            // delivered with an SKB mark.  The SKB mark must be set
            // to the routing domain for the mapped destination IP
            // address
            FlowEntry* ipmNextHopRevMark = new FlowEntry();
            setSourceMatchEp(ipmNextHopRevMark, 200,
                             nextHopPort, effNextHopMac);
            match_set_pkt_mark(&ipmNextHopRevMark->entry->match, rdId);
            addMatchIp(ipmNextHopRevMark, mappedIp);
            setActionRevNatDest(ipmNextHopRevMark, epgVnid, bdId,
                                fgrpId, rdId, ofPort,
                                nextHopMac ? flowMgr.getRouterMacAddr() : NULL);
            elSrc.push_back(FlowEntryPtr(ipmNextHopRevMark));
        }
    }
}

static void virtualDHCP(FlowEntryList& elSrc, uint32_t ofPort,
                        uint32_t epgVnid, uint8_t* macAddr, bool v4) {
    FlowEntry* dhcp = new FlowEntry();
    setSourceMatchEp(dhcp, 150, ofPort, macAddr);
    setMatchDHCP(dhcp, IntFlowManager::SRC_TABLE_ID, 150, v4,
                 v4 ? flow::cookie::DHCP_V4 : flow::cookie::DHCP_V6);
    setActionController(dhcp, epgVnid, 0xffff);

    elSrc.push_back(FlowEntryPtr(dhcp));
}

void
IntFlowManager::handleEndpointUpdate(const string& uuid) {

    LOG(DEBUG) << "Updating endpoint " << uuid;

    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        switchManager.writeFlow(uuid, SEC_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, SRC_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, ROUTE_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, LEARN_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, SERVICE_MAP_DST_TABLE_ID, NULL);
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
    BOOST_FOREACH(const string& ipStr, endPoint.getIPs()) {
        address addr = address::from_string(ipStr, ec);
        if (ec) {
            LOG(WARNING) << "Invalid endpoint IP: "
                         << ipStr << ": " << ec.message();
        } else {
            ipAddresses.push_back(addr);
        }
    }
    if (hasMac) {
        address_v6 linkLocalIp(packets::construct_link_local_ip_addr(macAddr));
        if (endPoint.getIPs().find(linkLocalIp.to_string()) ==
            endPoint.getIPs().end())
            ipAddresses.push_back(linkLocalIp);
    }

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = endPoint.getInterfaceName();
    if (ofPortName) {
        ofPort = switchManager.getPortMapper().FindPort(ofPortName.get());
    }

    /* Port security flows */
    FlowEntryList el;
    if (ofPort != OFPP_NONE) {
        if (endPoint.isPromiscuousMode()) {
            FlowEntry *e0 = new FlowEntry();
            setSecurityMatchEpMac(e0, 50, ofPort, NULL);
            setSecurityActionAllow(e0);
            el.push_back(FlowEntryPtr(e0));
        } else if (hasMac) {
            FlowEntry *e0 = new FlowEntry();
            setSecurityMatchEpMac(e0, 20, ofPort, macAddr);
            setSecurityActionAllow(e0);
            el.push_back(FlowEntryPtr(e0));

            BOOST_FOREACH(const address& ipAddr, ipAddresses) {
                FlowEntry *e1 = new FlowEntry();
                setSecurityMatchEp(e1, 30, ofPort, macAddr, ipAddr);
                setSecurityActionAllow(e1);
                el.push_back(FlowEntryPtr(e1));

                if (ipAddr.is_v4()) {
                    FlowEntry *e2 = new FlowEntry();
                    setSecurityMatchEpArp(e2, 40, ofPort, macAddr, &ipAddr);
                    setSecurityActionAllow(e2);
                    el.push_back(FlowEntryPtr(e2));
                } else {
                    FlowEntry *e2 = new FlowEntry();
                    setSecurityMatchEpND(e2, 40, ofPort, macAddr,
                                         &ipAddr, false);
                    setSecurityActionAllow(e2);
                    el.push_back(FlowEntryPtr(e2));
                }
            }
        }

        BOOST_FOREACH(const Endpoint::virt_ip_t& vip,
                      endPoint.getVirtualIPs()) {
            packets::cidr_t vip_cidr;
            if (!packets::cidr_from_string(vip.second, vip_cidr)) {
                LOG(WARNING) << "Invalid endpoint VIP (CIDR): " << vip.second;
                continue;
            }
            uint8_t vmac[6];
            vip.first.toUIntArray(vmac);

            // Handle ARP/ND from "active" virtual IPs normally, that is
            // without generating a packet-in
            BOOST_FOREACH(const address& ipAddr, ipAddresses) {
                if (!packets::cidr_contains(vip_cidr, ipAddr)) {
                    continue;
                }
                FlowEntry *active_vip = new FlowEntry();
                if (ipAddr.is_v4()) {
                    setSecurityMatchEpArp(active_vip, 61, ofPort, vmac,
                                          &ipAddr);
                } else {
                    setSecurityMatchEpND(active_vip, 61, ofPort, vmac,
                                         &ipAddr, false);
                }
                setSecurityActionAllow(active_vip);
                el.push_back(FlowEntryPtr(active_vip));
            }

            FlowEntry *vf = new FlowEntry();
            if (vip_cidr.first.is_v4()) {
                setSecurityMatchEpArp(vf, 60, ofPort, vmac, &vip_cidr.first,
                                      vip_cidr.second);
                vf->entry->cookie = flow::cookie::VIRTUAL_IP_V4;
            } else {
                setSecurityMatchEpND(vf, 60, ofPort, vmac, &vip_cidr.first,
                                     false, vip_cidr.second);
                vf->entry->cookie = flow::cookie::VIRTUAL_IP_V6;
            }
            ActionBuilder ab;
            ab.SetController(0xffff);
            ab.SetGotoTable(IntFlowManager::SRC_TABLE_ID);
            ab.Build(vf->entry);
            el.push_back(FlowEntryPtr(vf));
        }
    }
    switchManager.writeFlow(uuid, SEC_TABLE_ID, el);

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!getGroupForwardingInfo(epgURI.get(), epgVnid, rdURI,
                                rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    optional<shared_ptr<FloodDomain> > fd =
        agent.getPolicyManager().getFDForGroup(epgURI.get());

    uint8_t arpMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t ndMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
    uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
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

    FlowEntryList elSrc;
    FlowEntryList elEpLearn;
    FlowEntryList elBridgeDst;
    FlowEntryList elRouteDst;
    FlowEntryList elServiceMap;
    FlowEntryList elOutput;

    /* Source Table flows; applicable only to local endpoints */
    if (ofPort != OFPP_NONE) {
        if (hasMac) {
            FlowEntry *e0 = new FlowEntry();
            setSourceMatchEp(e0, 140, ofPort, macAddr);
            setSourceAction(e0, epgVnid, bdId, fgrpId, rdId);
            elSrc.push_back(FlowEntryPtr(e0));

            if (bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL &&
                unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
                // Prepopulate a stage1 learning entry for known EPs
                FlowEntry* learnEntry = new FlowEntry();
                setMatchFd(learnEntry, 101, fgrpId, false, LEARN_TABLE_ID,
                           macAddr, flow::cookie::PROACTIVE_LEARN);
                ActionBuilder ab;
                ab.SetRegLoad(MFF_REG7, ofPort);
                ab.SetOutputToPort(ofPort);
                ab.SetController();
                ab.Build(learnEntry->entry);
                elEpLearn.push_back(FlowEntryPtr(learnEntry));
            }
        }

        if (endPoint.isPromiscuousMode()) {
            // if the source is unknown, but the interface is
            // promiscuous we allow the traffic into the learning
            // table
            FlowEntry *e1 = new FlowEntry();
            setSourceMatchEp(e1, 138, ofPort, NULL);
            setSourceAction(e1, epgVnid, bdId, fgrpId, rdId, LEARN_TABLE_ID);
            elSrc.push_back(FlowEntryPtr(e1));

            // Multicast traffic from promiscuous ports is delivered
            // normally
            FlowEntry *e2 = new FlowEntry();
            setSourceMatchEp(e2, 139, ofPort, NULL);
            match_set_dl_dst_masked(&e2->entry->match,
                                    packets::MAC_ADDR_BROADCAST,
                                    packets::MAC_ADDR_MULTICAST);
            setSourceAction(e2, epgVnid, bdId, fgrpId, rdId);
            elSrc.push_back(FlowEntryPtr(e2));
        }

        if (virtualDHCPEnabled && hasMac) {
            optional<Endpoint::DHCPv4Config> v4c = endPoint.getDHCPv4Config();
            optional<Endpoint::DHCPv6Config> v6c = endPoint.getDHCPv6Config();

            if (v4c)
                virtualDHCP(elSrc, ofPort, epgVnid, macAddr, true);
            if (v6c)
                virtualDHCP(elSrc, ofPort, epgVnid, macAddr, false);

            BOOST_FOREACH(const Endpoint::virt_ip_t& vip,
                          endPoint.getVirtualIPs()) {
                if (endPoint.getMAC().get() == vip.first) continue;
                packets::cidr_t vip_cidr;
                if (!packets::cidr_from_string(vip.second, vip_cidr)) {
                    continue;
                }
                address& addr = vip_cidr.first;
                if (ec) continue;
                uint8_t vmacAddr[6];
                vip.first.toUIntArray(vmacAddr);

                if (v4c && addr.is_v4())
                    virtualDHCP(elSrc, ofPort, epgVnid, vmacAddr, true);
                else if (v6c && addr.is_v6())
                    virtualDHCP(elSrc, ofPort, epgVnid, vmacAddr, false);
            }
        }
    }

    /* Bridge, route, and output flows */
    if (bdId != 0 && hasMac && ofPort != OFPP_NONE) {
        FlowEntry *e0 = new FlowEntry();
        setDestMatchEpMac(e0, 10, macAddr, bdId);
        setDestActionEpMac(e0, epgVnid, ofPort);
        elBridgeDst.push_back(FlowEntryPtr(e0));
    }

    if (rdId != 0 && bdId != 0 && ofPort != OFPP_NONE) {
        uint8_t routingMode =
            agent.getPolicyManager().getEffectiveRoutingMode(epgURI.get());

        if (virtualRouterEnabled && hasMac &&
            routingMode == RoutingModeEnumT::CONST_ENABLED) {
            BOOST_FOREACH (const address& ipAddr, ipAddresses) {
                if (endPoint.isDiscoveryProxyMode()) {
                    // Auto-reply to ARP and NDP requests for endpoint
                    // IP addresses
                    proxyDiscovery(*this, elBridgeDst, ipAddr,
                                   macAddr, epgVnid, rdId, bdId);
                } else {
                    if (arpMode != AddressResModeEnumT::CONST_FLOOD &&
                        ipAddr.is_v4()) {
                        FlowEntry *e1 = new FlowEntry();
                        setDestMatchArp(e1, 20, ipAddr, bdId, rdId);
                        if (arpMode == AddressResModeEnumT::CONST_UNICAST) {
                            // ARP optimization: broadcast -> unicast
                            setDestActionEpArp(e1, epgVnid, ofPort, macAddr);
                        }
                        // else drop the ARP packet
                        elBridgeDst.push_back(FlowEntryPtr(e1));
                    }

                    if (ndMode != AddressResModeEnumT::CONST_FLOOD &&
                        ipAddr.is_v6()) {
                        FlowEntry *e1 = new FlowEntry();
                        setDestMatchNd(e1, 20, &ipAddr, bdId, rdId);
                        if (ndMode == AddressResModeEnumT::CONST_UNICAST) {
                            // neighbor discovery optimization:
                            // broadcast -> unicast
                            setDestActionEpArp(e1, epgVnid, ofPort, macAddr);
                        }
                        // else drop the ND packet
                        elBridgeDst.push_back(FlowEntryPtr(e1));
                    }
                }

                if (packets::is_link_local(ipAddr))
                    continue;

                {
                    FlowEntry *e0 = new FlowEntry();
                    setDestMatchEp(e0, 500, getRouterMacAddr(), ipAddr, rdId);
                    setDestActionEp(e0, epgVnid, ofPort, getRouterMacAddr(),
                                    macAddr);
                    elRouteDst.push_back(FlowEntryPtr(e0));
                }

            }

            // IP address mappings
            BOOST_FOREACH (const Endpoint::IPAddressMapping& ipm,
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
                if (!getGroupForwardingInfo(ipm.getEgURI().get(), fepgVnid,
                                            frdURI, frdId, fbdURI, fbdId,
                                            ffdURI, ffdId))
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

                computeIpmFlows(*this, elSrc, elBridgeDst, elRouteDst,
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
            BOOST_FOREACH(const string& ipStr,
                          endPoint.getAnycastReturnIPs()) {
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

            BOOST_FOREACH (const address& ipAddr, anycastReturnIps) {
                {
                    FlowEntry *serviceDest = new FlowEntry();
                    setDestMatchEp(serviceDest, 50, NULL, ipAddr, rdId);
                    serviceDest->entry->table_id =
                        IntFlowManager::SERVICE_MAP_DST_TABLE_ID;
                    ActionBuilder ab;
                    ab.SetEthSrcDst(getRouterMacAddr(), macAddr);
                    ab.SetDecNwTtl();
                    ab.SetOutputToPort(ofPort);
                    ab.Build(serviceDest->entry);
                    elServiceMap.push_back(FlowEntryPtr(serviceDest));
                }
                if (ipAddr.is_v4()) {
                    FlowEntry *proxyArp = new FlowEntry();
                    setDestMatchArp(proxyArp, 51, ipAddr, 0, rdId);
                    proxyArp->entry->table_id =
                        IntFlowManager::SERVICE_MAP_DST_TABLE_ID;
                    setDestActionArpReply(proxyArp, macAddr, ipAddr);
                    elServiceMap.push_back(FlowEntryPtr(proxyArp));
                } else {
                    FlowEntry* proxyND = new FlowEntry();
                    // pass MAC address in flow metadata
                    uint64_t metadata = 0;
                    memcpy(&metadata, macAddr, 6);
                    ((uint8_t*)&metadata)[7] = 1;
                    setDestMatchNd(proxyND, 51, &ipAddr, 0, rdId,
                                   flow::cookie::NEIGH_DISC);
                    proxyND->entry->table_id =
                        IntFlowManager::SERVICE_MAP_DST_TABLE_ID;
                    setActionController(proxyND, 0, 0xffff, metadata);
                    elServiceMap.push_back(FlowEntryPtr(proxyND));
                }
            }
        }
    }

    switchManager.writeFlow(uuid, SRC_TABLE_ID, elSrc);
    switchManager.writeFlow(uuid, LEARN_TABLE_ID, elEpLearn);
    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, elBridgeDst);
    switchManager.writeFlow(uuid, ROUTE_TABLE_ID, elRouteDst);
    switchManager.writeFlow(uuid, SERVICE_MAP_DST_TABLE_ID, elServiceMap);
    switchManager.writeFlow(uuid, OUT_TABLE_ID, elOutput);

    if (fgrpURI && ofPort != OFPP_NONE) {
        updateEndpointFloodGroup(fgrpURI.get(), endPoint, ofPort,
                                 endPoint.isPromiscuousMode(),
                                 fd);
    } else {
        removeEndpointFromFloodGroup(uuid);
    }
}

void
IntFlowManager::handleAnycastServiceUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating anycast service " << uuid;

    ServiceManager& srvMgr = agent.getServiceManager();
    shared_ptr<const AnycastService> asWrapper = srvMgr.getAnycastService(uuid);

    if (!asWrapper) {
        switchManager.writeFlow(uuid, SEC_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, NULL);
        switchManager.writeFlow(uuid, SERVICE_MAP_DST_TABLE_ID, NULL);
        return;
    }

    const AnycastService& as = *asWrapper;

    FlowEntryList secFlows;
    FlowEntryList bridgeFlows;
    FlowEntryList serviceMapDstFlows;

    boost::system::error_code ec;

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = as.getInterfaceName();
    if (ofPortName) {
        ofPort = switchManager.getPortMapper()
            .FindPort(as.getInterfaceName().get());
    }

    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(),
                               as.getDomainURI().get());

    if (ofPort != OFPP_NONE && as.getServiceMAC() && rd) {
        uint8_t macAddr[6];
        as.getServiceMAC().get().toUIntArray(macAddr);

        uint32_t rdId =
            getId(RoutingDomain::CLASS_ID, as.getDomainURI().get());

        BOOST_FOREACH(AnycastService::ServiceMapping sm,
                      as.getServiceMappings()) {
            if (!sm.getServiceIP())
                continue;

            address addr = address::from_string(sm.getServiceIP().get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid service IP: "
                             << sm.getServiceIP().get()
                             << ": " << ec.message();
                continue;
            }
            address nextHopAddr;
            bool hasNextHop = false;
            if (sm.getNextHopIP()) {
                nextHopAddr =
                    address::from_string(sm.getNextHopIP().get(), ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service next hop IP: "
                                 << sm.getNextHopIP().get()
                                 << ": " << ec.message();
                } else {
                    hasNextHop = true;
                }
            }

            // Traffic sent to anycast services is intercepted in the
            // bridge table, despite the fact that it is effectively
            // performing a routing action, to allow it to work with flood
            // domains configured to use mac learning.
            {
                FlowEntry *serviceDest = new FlowEntry();
                setDestMatchEp(serviceDest, 50, NULL, addr, rdId);
                serviceDest->entry->table_id = IntFlowManager::BRIDGE_TABLE_ID;
                ActionBuilder ab;
                ab.SetEthSrcDst(getRouterMacAddr(), macAddr);
                if (hasNextHop) {
                    // map traffic to service interface to the next
                    // hop IP using DNAT semantics
                    ab.SetIpDst(nextHopAddr);
                }

                ab.SetDecNwTtl();
                ab.SetOutputToPort(ofPort);
                ab.Build(serviceDest->entry);
                bridgeFlows.push_back(FlowEntryPtr(serviceDest));
            }

            // Traffic sent from anycast services is intercepted in
            // the security table to prevent normal processing
            // semantics.
            {
                FlowEntry *svcIP = new FlowEntry();
                svcIP->entry->table_id = SEC_TABLE_ID;
                svcIP->entry->priority = 100;
                match_set_in_port(&svcIP->entry->match, ofPort);
                match_set_dl_src(&svcIP->entry->match, macAddr);

                ActionBuilder ab;
                ab.SetRegLoad(MFF_REG6, rdId);

                if (hasNextHop) {
                    // If there is a next hop mapping, map the return
                    // traffic from service interface using DNAT
                    // semantics
                    addMatchIp(svcIP, nextHopAddr, true);
                    ab.SetIpSrc(addr);
                    ab.SetDecNwTtl();
                } else {
                    addMatchIp(svcIP, addr, true);
                }

                ab.SetGotoTable(SERVICE_MAP_DST_TABLE_ID);
                ab.Build(svcIP->entry);
                secFlows.push_back(FlowEntryPtr(svcIP));
            }
            if (addr.is_v4()) {
                FlowEntry *svcARP = new FlowEntry();
                setSecurityMatchEpArp(svcARP, 100, ofPort, macAddr,
                                      hasNextHop ? &nextHopAddr : &addr);

                ActionBuilder ab;
                ab.SetRegLoad(MFF_REG6, rdId);
                ab.SetGotoTable(SERVICE_MAP_DST_TABLE_ID);
                ab.Build(svcARP->entry);
                secFlows.push_back(FlowEntryPtr(svcARP));
            }

            if (addr.is_v4()) {
                FlowEntry *proxyArp = new FlowEntry();
                setDestMatchArp(proxyArp, 51, addr, 0, rdId);
                setDestActionArpReply(proxyArp, macAddr, addr);
                bridgeFlows.push_back(FlowEntryPtr(proxyArp));
            } else {
                FlowEntry* proxyND = new FlowEntry();
                // pass MAC address in flow metadata
                uint64_t metadata = 0;
                memcpy(&metadata, macAddr, 6);
                ((uint8_t*)&metadata)[7] = 1;
                setDestMatchNd(proxyND, 51, &addr, 0, rdId,
                               flow::cookie::NEIGH_DISC);
                setActionController(proxyND, 0, 0xffff, metadata);
                bridgeFlows.push_back(FlowEntryPtr(proxyND));
            }

            if (sm.getGatewayIP()) {
                address gwAddr =
                    address::from_string(sm.getGatewayIP().get(), ec);
                if (ec) {
                    LOG(WARNING) << "Invalid service gateway IP: "
                                 << sm.getGatewayIP().get()
                                 << ": " << ec.message();
                } else {
                    if (gwAddr.is_v4()) {
                        FlowEntry *gwArp = new FlowEntry();
                        setDestMatchArp(gwArp, 31, gwAddr, 0, rdId);
                        match_set_dl_src(&gwArp->entry->match, macAddr);
                        gwArp->entry->table_id =
                            IntFlowManager::SERVICE_MAP_DST_TABLE_ID;
                        setDestActionArpReply(gwArp, getRouterMacAddr(),
                                              gwAddr);
                        serviceMapDstFlows.push_back(FlowEntryPtr(gwArp));
                    } else {
                        FlowEntry* gwND = new FlowEntry();
                        // pass MAC address in flow metadata
                        uint64_t metadata = 0;
                        memcpy(&metadata, macAddr, 6);
                        ((uint8_t*)&metadata)[7] = 1;
                        ((uint8_t*)&metadata)[6] = 1;
                        setDestMatchNd(gwND, 31, &gwAddr, 0, rdId,
                                       flow::cookie::NEIGH_DISC);
                        match_set_dl_src(&gwND->entry->match, macAddr);
                        gwND->entry->table_id =
                            IntFlowManager::SERVICE_MAP_DST_TABLE_ID;
                        setActionController(gwND, 0, 0xffff, metadata);
                        serviceMapDstFlows.push_back(FlowEntryPtr(gwND));
                    }
                }
            }
        }
    }

    switchManager.writeFlow(uuid, SEC_TABLE_ID, secFlows);
    switchManager.writeFlow(uuid, BRIDGE_TABLE_ID, bridgeFlows);
    switchManager.writeFlow(uuid, SERVICE_MAP_DST_TABLE_ID, serviceMapDstFlows);
}

void
IntFlowManager::updateEPGFlood(const URI& epgURI, uint32_t epgVnid,
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
        FlowEntry *mcast = new FlowEntry();
        setMatchFd(mcast, 10, fgrpId, true, BRIDGE_TABLE_ID);
        match_set_reg(&mcast->entry->match, 0 /* REG0 */, epgVnid);
        if (bcastFloodMode == BcastFloodModeEnumT::CONST_ISOLATED) {
            // In isolated mode deliver only if policy has already
            // been applied (i.e. it comes from the tunnel uplink)
            match_set_metadata_masked(&mcast->entry->match,
                                      htonll(flow::meta::POLICY_APPLIED),
                                      htonll(flow::meta::POLICY_APPLIED));
        }
        setActionEPGFdBroadcast(mcast, getEncapType(), epgTunDst);
        grpDst.push_back(FlowEntryPtr(mcast));
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
            FlowEntry *dropARP = new FlowEntry();
            setSecurityMatchEpArp(dropARP, 25, OFPP_ANY, NULL, 0);
            portSec.push_back(FlowEntryPtr(dropARP));

            FlowEntry *dropIPv4 = new FlowEntry();
            setSecurityMatchIP(dropIPv4, 25, true);
            portSec.push_back(FlowEntryPtr(dropIPv4));

            FlowEntry *dropIPv6 = new FlowEntry();
            setSecurityMatchIP(dropIPv6, 25, false);
            portSec.push_back(FlowEntryPtr(dropIPv6));
        }
        {
            // Allow DHCP requests but not replies
            FlowEntry *allowDHCP = new FlowEntry();
            setMatchDHCP(allowDHCP, IntFlowManager::SEC_TABLE_ID, 27, true);
            setSecurityActionAllow(allowDHCP);
            portSec.push_back(FlowEntryPtr(allowDHCP));
        }
        {
            // Allow IPv6 autoconfiguration (DHCP & router solicitiation)
            // requests but not replies
            FlowEntry *allowDHCPv6 = new FlowEntry();
            setMatchDHCP(allowDHCPv6, IntFlowManager::SEC_TABLE_ID, 27, false);
            setSecurityActionAllow(allowDHCPv6);
            portSec.push_back(FlowEntryPtr(allowDHCPv6));

            FlowEntry *allowRouterSolit = new FlowEntry();
            setSecurityMatchRouterSolit(allowRouterSolit, 27);
            setSecurityActionAllow(allowRouterSolit);
            portSec.push_back(FlowEntryPtr(allowRouterSolit));
        }
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // allow all traffic from the tunnel uplink through the port
            // security table
            FlowEntry *allowTunnel = new FlowEntry();
            setSecurityMatchEpMac(allowTunnel, 50, tunPort, NULL);
            setSecurityActionAllow(allowTunnel);
            portSec.push_back(FlowEntryPtr(allowTunnel));
        }
        switchManager.writeFlow("static", SEC_TABLE_ID, portSec);
    }
    {
        // Block flows from the uplink when not allowed by
        // higher-priority per-EPG rules to allow them.
        FlowEntry *policyApplied = new FlowEntry();
        policyApplied->entry->table_id = POL_TABLE_ID;
        policyApplied->entry->priority =
            flowutils::MAX_POLICY_RULE_PRIORITY + 50;
        match_set_metadata_masked(&policyApplied->entry->match,
                                  htonll(flow::meta::POLICY_APPLIED),
                                  htonll(flow::meta::POLICY_APPLIED));
        switchManager.writeFlow("static", POL_TABLE_ID, policyApplied);
    }
    {
        FlowEntry *unknownTunnelBr = NULL;
        FlowEntry *unknownTunnelRt = NULL;
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // Output to the tunnel interface, bypassing policy
            // note that if the flood domain is set to flood unknown, then
            // there will be a higher-priority rule installed for that
            // flood domain.
            unknownTunnelBr = new FlowEntry();
            unknownTunnelBr->entry->priority = 1;
            unknownTunnelBr->entry->table_id = BRIDGE_TABLE_ID;
            setDestActionOutputToEPGTunnel(unknownTunnelBr);

            if (virtualRouterEnabled) {
                unknownTunnelRt = new FlowEntry();
                unknownTunnelRt->entry->priority = 1;
                unknownTunnelRt->entry->table_id = ROUTE_TABLE_ID;
                setDestActionOutputToEPGTunnel(unknownTunnelRt);
            }
        }
        switchManager.writeFlow("static", BRIDGE_TABLE_ID, unknownTunnelBr);
        switchManager.writeFlow("static", ROUTE_TABLE_ID, unknownTunnelRt);
    }
    {
        FlowEntryList outFlows;
        outFlows.push_back(flowutils::default_out_flow(OUT_TABLE_ID));
        {
            FlowEntry* revNatOutputReg = new FlowEntry();
            revNatOutputReg->entry->table_id = OUT_TABLE_ID;
            revNatOutputReg->entry->priority = 1;
            match_set_metadata_masked(&revNatOutputReg->entry->match,
                                      htonll(flow::meta::out::REV_NAT),
                                      htonll(flow::meta::out::MASK));
            ActionBuilder ab;
            ab.SetOutputReg(MFF_REG7);
            ab.Build(revNatOutputReg->entry);

            outFlows.push_back(FlowEntryPtr(revNatOutputReg));
        }
        {
            // send reverse NAT ICMP error packets to controller
            FlowEntry* revNatICMPUnreach = new FlowEntry();
            setOutRevNatICMP(revNatICMPUnreach, true, 3);
            outFlows.push_back(FlowEntryPtr(revNatICMPUnreach));

            FlowEntry* revNatICMPTimeExc = new FlowEntry();
            setOutRevNatICMP(revNatICMPTimeExc, true, 11);
            outFlows.push_back(FlowEntryPtr(revNatICMPTimeExc));

            FlowEntry* revNatICMPParam = new FlowEntry();
            setOutRevNatICMP(revNatICMPParam, true, 12);
            outFlows.push_back(FlowEntryPtr(revNatICMPParam));
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
        switchManager.writeFlow(epgId, SRC_TABLE_ID, NULL);
        switchManager.writeFlow(epgId, POL_TABLE_ID, NULL);
        switchManager.writeFlow(epgId, OUT_TABLE_ID, NULL);
        switchManager.writeFlow(epgId, BRIDGE_TABLE_ID, NULL);
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
        // table as normal.  Multicast traffic still goes to the
        // destination table, however.

        uint8_t unkFloodMode = UnknownFloodModeEnumT::CONST_DROP;
        uint8_t bcastFloodMode = BcastFloodModeEnumT::CONST_NORMAL;
        optional<shared_ptr<FloodDomain> > epgFd = polMgr.getFDForGroup(epgURI);
        if (epgFd) {
            unkFloodMode = epgFd.get()
                ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
            bcastFloodMode = epgFd.get()
                ->getBcastFloodMode(BcastFloodModeEnumT::CONST_NORMAL);
        }

        uint8_t nextTable = IntFlowManager::BRIDGE_TABLE_ID;
        if (unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD &&
            bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL)
            nextTable = IntFlowManager::LEARN_TABLE_ID;

        // Assign the source registers based on the VNID from the
        // tunnel uplink
        FlowEntry *e0 = new FlowEntry();
        setSourceMatchEpg(e0, encapType, 149, tunPort, epgVnid);
        setSourceAction(e0, epgVnid, bdId, fgrpId, rdId,
                        nextTable, encapType, true);
        uplinkMatch.push_back(FlowEntryPtr(e0));

        if (unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD &&
            bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL) {
            FlowEntry *e1 = new FlowEntry();
            setSourceMatchEpg(e1, encapType, 150, tunPort, epgVnid);
            match_set_dl_dst_masked(&e1->entry->match,
                                    packets::MAC_ADDR_BROADCAST,
                                    packets::MAC_ADDR_MULTICAST);
            setSourceAction(e1, epgVnid, bdId, fgrpId, rdId,
                            IntFlowManager::BRIDGE_TABLE_ID, encapType, true);
            uplinkMatch.push_back(FlowEntryPtr(e1));
        }
    }
    switchManager.writeFlow(epgId, SRC_TABLE_ID, uplinkMatch);

    {
        uint8_t intraGroup = IntraGroupPolicyEnumT::CONST_ALLOW;
        optional<shared_ptr<EpGroup> > epg =
            EpGroup::resolve(agent.getFramework(), epgURI);
        if (epg && epg.get()->isIntraGroupPolicySet()) {
            intraGroup = epg.get()->getIntraGroupPolicy().get();
        }

        FlowEntryPtr intraGroupFlow(new FlowEntry());
        uint16_t prio = flowutils::MAX_POLICY_RULE_PRIORITY + 100;
        switch (intraGroup) {
        case IntraGroupPolicyEnumT::CONST_DENY:
            prio = flowutils::MAX_POLICY_RULE_PRIORITY + 200;
            break;
        case IntraGroupPolicyEnumT::CONST_REQUIRE_CONTRACT:
            // Only automatically allow intra-EPG traffic if its come
            // from the uplink and therefore already had policy
            // applied.
            match_set_metadata_masked(&intraGroupFlow->entry->match,
                                      htonll(flow::meta::POLICY_APPLIED),
                                      htonll(flow::meta::POLICY_APPLIED));
            /* fall through */
        case IntraGroupPolicyEnumT::CONST_ALLOW:
        default:
            {
                ActionBuilder ab;
                ab.SetGotoTable(IntFlowManager::OUT_TABLE_ID);
                ab.Build(intraGroupFlow->entry);
            }
            break;
        }
        flowutils::set_match_group(*intraGroupFlow, POL_TABLE_ID,
                                   prio, epgVnid, epgVnid);
        switchManager.writeFlow(epgId, POL_TABLE_ID, intraGroupFlow);
    }

    if (virtualRouterEnabled && rdId != 0 && bdId != 0) {
        updateGroupSubnets(epgURI, bdId, rdId);

        FlowEntryList bridgel;
        uint8_t routingMode =
            agent.getPolicyManager().getEffectiveRoutingMode(epgURI);

        if (routingMode == RoutingModeEnumT::CONST_ENABLED) {
            FlowEntry *br = new FlowEntry();
            br->entry->priority = 2;
            br->entry->table_id = BRIDGE_TABLE_ID;
            match_set_reg(&br->entry->match, 4 /* REG4 */, bdId);
            ActionBuilder ab;
            ab.SetGotoTable(ROUTE_TABLE_ID);
            ab.Build(br->entry);
            bridgel.push_back(FlowEntryPtr(br));

            if (routerAdv) {
                FlowEntry *r = new FlowEntry();
                setDestMatchNd(r, 20, NULL, bdId, rdId,
                               flow::cookie::NEIGH_DISC,
                               ND_ROUTER_SOLICIT);
                setActionController(r);
                bridgel.push_back(FlowEntryPtr(r));

                if (!isSyncing)
                    advertManager.scheduleInitialRouterAdv();
            }
        }
        switchManager.writeFlow(bdURI.get().toString(), BRIDGE_TABLE_ID, bridgel);
    }

    updateEPGFlood(epgURI, epgVnid, fgrpId, epgTunDst);

    FlowEntryList egOutFlows;
    {
        // Output table action to resubmit the flow to the bridge
        // table with source registers set to the current EPG
        FlowEntry* resubmitDst = new FlowEntry();
        resubmitDst->entry->table_id = OUT_TABLE_ID;
        resubmitDst->entry->priority = 10;
        match_set_reg(&resubmitDst->entry->match, 7 /* REG7 */, epgVnid);
        match_set_metadata_masked(&resubmitDst->entry->match,
                                  htonll(flow::meta::out::RESUBMIT_DST),
                                  htonll(flow::meta::out::MASK));
        ActionBuilder ab;
        ab.SetRegLoad(MFF_REG0, epgVnid);
        ab.SetRegLoad(MFF_REG4, bdId);
        ab.SetRegLoad(MFF_REG5, fgrpId);
        ab.SetRegLoad(MFF_REG6, rdId);
        ab.SetRegLoad(MFF_REG7, (uint32_t)0);
        ab.SetRegLoad64(MFF_METADATA, 0);
        ab.SetResubmit(OFPP_IN_PORT, BRIDGE_TABLE_ID);
        ab.Build(resubmitDst->entry);

        egOutFlows.push_back(FlowEntryPtr(resubmitDst));
    }
    if (encapType != ENCAP_NONE && tunPort != OFPP_NONE) {
        {
            // Output table action to output to the tunnel appropriate for
            // the source EPG
            FlowEntry* tunnelOut = new FlowEntry();
            tunnelOut->entry->table_id = OUT_TABLE_ID;
            tunnelOut->entry->priority = 10;
            match_set_reg(&tunnelOut->entry->match, 0 /* REG0 */, epgVnid);
            match_set_metadata_masked(&tunnelOut->entry->match,
                                      htonll(flow::meta::out::TUNNEL),
                                      htonll(flow::meta::out::MASK));
            ActionBuilder ab;
            IntFlowManager::setActionTunnelMetadata(ab, encapType, epgTunDst);
            ab.SetOutputToPort(tunPort);
            ab.Build(tunnelOut->entry);

            egOutFlows.push_back(FlowEntryPtr(tunnelOut));
        }
        if (encapType != ENCAP_VLAN) {
            // If destination is the router mac, override EPG tunnel
            // and send to unicast tunnel
            FlowEntry* tunnelOutRtr = new FlowEntry();
            tunnelOutRtr->entry->table_id = OUT_TABLE_ID;
            tunnelOutRtr->entry->priority = 11;
            match_set_reg(&tunnelOutRtr->entry->match, 0 /* REG0 */, epgVnid);
            match_set_dl_dst(&tunnelOutRtr->entry->match,
                             getRouterMacAddr());
            match_set_metadata_masked(&tunnelOutRtr->entry->match,
                                      htonll(flow::meta::out::TUNNEL),
                                      htonll(flow::meta::out::MASK));
            ActionBuilder ab;
            IntFlowManager::setActionTunnelMetadata(ab, encapType,
                                                    getTunnelDst());
            ab.SetOutputToPort(tunPort);
            ab.Build(tunnelOutRtr->entry);

            egOutFlows.push_back(FlowEntryPtr(tunnelOutRtr));
        }
    }
    switchManager.writeFlow(epgId, OUT_TABLE_ID, egOutFlows);

    unordered_set<string> epUuids;
    EndpointManager& epMgr = agent.getEndpointManager();
    epMgr.getEndpointsForIPMGroup(epgURI, epUuids);
    boost::unordered_set<URI> ipmRds;
    BOOST_FOREACH(const string& uuid, epUuids) {
        boost::shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
        if (!ep) continue;
        const boost::optional<opflex::modb::URI>& egURI = ep->getEgURI();
        if (!egURI) continue;
        boost::optional<boost::shared_ptr<modelgbp::gbp::RoutingDomain> > rd =
            polMgr.getRDForGroup(egURI.get());
        if (rd)
            ipmRds.insert(rd.get()->getURI());
    }
    BOOST_FOREACH(const URI& rdURI, ipmRds) {
        // update routing domains that have references to the
        // IP-mapping EPG to ensure external subnets are correctly
        // mapped.
        rdConfigUpdated(rdURI);
    }

    // note this combines with the IPM group endpoints from above:
    epMgr.getEndpointsForGroup(epgURI, epUuids);
    BOOST_FOREACH(const string& uuid, epUuids) {
        advertManager.scheduleEndpointAdv(uuid);
        endpointUpdated(uuid);
    }

    PolicyManager::uri_set_t contractURIs;
    polMgr.getContractsForGroup(epgURI, contractURIs);
    BOOST_FOREACH(const URI& contract, contractURIs) {
        contractUpdated(contract);
    }

    optional<string> epgMcastIp = polMgr.getMulticastIPForGroup(epgURI);
    updateMulticastList(epgMcastIp, epgURI);
    optional<string> fdcMcastIp;
    optional<shared_ptr<FloodContext> > fdCtx =
        polMgr.getFloodContextForGroup(epgURI);
    if (fdCtx) {
        fdcMcastIp = fdCtx.get()->getMulticastGroupIP();
        updateMulticastList(fdcMcastIp, fdCtx.get()->getURI());
    }
}

void
IntFlowManager::updateGroupSubnets(const URI& egURI, uint32_t bdId,
                                   uint32_t rdId) {
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(egURI, subnets);

    uint32_t tunPort = getTunnelPort();

    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets) {
        FlowEntryList el;

        optional<const string&> networkAddrStr = sn->getAddress();
        bool hasRouterIp = false;
        boost::system::error_code ec;
        if (networkAddrStr) {
            address networkAddr, routerIp;
            networkAddr = address::from_string(networkAddrStr.get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid network address for subnet: "
                             << networkAddrStr << ": " << ec.message();
            } else {
                if (networkAddr.is_v4()) {
                    optional<const string&> routerIpStr =
                        sn->getVirtualRouterIp();
                    if (routerIpStr) {
                        routerIp = address::from_string(routerIpStr.get(), ec);
                        if (ec) {
                            LOG(WARNING) << "Invalid router IP: "
                                         << routerIpStr << ": " << ec.message();
                        } else {
                            hasRouterIp = true;
                        }
                    }
                } else {
                    // Construct router IP address
                    routerIp = packets::construct_link_local_ip_addr(routerMac);
                    hasRouterIp = true;
                }
            }
            // Reply to ARP requests for the router IP only from local
            // endpoints.
            if (hasRouterIp) {
                if (routerIp.is_v4()) {
                    if (tunPort != OFPP_NONE) {
                        FlowEntry *e0 = new FlowEntry();
                        setDestMatchArp(e0, 22, routerIp, bdId, rdId);
                        match_set_in_port(&e0->entry->match, tunPort);
                        el.push_back(FlowEntryPtr(e0));
                    }

                    FlowEntry *e1 = new FlowEntry();
                    setDestMatchArp(e1, 20, routerIp, bdId, rdId);
                    setDestActionArpReply(e1, getRouterMacAddr(), routerIp);
                    el.push_back(FlowEntryPtr(e1));
                } else {
                    if (tunPort != OFPP_NONE) {
                        FlowEntry *e0 = new FlowEntry();
                        setDestMatchNd(e0, 22, &routerIp, bdId, rdId,
                                       flow::cookie::NEIGH_DISC);
                        match_set_in_port(&e0->entry->match, tunPort);
                        el.push_back(FlowEntryPtr(e0));
                    }

                    FlowEntry *e1 = new FlowEntry();
                    setDestMatchNd(e1, 20, &routerIp, bdId, rdId,
                                   flow::cookie::NEIGH_DISC);
                    setActionController(e1);
                    el.push_back(FlowEntryPtr(e1));
                }
            }
        }
        switchManager.writeFlow(sn->getURI().toString(), BRIDGE_TABLE_ID, el);
    }
}

void
IntFlowManager::handleRoutingDomainUpdate(const URI& rdURI) {
    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(), rdURI);

    if (!rd) {
        LOG(DEBUG) << "Cleaning up for RD: " << rdURI;
        switchManager.writeFlow(rdURI.toString(), NAT_IN_TABLE_ID, NULL);
        switchManager.writeFlow(rdURI.toString(), ROUTE_TABLE_ID, NULL);
        idGen.erase(getIdNamespace(RoutingDomain::CLASS_ID), rdURI.toString());
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

    typedef std::pair<std::string, uint16_t> subnet_t;
    boost::unordered_set<subnet_t> intSubnets;

    vector<shared_ptr<RoutingDomainToIntSubnetsRSrc> > subnets_list;
    rd.get()->resolveGbpRoutingDomainToIntSubnetsRSrc(subnets_list);
    BOOST_FOREACH(shared_ptr<RoutingDomainToIntSubnetsRSrc>& subnets_ref,
                  subnets_list) {
        optional<URI> subnets_uri = subnets_ref->getTargetURI();
        if (!subnets_uri) continue;
        optional<shared_ptr<Subnets> > subnets_obj =
            Subnets::resolve(agent.getFramework(), subnets_uri.get());
        if (!subnets_obj) continue;

        vector<shared_ptr<Subnet> > subnets;
        subnets_obj.get()->resolveGbpSubnet(subnets);

        BOOST_FOREACH(shared_ptr<Subnet>& subnet, subnets) {
            if (!subnet->isAddressSet() || !subnet->isPrefixLenSet())
                continue;
            address addr = address::from_string(subnet->getAddress().get(),
                                                ec);
            if (ec) continue;
            addr = packets::mask_address(addr, subnet->getPrefixLen().get());
            intSubnets.insert(make_pair(addr.to_string(),
                                        subnet->getPrefixLen().get()));
        }
    }
    boost::shared_ptr<const RDConfig> rdConfig =
        agent.getExtraConfigManager().getRDConfig(rdURI);
    if (rdConfig) {
        BOOST_FOREACH(const std::string& cidrSn,
                      rdConfig->getInternalSubnets()) {
            packets::cidr_t cidr;
            if (packets::cidr_from_string(cidrSn, cidr)) {
                intSubnets.insert(make_pair(cidr.first.to_string(),
                                            cidr.second));
            } else {
                LOG(ERROR) << "Invalid CIDR subnet: " << cidrSn;
            }
        }
    }
    BOOST_FOREACH(const subnet_t& sn, intSubnets) {
        address addr = address::from_string(sn.first, ec);
        if (ec) continue;

        FlowEntry *snr = new FlowEntry();
        setMatchSubnet(snr, rdId, 300, ROUTE_TABLE_ID, addr,
                       sn.second, false);
        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            setDestActionOutputToEPGTunnel(snr);
        }
        rdRouteFlows.push_back(FlowEntryPtr(snr));
    }

    // If we miss the local endpoints and the internal subnets, check
    // each of the external layer 3 networks.  Match using
    // longest-prefix.
    vector<shared_ptr<L3ExternalDomain> > extDoms;
    rd.get()->resolveGbpL3ExternalDomain(extDoms);
    BOOST_FOREACH(shared_ptr<L3ExternalDomain>& extDom, extDoms) {
        vector<shared_ptr<L3ExternalNetwork> > extNets;
        extDom->resolveGbpL3ExternalNetwork(extNets);

        BOOST_FOREACH(shared_ptr<L3ExternalNetwork> net, extNets) {
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

            BOOST_FOREACH(shared_ptr<ExternalSubnet> extsub, extSubs) {
                if (!extsub->isAddressSet() || !extsub->isPrefixLenSet())
                    continue;
                address addr =
                    address::from_string(extsub->getAddress().get(), ec);
                if (ec) continue;

                {
                    FlowEntry *snr = new FlowEntry();
                    setMatchSubnet(snr, rdId, 150, ROUTE_TABLE_ID, addr,
                                   extsub->getPrefixLen(0), false);

                    if (natRef) {
                        // For external networks mapped to a NAT EPG,
                        // set the next hop action to NAT_OUT
                        ActionBuilder ab;
                        if (natEpgVnid) {
                            ab.SetRegLoad(MFF_REG2, netVnid);
                            ab.SetRegLoad(MFF_REG7, natEpgVnid.get());
                            ab.SetWriteMetadata(flow::meta::out::NAT,
                                                flow::meta::out::MASK);
                            ab.SetGotoTable(POL_TABLE_ID);
                            ab.Build(snr->entry);
                        }
                        // else drop until resolved
                    } else if (tunPort != OFPP_NONE &&
                               encapType != ENCAP_NONE) {
                        // For other external networks, output to the tunnel
                        setDestActionOutputToEPGTunnel(snr);
                    }
                    // else drop the packets
                    rdRouteFlows.push_back(FlowEntryPtr(snr));
                }
                {
                    FlowEntry *snn = new FlowEntry();
                    setMatchSubnet(snn, rdId, 150, NAT_IN_TABLE_ID, addr,
                                   extsub->getPrefixLen(0), true);
                    ActionBuilder ab;
                    ab.SetRegLoad(MFF_REG0, netVnid);
                    // We want to ensure that on the final delivery of
                    // the packet we perform protocol-specific reverse
                    // mapping.  This doesn't let us do hop-by-hop
                    // translations however.
                    //
                    // Also remove policy applied since we're changing
                    // the effective EPG and need to apply policy
                    // again.
                    ab.SetWriteMetadata(flow::meta::out::REV_NAT,
                                        flow::meta::out::MASK |
                                        flow::meta::POLICY_APPLIED);
                    ab.SetGotoTable(POL_TABLE_ID);
                    ab.Build(snn->entry);
                    rdNatFlows.push_back(FlowEntryPtr(snn));
                }
            }
        }
    }

    switchManager.writeFlow(rdURI.toString(), NAT_IN_TABLE_ID, rdNatFlows);
    switchManager.writeFlow(rdURI.toString(), ROUTE_TABLE_ID, rdRouteFlows);

    unordered_set<string> uuids;
    agent.getServiceManager().getAnycastServicesByDomain(rdURI, uuids);
    BOOST_FOREACH (const string& uuid, uuids) {
        anycastServiceUpdated(uuid);
    }
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
            switchManager.writeFlow(domURI.toString(), BRIDGE_TABLE_ID, NULL);
        }
        break;
    case BridgeDomain::CLASS_ID:
        if (!BridgeDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for BD: " << domURI;
            switchManager.writeFlow(domURI.toString(), BRIDGE_TABLE_ID, NULL);
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
    bkt->watch_group = OFPG11_ANY;
    return bkt;
}

GroupEdit::Entry
IntFlowManager::createGroupMod(uint16_t type, uint32_t groupId,
                            const Ep2PortMap& ep2port, bool onlyPromiscuous) {
    GroupEdit::Entry entry(new GroupEdit::GroupMod());
    entry->mod->command = type;
    entry->mod->group_id = groupId;

    BOOST_FOREACH(const Ep2PortMap::value_type& kv, ep2port) {
        if (onlyPromiscuous && !kv.second.second)
            continue;

        ofputil_bucket *bkt = createBucket(kv.second.first);
        ActionBuilder ab;
        ab.SetOutputToPort(kv.second.first);
        ab.Build(bkt);
        list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    uint32_t tunPort = getTunnelPort();
    if (type != OFPGC11_DELETE && tunPort != OFPP_NONE &&
        encapType != ENCAP_NONE) {
        ofputil_bucket *bkt = createBucket(tunPort);
        ActionBuilder ab;
        setActionTunnelMetadata(ab, encapType);
        ab.SetOutputToPort(tunPort);
        ab.Build(bkt);
        list_push_back(&entry->mod->buckets, &bkt->list_node);
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
        FlowEntry* floodOut = new FlowEntry();
        floodOut->entry->table_id = OUT_TABLE_ID;
        floodOut->entry->priority = 10;
        match_set_reg(&floodOut->entry->match, 5 /* REG5 */, fgrpId);
        match_set_metadata_masked(&floodOut->entry->match,
                                  htonll(flow::meta::out::FLOOD),
                                  htonll(flow::meta::out::MASK));
        ActionBuilder ab;
        ab.SetGroup(fgrpId);
        ab.Build(floodOut->entry);
        fdOutput.push_back(FlowEntryPtr(floodOut));
    }
    switchManager.writeFlow(fgrpStrId, OUT_TABLE_ID, fdOutput);

    FlowEntryList grpDst;
    FlowEntryList learnDst;
    if (bcastFloodMode == BcastFloodModeEnumT::CONST_NORMAL &&
        unkFloodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
        // go to the learning table on an unknown unicast
        // destination in flood mode
        FlowEntry *unicastLearn = new FlowEntry();
        setMatchFd(unicastLearn, 5, fgrpId, false, BRIDGE_TABLE_ID);
        setActionGotoLearn(unicastLearn);
        grpDst.push_back(FlowEntryPtr(unicastLearn));

        // Deliver unknown packets in the flood domain when
        // learning to the controller for reactive flow setup.
        FlowEntry *fdContr = new FlowEntry();
        setMatchFd(fdContr, 5, fgrpId, false, LEARN_TABLE_ID,
                   NULL, flow::cookie::PROACTIVE_LEARN);
        setActionController(fdContr);
        learnDst.push_back(FlowEntryPtr(fdContr));
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
            switchManager.writeFlow(fgrpStrId, OUT_TABLE_ID, NULL);
            switchManager.writeFlow(fgrpStrId, BRIDGE_TABLE_ID, NULL);
            switchManager.writeFlow(fgrpStrId, LEARN_TABLE_ID, NULL);
            floodGroupMap.erase(fgrpURI);
        }
        switchManager.writeGroupMod(e0);
        switchManager.writeGroupMod(e1);
        break;
    }
}

void
IntFlowManager::handleContractUpdate(const opflex::modb::URI& contractURI) {
    LOG(DEBUG) << "Updating contract " << contractURI;

    const string& contractId = contractURI.toString();
    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.contractExists(contractURI)) {  // Contract removed
        switchManager.writeFlow(contractId, POL_TABLE_ID, NULL);
        idGen.erase(getIdNamespace(Contract::CLASS_ID), contractURI.toString());
        return;
    }
    PolicyManager::uri_set_t provURIs;
    PolicyManager::uri_set_t consURIs;
    polMgr.getContractProviders(contractURI, provURIs);
    polMgr.getContractConsumers(contractURI, consURIs);

    typedef unordered_map<uint32_t, uint32_t> IdMap;
    IdMap provIds;
    IdMap consIds;
    getGroupVnidAndRdId(provURIs, provIds);
    getGroupVnidAndRdId(consURIs, consIds);

    PolicyManager::rule_list_t rules;
    polMgr.getContractRules(contractURI, rules);

    LOG(DEBUG) << "Update for contract " << contractURI
               << ", #prov=" << provIds.size()
               << ", #cons=" << consIds.size()
               << ", #rules=" << rules.size();

    FlowEntryList entryList;
    uint64_t conCookie = getId(Contract::CLASS_ID, contractURI);

    BOOST_FOREACH(const IdMap::value_type& pid, provIds) {
        const uint32_t& pvnid = pid.first;
        BOOST_FOREACH(const IdMap::value_type& cid, consIds) {
            const uint32_t& cvnid = cid.first;

            uint16_t prio = flowutils::MAX_POLICY_RULE_PRIORITY;
            BOOST_FOREACH(shared_ptr<PolicyRule>& pc, rules) {
                uint8_t dir = pc->getDirection();
                const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
                /*
                 * Collapse bidirectional rules - if consumer 'cvnid' is also
                 * a provider and provider 'pvnid' is also a consumer, then
                 * add entry for cvnid to pvnid traffic only.
                 */
                if (dir == DirectionEnumT::CONST_BIDIRECTIONAL &&
                    provIds.find(cvnid) != provIds.end() &&
                    consIds.find(pvnid) != consIds.end()) {
                    dir = DirectionEnumT::CONST_IN;
                }
                if (dir == DirectionEnumT::CONST_IN ||
                    dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
                    flowutils::add_classifier_entries(POL_TABLE_ID,
                                                      cls.get(),
                                                      pc->getAllow(),
                                                      OUT_TABLE_ID,
                                                      prio, conCookie,
                                                      cvnid, pvnid,
                                                      entryList);
                }
                if (dir == DirectionEnumT::CONST_OUT ||
                    dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
                    flowutils::add_classifier_entries(POL_TABLE_ID,
                                                      cls.get(),
                                                      pc->getAllow(),
                                                      OUT_TABLE_ID,
                                                      prio, conCookie,
                                                      pvnid, cvnid,
                                                      entryList);
                }
                --prio;
            }
        }
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
    BOOST_FOREACH (FloodGroupMap::value_type& kv, floodGroupMap) {
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
        BOOST_FOREACH (const URI& epg, epgURIs) {
            egDomainUpdated(epg);
        }
        PolicyManager::uri_set_t rdURIs;
        agent.getPolicyManager().getRoutingDomains(rdURIs);
        BOOST_FOREACH (const URI& rd, rdURIs) {
            rdConfigUpdated(rd);
        }
        /* Directly update the group-table */
        updateGroupTable();
    } else {
        unordered_set<string> uuids;
        agent.getEndpointManager().getEndpointsByIface(portName, uuids);
        agent.getEndpointManager().getEndpointsByIpmNextHopIf(portName,
                                                              uuids);
        BOOST_FOREACH (const string& uuid, uuids) {
            endpointUpdated(uuid);
        }

        uuids.clear();
        agent.getServiceManager().getAnycastServicesByIface(portName, uuids);
        BOOST_FOREACH (const string& uuid, uuids) {
            anycastServiceUpdated(uuid);
        }
    }
}

void IntFlowManager::getGroupVnidAndRdId(const unordered_set<URI>& uris,
    /* out */unordered_map<uint32_t, uint32_t>& ids) {
    PolicyManager& pm = agent.getPolicyManager();
    BOOST_FOREACH(const URI& u, uris) {
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
            ids[vnid.get()] = getId(RoutingDomain::CLASS_ID,
                                    rd.get()->getURI());
        }
    }
}

static const boost::function<bool(opflex::ofcore::OFFramework&,
                            const string&,
                            const string&)> ID_NAMESPACE_CB[] =
    {IdGenerator::uriIdGarbageCb<FloodDomain>,
     IdGenerator::uriIdGarbageCb<BridgeDomain>,
     IdGenerator::uriIdGarbageCb<RoutingDomain>,
     IdGenerator::uriIdGarbageCb<Contract>,
     IdGenerator::uriIdGarbageCb<L3ExternalNetwork>};

void IntFlowManager::cleanup() {
    for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
        string ns(ID_NAMESPACES[i]);
        IdGenerator::garbage_cb_t gcb =
            bind(ID_NAMESPACE_CB[i], ref(agent.getFramework()), _1, _2);
        agent.getAgentIOService()
            .dispatch(bind(&IdGenerator::collectGarbage, ref(idGen),
                           ns, gcb));
    }
}

const char * IntFlowManager::getIdNamespace(class_id_t cid) {
    const char *nmspc = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   nmspc = ID_NMSPC_RD; break;
    case BridgeDomain::CLASS_ID:    nmspc = ID_NMSPC_BD; break;
    case FloodDomain::CLASS_ID:     nmspc = ID_NMSPC_FD; break;
    case Contract::CLASS_ID:        nmspc = ID_NMSPC_CON; break;
    case L3ExternalNetwork::CLASS_ID: nmspc = ID_NMSPC_EXTNET; break;
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
    BOOST_FOREACH (MulticastMap::value_type& kv, mcastMap) {
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
    BOOST_FOREACH (MulticastMap::value_type& kv, mcastMap)
        groups.push_back(std::make_pair("", kv.first));
    tree.add_child("multicast-groups", groups);

    try {
        pt::write_json(mcastGroupFile, tree);
    } catch (pt::json_parser_error e) {
        LOG(ERROR) << "Could not write multicast group file " << mcastGroupFile
                   << ": " << e.what();
    }
}

std::vector<FlowEdit>
IntFlowManager::reconcileFlows(std::vector<TableState> flowTables,
                               std::vector<FlowEntryList>& recvFlows) {
    // special handling for learning table - reconcile using
    // PacketInHandler reactive reconciler
    FlowEntryList learnFlows;
    recvFlows[IntFlowManager::LEARN_TABLE_ID].swap(learnFlows);
    BOOST_FOREACH (const FlowEntryPtr& fe, learnFlows) {
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
    if (!GroupEdit::GroupEq(e0, recv)) {
        ge.edits.push_back(e0);
    }
    if (itr != recvGroups.end()) {
        recvGroups.erase(itr);
    }
}

GroupEdit IntFlowManager::reconcileGroups(GroupMap& recvGroups) {
    GroupEdit ge;
    BOOST_FOREACH (FloodGroupMap::value_type& kv, floodGroupMap) {
        const URI& fgrpURI = kv.first;
        Ep2PortMap& epMap = kv.second;

        uint32_t fgrpId = getId(FloodDomain::CLASS_ID, fgrpURI);
        checkGroupEntry(recvGroups, fgrpId, epMap, false, ge);

        uint32_t promFdId = getPromId(fgrpId);
        checkGroupEntry(recvGroups, promFdId, epMap, true, ge);
    }
    Ep2PortMap tmp;
    BOOST_FOREACH (const GroupMap::value_type& kv, recvGroups) {
        GroupEdit::Entry e0 = createGroupMod(OFPGC11_DELETE, kv.first, tmp);
        ge.edits.push_back(e0);
    }
    return ge;
}

void IntFlowManager::completeSync() {
    writeMulticastGroups();
    advertManager.start();
}

} // namespace ovsagent
