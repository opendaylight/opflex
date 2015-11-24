/*
 * Copyright (c) 2014-2015 Cisco Systems, Inc. and others.  All rights reserved.
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
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include <netinet/icmp6.h>

#include <modelgbp/arp/OpcodeEnumT.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/l4/TcpFlagsEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>

#include "logging.h"
#include "Endpoint.h"
#include "EndpointManager.h"
#include "EndpointListener.h"
#include "SwitchConnection.h"
#include "FlowManager.h"
#include "TableState.h"
#include "RangeMask.h"
#include "PacketInHandler.h"
#include "Packets.h"

using std::string;
using std::vector;
using std::ostringstream;
using boost::bind;
using boost::algorithm::split;
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

using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

namespace ovsagent {

const uint16_t FlowManager::MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary
const long DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC = 5000;

// the OUT_MASK specified 7 bits that indicate the action to take in
// the output table.  If nothing is set, then the action is to output
// to the point in REG7
const uint64_t METADATA_OUT_MASK = 0xff;

// Resubmit to the first "dest" table with the source registers set to
// the corresponding values for the EPG in REG7
const uint64_t METADATA_RESUBMIT_DST = 0x1;

// Perform "outbound" NAT action and then resubmit with the source EPG
// set to the mapped NAT EPG
const uint64_t METADATA_NAT_OUT = 0x2;

static const char* ID_NAMESPACES[] =
    {"floodDomain", "bridgeDomain", "routingDomain",
     "contract", "externalNetwork"};

static const char * ID_NMSPC_FD     = ID_NAMESPACES[0];
static const char * ID_NMSPC_BD     = ID_NAMESPACES[1];
static const char * ID_NMSPC_RD     = ID_NAMESPACES[2];
static const char * ID_NMSPC_CON    = ID_NAMESPACES[3];
static const char * ID_NMSPC_EXTNET = ID_NAMESPACES[4];

template <class MO>
static bool idGarbageCb(opflex::ofcore::OFFramework& framework,
                        const string& ns, const URI& uri) {
    return MO::resolve(framework, uri);
}

static boost::function<bool(opflex::ofcore::OFFramework&,
                            const string&,
                            const URI&)> ID_NAMESPACE_CB[] =
    {idGarbageCb<FloodDomain>,
     idGarbageCb<BridgeDomain>,
     idGarbageCb<RoutingDomain>,
     idGarbageCb<Contract>,
     idGarbageCb<L3ExternalNetwork>};

FlowManager::FlowManager(ovsagent::Agent& ag) :
        agent(ag), connection(NULL), executor(NULL), portMapper(NULL),
        reader(NULL), jsonCmdExecutor(NULL), fallbackMode(FALLBACK_PROXY),
        encapType(ENCAP_NONE), floodScope(FLOOD_DOMAIN), tunnelPortStr("4789"),
        virtualRouterEnabled(false), routerAdv(false),
        virtualDHCPEnabled(false),
        isSyncing(false), flowSyncer(*this), pktInHandler(agent, *this),
        advertManager(agent, *this), stopping(false),
        connectDelayMs(DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC),
        opflexPeerConnected(false) {
    memset(routerMac, 0, sizeof(routerMac));
    memset(dhcpMac, 0, sizeof(dhcpMac));
    tunnelDst = address::from_string("127.0.0.1");

    agent.getFramework().registerPeerStatusListener(this);
}

void FlowManager::Start()
{
    /*
     * Start out in syncing mode to avoid writing to the flow tables; we'll
     * update cached state only.
     */
    isSyncing = true;
    if (!flowIdCache.empty()) {
        idGen.setPersistLocation(flowIdCache);
    }
    idGen.initNamespace(ID_NMSPC_FD);
    idGen.initNamespace(ID_NMSPC_BD);
    idGen.initNamespace(ID_NMSPC_RD);
    idGen.initNamespace(ID_NMSPC_CON);
    idGen.initNamespace(ID_NMSPC_EXTNET);

    idCleanupTimer.reset(new deadline_timer(agent.getAgentIOService()));
    idCleanupTimer->expires_from_now(milliseconds(3*60*1000));
    idCleanupTimer->async_wait(bind(&FlowManager::OnIdCleanupTimer,
                                    this, error));

    initPlatformConfig();
}

void FlowManager::Stop()
{
    stopping = true;
    advertManager.stop();
    if (connectTimer) {
        connectTimer->cancel();
    }
    if (idCleanupTimer) {
        idCleanupTimer->cancel();
    }
    if (portMapper) {
        portMapper->unregisterPortStatusListener(this);
    }
}

void FlowManager::registerConnection(SwitchConnection *conn) {
    this->connection = conn;
    conn->RegisterOnConnectListener(this);
    conn->RegisterMessageHandler(OFPTYPE_PACKET_IN, &pktInHandler);
    pktInHandler.registerConnection(conn);
    advertManager.registerConnection(conn);
}

void FlowManager::unregisterConnection(SwitchConnection *conn) {
    advertManager.unregisterConnection();
    pktInHandler.unregisterConnection();
    this->connection = NULL;
    conn->UnregisterOnConnectListener(this);
    conn->UnregisterMessageHandler(OFPTYPE_PACKET_IN, &pktInHandler);
}

void FlowManager::registerModbListeners() {
    agent.getEndpointManager().registerListener(this);
    agent.getServiceManager().registerListener(this);
    agent.getExtraConfigManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
}

void FlowManager::unregisterModbListeners() {
    agent.getEndpointManager().unregisterListener(this);
    agent.getServiceManager().unregisterListener(this);
    agent.getExtraConfigManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);
}

void FlowManager::SetPortMapper(PortMapper *m) {
    portMapper = m;
    portMapper->registerPortStatusListener(this);
    pktInHandler.setPortMapper(m);
    advertManager.setPortMapper(m);
}

void FlowManager::SetFlowReader(FlowReader *r) {
    reader = r;
    pktInHandler.setFlowReader(r);
}

void FlowManager::SetFallbackMode(FallbackMode fallbackMode) {
    this->fallbackMode = fallbackMode;
}

void FlowManager::SetEncapType(EncapType encapType) {
    this->encapType = encapType;
}

void FlowManager::SetEncapIface(const string& encapIf) {
    if (encapIf.empty()) {
        LOG(ERROR) << "Ignoring empty encapsulation interface name";
        return;
    }
    encapIface = encapIf;
}

void FlowManager::SetFloodScope(FloodScope fscope) {
    floodScope = fscope;
}

uint32_t FlowManager::GetTunnelPort() {
    return portMapper ? portMapper->FindPort(encapIface) : OFPP_NONE;
}

void FlowManager::SetTunnelRemoteIp(const string& tunnelRemoteIp) {
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
}

void FlowManager::setTunnelRemotePort(uint16_t tunnelRemotePort) {
    ostringstream ss;
    ss << tunnelRemotePort;
    tunnelPortStr = ss.str();
}

void FlowManager::SetVirtualRouter(bool virtualRouterEnabled,
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

void FlowManager::SetVirtualDHCP(bool dhcpEnabled,
                                 const string& mac) {
    this->virtualDHCPEnabled = dhcpEnabled;
    try {
        MAC(mac).toUIntArray(dhcpMac);
    } catch (std::invalid_argument) {
        LOG(ERROR) << "Invalid virtual DHCP server MAC: " << mac;
    }
}

void FlowManager::SetEndpointAdv(bool endpointAdv) {
    advertManager.enableEndpointAdv(endpointAdv);
}

void FlowManager::SetSyncDelayOnConnect(long delay) {
    connectDelayMs = delay;
}

void FlowManager::SetFlowIdCache(const std::string& flowIdCache) {
    this->flowIdCache = flowIdCache;
}

ovs_be64 FlowManager::GetProactiveLearnEntryCookie() {
    return htonll((uint64_t)1 << 63 | 1);
}

ovs_be64 FlowManager::GetLearnEntryCookie() {
    return htonll((uint64_t)1 << 63 | 2);
}

ovs_be64 FlowManager::GetNDCookie() {
    return htonll((uint64_t)1 << 63 | 3);
}

ovs_be64 FlowManager::GetDHCPCookie(bool v4) {
    return htonll((uint64_t)1 << 63 | (v4 ? 4 : 5));
}

ovs_be64 FlowManager::GetVIPCookie(bool v4) {
    return htonll((uint64_t)1 << 63 | (v4 ? 6 : 7));
}

/** Source table helper functions */
static void
SetSourceAction(FlowEntry *fe, uint32_t epgId,
                uint32_t bdId,  uint32_t fgrpId,  uint32_t l3Id,
                uint8_t nextTable = FlowManager::BRIDGE_TABLE_ID,
                FlowManager::EncapType encapType = FlowManager::ENCAP_NONE)
{
    ActionBuilder ab;
    if (encapType == FlowManager::ENCAP_VLAN)
        ab.SetPopVlan();
    ab.SetRegLoad(MFF_REG0, epgId);
    ab.SetRegLoad(MFF_REG4, bdId);
    ab.SetRegLoad(MFF_REG5, fgrpId);
    ab.SetRegLoad(MFF_REG6, l3Id);
    ab.SetGotoTable(nextTable);

    ab.Build(fe->entry);
}

static void
SetSourceMatchEp(FlowEntry *fe, uint16_t prio, uint32_t ofPort,
        const uint8_t *mac) {
    fe->entry->table_id = FlowManager::SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, ofPort);
    if (mac)
        match_set_dl_src(&fe->entry->match, mac);
}

static void
SetSourceMatchEpg(FlowEntry *fe, FlowManager::EncapType encapType,
                  uint16_t prio, uint32_t tunPort, uint32_t epgId) {
    fe->entry->table_id = FlowManager::SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, tunPort);
    switch (encapType) {
    case FlowManager::ENCAP_VLAN:
        match_set_dl_vlan(&fe->entry->match, htons(0xfff & epgId));
        break;
    case FlowManager::ENCAP_VXLAN:
    case FlowManager::ENCAP_IVXLAN:
    default:
        match_set_tun_id(&fe->entry->match, htonll(epgId));
        break;
    }
}

/** Destination table helper functions */
static void
SetDestMatchEpMac(FlowEntry *fe, uint16_t prio, const uint8_t *mac,
        uint32_t bdId) {
    fe->entry->table_id = FlowManager::BRIDGE_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_dst(&fe->entry->match, mac);
    match_set_reg(&fe->entry->match, 4 /* REG4 */, bdId);
}

static inline void fill_in6_addr(struct in6_addr& addr, const address_v6& ip) {
    address_v6::bytes_type bytes = ip.to_bytes();
    std::memcpy(&addr, bytes.data(), bytes.size());
}

static void
AddMatchIp(FlowEntry *fe, const address& ip, bool src = false) {
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
SetDestMatchEp(FlowEntry *fe, uint16_t prio, const uint8_t *mac,
               const address& ip, uint32_t l3Id) {
    fe->entry->table_id = FlowManager::ROUTE_TABLE_ID;
    fe->entry->priority = prio;
    if (mac)
        match_set_dl_dst(&fe->entry->match, mac);
    AddMatchIp(fe, ip);
    match_set_reg(&fe->entry->match, 6 /* REG6 */, l3Id);
}

static void
SetDestMatchArp(FlowEntry *fe, uint16_t prio, const address& ip,
                uint32_t bdId, uint32_t l3Id) {
    fe->entry->table_id = FlowManager::BRIDGE_TABLE_ID;
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
SetDestMatchNd(FlowEntry *fe, uint16_t prio, const address* ip,
               uint32_t bdId, uint32_t rdId, uint64_t cookie = 0,
               uint8_t type = ND_NEIGHBOR_SOLICIT) {
    fe->entry->table_id = FlowManager::BRIDGE_TABLE_ID;
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
SetMatchFd(FlowEntry *fe, uint16_t prio, uint32_t fgrpId, bool broadcast,
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
AddMatchSubnet(FlowEntry *fe, address& ip, uint8_t prefixLen, bool src) {
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
SetMatchSubnet(FlowEntry *fe, uint32_t rdId,
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
   AddMatchSubnet(fe, ip, prefixLen, src);
}

void FlowManager::SetActionTunnelMetadata(ActionBuilder& ab,
                                          FlowManager::EncapType type,
                                          const address& tunDst) {
    switch (type) {
    case FlowManager::ENCAP_VLAN:
        ab.SetPushVlan();
        ab.SetRegMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case FlowManager::ENCAP_VXLAN:
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        if (tunDst.is_v4()) {
            ab.SetRegLoad(MFF_TUN_DST, tunDst.to_v4().to_ulong());
        } else {
            // this should be unreachable
            LOG(WARNING) << "Ipv6 tunnel destination unsupported";
        }
        // fall through
    case FlowManager::ENCAP_IVXLAN:
        // TODO: set additional ivxlan fields
        break;
    default:
        break;
    }
}

static void
SetDestActionEpMac(FlowEntry *fe, uint32_t epgId, uint32_t port) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    ab.SetGotoTable(FlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionEp(FlowEntry *fe, uint32_t epgId, uint32_t port,
                const uint8_t *specialMac, const uint8_t *dstMac,
                uint8_t nextTable = FlowManager::POL_TABLE_ID) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    ab.SetEthSrcDst(specialMac, dstMac);
    ab.SetDecNwTtl();
    ab.SetGotoTable(nextTable);

    ab.Build(fe->entry);
}

static void
SetDestActionEpArp(FlowEntry *fe, uint32_t epgId, uint32_t port,
                   const uint8_t *dstMac) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    ab.SetEthSrcDst(NULL, dstMac);
    ab.SetGotoTable(FlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionArpReply(FlowEntry *fe, const uint8_t *mac, const address& ip,
                      uint32_t outPort = OFPP_IN_PORT,
                      FlowManager::EncapType type = FlowManager::ENCAP_NONE,
                      const address* tunDst = NULL) {
    ActionBuilder ab;
    ab.SetRegMove(MFF_ETH_SRC, MFF_ETH_DST);
    ab.SetRegLoad(MFF_ETH_SRC, mac);
    ab.SetRegLoad16(MFF_ARP_OP, ARP_OP_REPLY);
    ab.SetRegMove(MFF_ARP_SHA, MFF_ARP_THA);
    ab.SetRegLoad(MFF_ARP_SHA, mac);
    ab.SetRegMove(MFF_ARP_SPA, MFF_ARP_TPA);
    ab.SetRegLoad(MFF_ARP_SPA, ip.to_v4().to_ulong());
    if (type != FlowManager::ENCAP_NONE)
        FlowManager::SetActionTunnelMetadata(ab, type, *tunDst);
    ab.SetOutputToPort(outPort);

    ab.Build(fe->entry);
}

static void
AddActionRevNatDest(ActionBuilder& ab, uint32_t epgVnid, uint32_t bdId,
                    uint32_t fgrpId, uint32_t rdId, uint32_t ofPort) {
    ab.SetRegLoad(MFF_REG2, epgVnid);
    ab.SetRegLoad(MFF_REG4, bdId);
    ab.SetRegLoad(MFF_REG5, fgrpId);
    ab.SetRegLoad(MFF_REG6, rdId);
    ab.SetRegLoad(MFF_REG7, ofPort);
    ab.SetGotoTable(FlowManager::NAT_IN_TABLE_ID);
}

static void
SetActionRevNatDest(FlowEntry *fe, uint32_t epgVnid, uint32_t bdId,
                    uint32_t fgrpId, uint32_t rdId, uint32_t ofPort,
                    const uint8_t* routerMac) {
    ActionBuilder ab;
    ab.SetEthSrcDst(routerMac, NULL);
    AddActionRevNatDest(ab, epgVnid, bdId, fgrpId, rdId, ofPort);
    ab.Build(fe->entry);
}

static void
SetActionRevDNatDest(FlowEntry *fe, uint32_t epgVnid, uint32_t bdId,
                     uint32_t fgrpId, uint32_t rdId, uint32_t ofPort,
                     const uint8_t* routerMac, const uint8_t* macAddr,
                     address& mappedIp) {
    ActionBuilder ab;
    ab.SetEthSrcDst(routerMac, macAddr);
    ab.SetIpDst(mappedIp);
    ab.SetDecNwTtl();
    AddActionRevNatDest(ab, epgVnid, bdId, fgrpId, rdId, ofPort);
    ab.Build(fe->entry);
}

static void
SetActionFdBroadcast(FlowEntry *fe, uint32_t fgrpId) {
    ActionBuilder ab;
    ab.SetGroup(fgrpId);
    ab.Build(fe->entry);
}

static void
SetDestActionOutputToTunnel(FlowEntry *fe, FlowManager::EncapType type,
                            const address& tunDst, uint32_t tunPort) {
    ActionBuilder ab;
    FlowManager::SetActionTunnelMetadata(ab, type, tunDst);
    ab.SetOutputToPort(tunPort);
    ab.Build(fe->entry);
}

static void
SetActionGotoLearn(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetGotoTable(FlowManager::LEARN_TABLE_ID);
    ab.Build(fe->entry);
}

static void
SetActionController(FlowEntry *fe, uint32_t epgId = 0,
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

/** Policy table */
static void
SetPolicyActionAllow(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetGotoTable(FlowManager::OUT_TABLE_ID);
    ab.Build(fe->entry);
}

#if 0
static void
SetPolicyActionConntrack(FlowEntry *fe, uint16_t zone, uint16_t flags,
                         bool output) {
    ActionBuilder ab;
    ab.SetConntrack(zone, flags);
    if (output) {
        ab.SetGotoTable(FlowManager::OUT_TABLE_ID);
    }
    ab.Build(fe->entry);
}
#endif

static void
SetPolicyMatchGroup(FlowEntry *fe, uint16_t prio,
                    uint32_t svnid, uint32_t dvnid) {
    fe->entry->table_id = FlowManager::POL_TABLE_ID;
    fe->entry->priority = prio;
    match_set_reg(&fe->entry->match, 0 /* REG0 */, svnid);
    match_set_reg(&fe->entry->match, 2 /* REG2 */, dvnid);
}

/** Security table */

static void
SetSecurityMatchEpMac(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
}

static void
SetSecurityMatchIP(FlowEntry *fe, uint16_t prio, bool v4) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (v4)
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
    else
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
}

static void
SetSecurityMatchEp(FlowEntry *fe, uint16_t prio, uint32_t port,
                   const uint8_t *epMac, const address& epIp) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
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
SetSecurityMatchEpArp(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac, const address* epIp) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_ARP));
    if (epIp != NULL)
        match_set_nw_src(&fe->entry->match, htonl(epIp->to_v4().to_ulong()));
}

static void
SetSecurityMatchEpND(FlowEntry *fe, uint16_t prio, uint32_t port,
                     const uint8_t *epMac, const address* epIp,
                     bool routerAdv) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
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
        fill_in6_addr(addr, epIp->to_v6());
        match_set_nd_target(&fe->entry->match, &addr);

        // OVS uses tp_dst for ICMPV6_CODE
        match_set_tp_dst(&fe->entry->match, 0);
    }
}

static void
SetSecurityMatchRouterSolit(FlowEntry *fe, uint16_t prio) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
    match_set_nw_proto(&fe->entry->match, 58);
    // OVS uses tp_src for ICMPV6_TYPE
    match_set_tp_src(&fe->entry->match, htons(ND_ROUTER_SOLICIT));
    // OVS uses tp_dst for ICMPV6_CODE
    match_set_tp_dst(&fe->entry->match, 0);
}

static void
SetMatchDHCP(FlowEntry *fe, uint8_t tableId, uint16_t prio, bool v4,
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
SetSecurityActionAllow(FlowEntry *fe,
                       uint8_t nextTableId = FlowManager::SRC_TABLE_ID) {
    ActionBuilder ab;
    ab.SetGotoTable(nextTableId);
    ab.Build(fe->entry);
}

void FlowManager::QueueFlowTask(const boost::function<void ()>& w) {
    agent.getAgentIOService().dispatch(w);
}

void FlowManager::endpointUpdated(const std::string& uuid) {
    if (stopping) return;
    {
        unique_lock<mutex> guard(queueMutex);
        if (!queuedItems.insert(uuid).second) return;
    }

    advertManager.scheduleEndpointAdv(uuid);
    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleEndpointUpdate, this, uuid));
}

void FlowManager::anycastServiceUpdated(const std::string& uuid) {
    if (stopping) return;
    {
        unique_lock<mutex> guard(queueMutex);
        if (!queuedItems.insert(uuid).second) return;
    }

    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleAnycastServiceUpdate, this, uuid));
}

void FlowManager::rdConfigUpdated(const opflex::modb::URI& rdURI) {
    domainUpdated(RoutingDomain::CLASS_ID, rdURI);
}

void FlowManager::egDomainUpdated(const opflex::modb::URI& egURI) {
    if (stopping) return;

    {
        unique_lock<mutex> guard(queueMutex);
        if (!queuedItems.insert(egURI.toString()).second) return;
    }

    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleEndpointGroupDomainUpdate,
                       this, egURI));
}

void FlowManager::domainUpdated(class_id_t cid, const URI& domURI) {
    if (stopping) return;
    {
        unique_lock<mutex> guard(queueMutex);
        if (!queuedItems.insert(domURI.toString()).second) return;
    }

    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleDomainUpdate, this, cid, domURI));
}

void FlowManager::contractUpdated(const opflex::modb::URI& contractURI) {
    if (stopping) return;

    {
        unique_lock<mutex> guard(queueMutex);
        if (!queuedItems.insert(contractURI.toString()).second) return;
    }

    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleContractUpdate, this, contractURI));
}

void FlowManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    PeerConnected();
    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleConfigUpdate, this, configURI));
}

void FlowManager::Connected(SwitchConnection *swConn) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandleConnection, this, swConn));
}

void FlowManager::portStatusUpdate(const string& portName,
                                   uint32_t portNo, bool fromDesc) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(bind(&FlowManager::HandlePortStatusUpdate, this,
                       portName, portNo));
    pktInHandler.portStatusUpdate(portName, portNo, fromDesc);
}

void FlowManager::peerStatusUpdated(const std::string& peerHostname,
                                    int peerPort,
                                    PeerStatus peerStatus) {
    if (stopping || isSyncing) return;
    if (peerStatus == PeerStatusListener::READY) {
        advertManager.scheduleInitialEndpointAdv();
    }
}

void FlowManager::PeerConnected() {
    if (stopping) return;
    if (!opflexPeerConnected) {
        opflexPeerConnected = true;
        LOG(INFO) << "Opflex peer connected";
        /*
         * Set a deadline for sync-ing of switch state. If we get connected
         * to the switch before that, then we'll wait till the deadline
         * expires before attempting to sync.
         */
        if (connectDelayMs > 0) {
            connectTimer.reset(new deadline_timer(agent.getAgentIOService(),
                milliseconds(connectDelayMs)));
        }
        // Pretend that we just got connected to the switch to schedule sync
        if (connection && connection->IsConnected()) {

            agent.getAgentIOService()
                .dispatch(bind(&FlowManager::HandleConnection, this,
                               connection));
        }
    }
}

bool
FlowManager::GetGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
                                    optional<URI>& rdURI, uint32_t& rdId,
                                    optional<URI>& bdURI, uint32_t& bdId,
                                    optional<URI>& fgrpURI, uint32_t& fgrpId) {
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
        bdId = GetId(BridgeDomain::CLASS_ID, bdURI.get());
    }
    fgrpId = 0;
    if (epgFd) {    // FD present -> flooding is desired
        if (floodScope == ENDPOINT_GROUP) {
            fgrpURI = epgURI;
        } else  {
            fgrpURI = epgFd.get()->getURI();
        }
        fgrpId = GetId(FloodDomain::CLASS_ID, fgrpURI.get());
    }
    rdId = 0;
    if (epgRd) {
        rdURI = epgRd.get()->getURI();
        rdId = GetId(RoutingDomain::CLASS_ID, rdURI.get());
    }
    return true;
}

static uint8_t getEffectiveRoutingMode(PolicyManager& polMgr,
                                       const URI& egURI) {
    optional<shared_ptr<FloodDomain> > fd = polMgr.getFDForGroup(egURI);
    optional<shared_ptr<BridgeDomain> > bd = polMgr.getBDForGroup(egURI);

    uint8_t floodMode = UnknownFloodModeEnumT::CONST_DROP;
    if (fd)
        floodMode = fd.get()
            ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);

    uint8_t routingMode = RoutingModeEnumT::CONST_ENABLED;
    if (bd)
        routingMode = bd.get()->getRoutingMode(RoutingModeEnumT::CONST_ENABLED);

    // In learning mode, we can't handle routing at present
    if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD)
        routingMode = RoutingModeEnumT::CONST_DISABLED;

    return routingMode;
}

static void proxyDiscovery(FlowManager& flowMgr, FlowEntryList& elBridgeDst,
                           const address& ipAddr,
                           const uint8_t* macAddr,
                           uint32_t epgVnid, uint32_t rdId,
                           uint32_t bdId) {
    if (ipAddr.is_v4()) {
        uint32_t tunPort = flowMgr.GetTunnelPort();
        if (tunPort != OFPP_NONE &&
            flowMgr.GetEncapType() != FlowManager::ENCAP_NONE) {
            FlowEntry* proxyArpTun = new FlowEntry();
            SetDestMatchArp(proxyArpTun, 21, ipAddr,
                            bdId, rdId);
            match_set_in_port(&proxyArpTun->entry->match, tunPort);
            SetDestActionArpReply(proxyArpTun, macAddr, ipAddr,
                                  OFPP_IN_PORT, flowMgr.GetEncapType(),
                                  &flowMgr.GetTunnelDst());
            elBridgeDst.push_back(FlowEntryPtr(proxyArpTun));
        }
        {
            FlowEntry* proxyArp = new FlowEntry();
            SetDestMatchArp(proxyArp, 20, ipAddr, bdId, rdId);
            SetDestActionArpReply(proxyArp, macAddr, ipAddr);
            elBridgeDst.push_back(FlowEntryPtr(proxyArp));
        }
    } else {
        FlowEntry* proxyND = new FlowEntry();
        // pass MAC address in flow metadata
        uint64_t metadata = 0;
        memcpy(&metadata, macAddr, 6);
        ((uint8_t*)&metadata)[7] = 1;
        SetDestMatchNd(proxyND, 20, &ipAddr, bdId, rdId,
                       FlowManager::GetNDCookie());
        SetActionController(proxyND, epgVnid, 0xffff, metadata);
        elBridgeDst.push_back(FlowEntryPtr(proxyND));
    }
}

static void computeIpmFlows(FlowManager& flowMgr,
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
        nextHopMac ? nextHopMac : flowMgr.GetRouterMacAddr();

    if (!floatingIp.is_unspecified()) {
        {
            // floating IP destination within the same EPG
            // Set up reverse DNAT
            FlowEntry* ipmRoute = new FlowEntry();
            SetDestMatchEp(ipmRoute, 452, NULL, floatingIp, frdId);
            match_set_reg(&ipmRoute->entry->match, 0 /* REG0 */, fepgVnid);
            SetActionRevDNatDest(ipmRoute, epgVnid, bdId,
                                 fgrpId, rdId, ofPort,
                                 flowMgr.GetRouterMacAddr(),
                                 macAddr, mappedIp);

            elRouteDst.push_back(FlowEntryPtr(ipmRoute));
        }
        {
            // Floating IP destination across EPGs
            // Apply policy for source EPG -> floating IP EPG
            // then resubmit with source EPG set to floating
            // IP EPG

            FlowEntry* ipmResub = new FlowEntry();
            SetDestMatchEp(ipmResub, 450, NULL, floatingIp, frdId);
            ActionBuilder ab;
            ab.SetRegLoad(MFF_REG2, fepgVnid);
            ab.SetRegLoad(MFF_REG7, fepgVnid);
            ab.SetWriteMetadata(METADATA_RESUBMIT_DST,
                                METADATA_OUT_MASK);
            ab.SetGotoTable(FlowManager::POL_TABLE_ID);
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
        ipmNatOut->entry->table_id = FlowManager::OUT_TABLE_ID;
        ipmNatOut->entry->priority = 10;
        match_set_metadata_masked(&ipmNatOut->entry->match,
                                  htonll(METADATA_NAT_OUT),
                                  htonll(METADATA_OUT_MASK));
        match_set_reg(&ipmNatOut->entry->match, 6 /* REG6 */, rdId);
        match_set_reg(&ipmNatOut->entry->match, 7 /* REG7 */, fepgVnid);
        AddMatchIp(ipmNatOut, mappedIp, true);

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
            ab.SetResubmit(OFPP_IN_PORT, FlowManager::BRIDGE_TABLE_ID);
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
            SetSourceMatchEp(ipmNextHopRev, 201,
                             nextHopPort, effNextHopMac);
            AddMatchIp(ipmNextHopRev, floatingIp);
            SetActionRevDNatDest(ipmNextHopRev, epgVnid, bdId,
                                 fgrpId, rdId, ofPort,
                                 flowMgr.GetRouterMacAddr(),
                                 macAddr, mappedIp);
            elSrc.push_back(FlowEntryPtr(ipmNextHopRev));
        }
        {
            // Reverse traffic from next hop interface where we
            // delivered with an SKB mark.  The SKB mark must be set
            // to the routing domain for the mapped destination IP
            // address
            FlowEntry* ipmNextHopRevMark = new FlowEntry();
            SetSourceMatchEp(ipmNextHopRevMark, 200,
                             nextHopPort, effNextHopMac);
            match_set_pkt_mark(&ipmNextHopRevMark->entry->match, rdId);
            AddMatchIp(ipmNextHopRevMark, mappedIp);
            SetActionRevNatDest(ipmNextHopRevMark, epgVnid, bdId,
                                fgrpId, rdId, ofPort,
                                nextHopMac ? flowMgr.GetRouterMacAddr() : NULL);
            elSrc.push_back(FlowEntryPtr(ipmNextHopRevMark));
        }
    }
}

static void virtualDHCP(FlowEntryList& elSrc, uint32_t ofPort,
                        uint32_t epgVnid, uint8_t* macAddr, bool v4) {
    FlowEntry* dhcp = new FlowEntry();
    SetSourceMatchEp(dhcp, 150, ofPort, macAddr);
    SetMatchDHCP(dhcp, FlowManager::SRC_TABLE_ID, 150, v4,
                 FlowManager::GetDHCPCookie(v4));
    SetActionController(dhcp, epgVnid, 0xffff);

    elSrc.push_back(FlowEntryPtr(dhcp));
}

void
FlowManager::HandleEndpointUpdate(const string& uuid) {

    LOG(DEBUG) << "Updating endpoint " << uuid;
    {
        unique_lock<mutex> guard(queueMutex);
        queuedItems.erase(uuid);
    }

    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        WriteFlow(uuid, SEC_TABLE_ID, NULL);
        WriteFlow(uuid, SRC_TABLE_ID, NULL);
        WriteFlow(uuid, BRIDGE_TABLE_ID, NULL);
        WriteFlow(uuid, ROUTE_TABLE_ID, NULL);
        WriteFlow(uuid, LEARN_TABLE_ID, NULL);
        WriteFlow(uuid, SERVICE_MAP_DST_TABLE_ID, NULL);
        RemoveEndpointFromFloodGroup(uuid);
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

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = endPoint.getInterfaceName();
    if (ofPortName && portMapper) {
        ofPort = portMapper->FindPort(ofPortName.get());
    }

    /* Port security flows */
    FlowEntryList el;
    if (ofPort != OFPP_NONE) {
        if (endPoint.isPromiscuousMode()) {
            FlowEntry *e0 = new FlowEntry();
            SetSecurityMatchEpMac(e0, 50, ofPort, NULL);
            SetSecurityActionAllow(e0);
            el.push_back(FlowEntryPtr(e0));
        } else if (hasMac) {
            FlowEntry *e0 = new FlowEntry();
            SetSecurityMatchEpMac(e0, 20, ofPort, macAddr);
            SetSecurityActionAllow(e0);
            el.push_back(FlowEntryPtr(e0));

            BOOST_FOREACH(const address& ipAddr, ipAddresses) {
                FlowEntry *e1 = new FlowEntry();
                SetSecurityMatchEp(e1, 30, ofPort, macAddr, ipAddr);
                SetSecurityActionAllow(e1);
                el.push_back(FlowEntryPtr(e1));

                if (ipAddr.is_v4()) {
                    FlowEntry *e2 = new FlowEntry();
                    SetSecurityMatchEpArp(e2, 40, ofPort, macAddr, &ipAddr);
                    SetSecurityActionAllow(e2);
                    el.push_back(FlowEntryPtr(e2));
                } else {
                    FlowEntry *e2 = new FlowEntry();
                    SetSecurityMatchEpND(e2, 40, ofPort, macAddr,
                                         &ipAddr, false);
                    SetSecurityActionAllow(e2);
                    el.push_back(FlowEntryPtr(e2));
                }
            }
        }

        BOOST_FOREACH(const Endpoint::virt_ip_t& vip,
                      endPoint.getVirtualIPs()) {
            // Don't write virtual IP entry if the endpoint already
            // has an "active" IP address for this entry.
            if (endPoint.getIPs().find(vip.second) != endPoint.getIPs().end())
                continue;

            address addr = address::from_string(vip.second, ec);
            if (ec) {
                LOG(WARNING) << "Invalid endpoint VIP: "
                             << vip.second << ": " << ec.message();
                continue;
            }
            uint8_t vmac[6];
            vip.first.toUIntArray(vmac);

            FlowEntry *vf = new FlowEntry();
            if (addr.is_v4()) {
                SetSecurityMatchEpArp(vf, 60, ofPort, vmac, &addr);
                vf->entry->cookie = GetVIPCookie(true);
            } else {
                SetSecurityMatchEpND(vf, 60, ofPort, vmac, &addr, false);
                vf->entry->cookie = GetVIPCookie(false);
            }
            ActionBuilder ab;
            ab.SetController(0xffff);
            ab.SetGotoTable(FlowManager::SRC_TABLE_ID);
            ab.Build(vf->entry);
            el.push_back(FlowEntryPtr(vf));
        }
    }
    WriteFlow(uuid, SEC_TABLE_ID, el);

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!GetGroupForwardingInfo(epgURI.get(), epgVnid, rdURI,
                                rdId, bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    optional<shared_ptr<FloodDomain> > fd =
        agent.getPolicyManager().getFDForGroup(epgURI.get());

    uint8_t arpMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t ndMode = AddressResModeEnumT::CONST_UNICAST;
    uint8_t floodMode = UnknownFloodModeEnumT::CONST_DROP;
    if (fd) {
        // Irrespective of flooding scope (epg vs. flood-domain), the
        // properties of the flood-domain object decide how flooding
        // is done.

        arpMode = fd.get()
            ->getArpMode(AddressResModeEnumT::CONST_UNICAST);
        ndMode = fd.get()
            ->getNeighborDiscMode(AddressResModeEnumT::CONST_UNICAST);
        floodMode = fd.get()
            ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
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
            SetSourceMatchEp(e0, 140, ofPort, macAddr);
            SetSourceAction(e0, epgVnid, bdId, fgrpId, rdId);
            elSrc.push_back(FlowEntryPtr(e0));

            if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
                // Prepopulate a stage1 learning entry for known EPs
                FlowEntry* learnEntry = new FlowEntry();
                SetMatchFd(learnEntry, 101, fgrpId, false, LEARN_TABLE_ID,
                           macAddr, GetProactiveLearnEntryCookie());
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
            SetSourceMatchEp(e1, 138, ofPort, NULL);
            SetSourceAction(e1, epgVnid, bdId, fgrpId, rdId, LEARN_TABLE_ID);
            elSrc.push_back(FlowEntryPtr(e1));

            // Multicast traffic from promiscuous ports is delivered
            // normally
            FlowEntry *e2 = new FlowEntry();
            SetSourceMatchEp(e2, 139, ofPort, NULL);
            match_set_dl_dst_masked(&e2->entry->match,
                                    packets::MAC_ADDR_BROADCAST,
                                    packets::MAC_ADDR_MULTICAST);
            SetSourceAction(e2, epgVnid, bdId, fgrpId, rdId);
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
                address addr = address::from_string(vip.second, ec);
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
        SetDestMatchEpMac(e0, 10, macAddr, bdId);
        SetDestActionEpMac(e0, epgVnid, ofPort);
        elBridgeDst.push_back(FlowEntryPtr(e0));
    }

    if (rdId != 0 && bdId != 0 && ofPort != OFPP_NONE) {
        uint8_t routingMode =
            getEffectiveRoutingMode(agent.getPolicyManager(), epgURI.get());

        if (virtualRouterEnabled && hasMac &&
            routingMode == RoutingModeEnumT::CONST_ENABLED) {
            BOOST_FOREACH (const address& ipAddr, ipAddresses) {
                {
                    FlowEntry *e0 = new FlowEntry();
                    SetDestMatchEp(e0, 500, GetRouterMacAddr(), ipAddr, rdId);
                    SetDestActionEp(e0, epgVnid, ofPort, GetRouterMacAddr(),
                                    macAddr);
                    elRouteDst.push_back(FlowEntryPtr(e0));
                }

                if (endPoint.isDiscoveryProxyMode()) {
                    // Auto-reply to ARP and NDP requests for endpoint
                    // IP addresses
                    proxyDiscovery(*this, elBridgeDst, ipAddr,
                                   macAddr, epgVnid, rdId, bdId);
                } else {
                    if (arpMode != AddressResModeEnumT::CONST_FLOOD &&
                        ipAddr.is_v4()) {
                        FlowEntry *e1 = new FlowEntry();
                        SetDestMatchArp(e1, 20, ipAddr, bdId, rdId);
                        if (arpMode == AddressResModeEnumT::CONST_UNICAST) {
                            // ARP optimization: broadcast -> unicast
                            SetDestActionEpArp(e1, epgVnid, ofPort, macAddr);
                        }
                        // else drop the ARP packet
                        elBridgeDst.push_back(FlowEntryPtr(e1));
                    }

                    if (ndMode != AddressResModeEnumT::CONST_FLOOD &&
                        ipAddr.is_v6()) {
                        FlowEntry *e1 = new FlowEntry();
                        SetDestMatchNd(e1, 20, &ipAddr, bdId, rdId);
                        if (ndMode == AddressResModeEnumT::CONST_UNICAST) {
                            // neighbor discovery optimization:
                            // broadcast -> unicast
                            SetDestActionEpArp(e1, epgVnid, ofPort, macAddr);
                        }
                        // else drop the ND packet
                        elBridgeDst.push_back(FlowEntryPtr(e1));
                    }
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
                if (!GetGroupForwardingInfo(ipm.getEgURI().get(), fepgVnid,
                                            frdURI, frdId, fbdURI, fbdId,
                                            ffdURI, ffdId))
                    continue;

                uint32_t nextHop = OFPP_NONE;
                if (ipm.getNextHopIf() && portMapper) {
                    nextHop = portMapper->FindPort(ipm.getNextHopIf().get());
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
            BOOST_FOREACH (const address& ipAddr, ipAddresses) {
                {
                    FlowEntry *serviceDest = new FlowEntry();
                    SetDestMatchEp(serviceDest, 50, NULL, ipAddr, rdId);
                    serviceDest->entry->table_id =
                        FlowManager::SERVICE_MAP_DST_TABLE_ID;
                    ActionBuilder ab;
                    ab.SetEthSrcDst(GetRouterMacAddr(), macAddr);
                    ab.SetDecNwTtl();
                    ab.SetOutputToPort(ofPort);
                    ab.Build(serviceDest->entry);
                    elServiceMap.push_back(FlowEntryPtr(serviceDest));
                }
                if (ipAddr.is_v4()) {
                    FlowEntry *proxyArp = new FlowEntry();
                    SetDestMatchArp(proxyArp, 51, ipAddr, 0, rdId);
                    proxyArp->entry->table_id =
                        FlowManager::SERVICE_MAP_DST_TABLE_ID;
                    SetDestActionArpReply(proxyArp, macAddr, ipAddr);
                    elServiceMap.push_back(FlowEntryPtr(proxyArp));
                } else {
                    FlowEntry* proxyND = new FlowEntry();
                    // pass MAC address in flow metadata
                    uint64_t metadata = 0;
                    memcpy(&metadata, macAddr, 6);
                    ((uint8_t*)&metadata)[7] = 1;
                    SetDestMatchNd(proxyND, 51, &ipAddr, 0, rdId,
                                   FlowManager::GetNDCookie());
                    proxyND->entry->table_id =
                        FlowManager::SERVICE_MAP_DST_TABLE_ID;
                    SetActionController(proxyND, 0, 0xffff, metadata);
                    elServiceMap.push_back(FlowEntryPtr(proxyND));
                }
            }
        }
    }

    WriteFlow(uuid, SRC_TABLE_ID, elSrc);
    WriteFlow(uuid, LEARN_TABLE_ID, elEpLearn);
    WriteFlow(uuid, BRIDGE_TABLE_ID, elBridgeDst);
    WriteFlow(uuid, ROUTE_TABLE_ID, elRouteDst);
    WriteFlow(uuid, SERVICE_MAP_DST_TABLE_ID, elServiceMap);
    WriteFlow(uuid, OUT_TABLE_ID, elOutput);

    if (fgrpURI && ofPort != OFPP_NONE) {
        UpdateEndpointFloodGroup(fgrpURI.get(), endPoint, ofPort,
                                 endPoint.isPromiscuousMode(),
                                 fd);
    } else {
        RemoveEndpointFromFloodGroup(uuid);
    }
}

void
FlowManager::HandleAnycastServiceUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating anycast service " << uuid;
    {
        unique_lock<mutex> guard(queueMutex);
        queuedItems.erase(uuid);
    }

    ServiceManager& srvMgr = agent.getServiceManager();
    shared_ptr<const AnycastService> asWrapper = srvMgr.getAnycastService(uuid);

    if (!asWrapper) {
        WriteFlow(uuid, SEC_TABLE_ID, NULL);
        WriteFlow(uuid, BRIDGE_TABLE_ID, NULL);
        WriteFlow(uuid, SERVICE_MAP_DST_TABLE_ID, NULL);
        return;
    }

    const AnycastService& as = *asWrapper;

    FlowEntryList secFlows;
    FlowEntryList bridgeFlows;
    FlowEntryList serviceMapDstFlows;

    boost::system::error_code ec;

    uint32_t ofPort = OFPP_NONE;
    const optional<string>& ofPortName = as.getInterfaceName();
    if (portMapper && ofPortName) {
        ofPort = portMapper->FindPort(as.getInterfaceName().get());
    }

    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(),
                               as.getDomainURI().get());

    if (ofPort != OFPP_NONE && as.getServiceMAC() && rd) {
        uint8_t macAddr[6];
        as.getServiceMAC().get().toUIntArray(macAddr);

        uint32_t rdId =
            GetId(RoutingDomain::CLASS_ID, as.getDomainURI().get());

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
                SetDestMatchEp(serviceDest, 50, NULL, addr, rdId);
                serviceDest->entry->table_id = FlowManager::BRIDGE_TABLE_ID;
                ActionBuilder ab;
                ab.SetEthSrcDst(GetRouterMacAddr(), macAddr);
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
                    AddMatchIp(svcIP, nextHopAddr, true);
                    ab.SetIpSrc(addr);
                    ab.SetDecNwTtl();
                } else {
                    AddMatchIp(svcIP, addr, true);
                }

                ab.SetGotoTable(SERVICE_MAP_DST_TABLE_ID);
                ab.Build(svcIP->entry);
                secFlows.push_back(FlowEntryPtr(svcIP));
            }

            if (addr.is_v4()) {
                FlowEntry *proxyArp = new FlowEntry();
                SetDestMatchArp(proxyArp, 51, addr, 0, rdId);
                SetDestActionArpReply(proxyArp, macAddr, addr);
                bridgeFlows.push_back(FlowEntryPtr(proxyArp));
            } else {
                FlowEntry* proxyND = new FlowEntry();
                // pass MAC address in flow metadata
                uint64_t metadata = 0;
                memcpy(&metadata, macAddr, 6);
                ((uint8_t*)&metadata)[7] = 1;
                SetDestMatchNd(proxyND, 51, &addr, 0, rdId,
                               FlowManager::GetNDCookie());
                SetActionController(proxyND, 0, 0xffff, metadata);
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
                        SetDestMatchArp(gwArp, 31, gwAddr, 0, rdId);
                        match_set_dl_src(&gwArp->entry->match, macAddr);
                        gwArp->entry->table_id =
                            FlowManager::SERVICE_MAP_DST_TABLE_ID;
                        SetDestActionArpReply(gwArp, GetRouterMacAddr(),
                                              gwAddr);
                        serviceMapDstFlows.push_back(FlowEntryPtr(gwArp));
                    } else {
                        FlowEntry* gwND = new FlowEntry();
                        // pass MAC address in flow metadata
                        uint64_t metadata = 0;
                        memcpy(&metadata, macAddr, 6);
                        ((uint8_t*)&metadata)[7] = 1;
                        ((uint8_t*)&metadata)[6] = 1;
                        SetDestMatchNd(gwND, 31, &gwAddr, 0, rdId,
                                       FlowManager::GetNDCookie());
                        match_set_dl_src(&gwND->entry->match, macAddr);
                        gwND->entry->table_id =
                            FlowManager::SERVICE_MAP_DST_TABLE_ID;
                        SetActionController(gwND, 0, 0xffff, metadata);
                        serviceMapDstFlows.push_back(FlowEntryPtr(gwND));
                    }
                }
            }
        }
    }

    WriteFlow(uuid, SEC_TABLE_ID, secFlows);
    WriteFlow(uuid, BRIDGE_TABLE_ID, bridgeFlows);
    WriteFlow(uuid, SERVICE_MAP_DST_TABLE_ID, serviceMapDstFlows);
}

void
FlowManager::HandleEndpointGroupDomainUpdate(const URI& epgURI) {
    LOG(DEBUG) << "Updating endpoint-group " << epgURI;
    {
        unique_lock<mutex> guard(queueMutex);
        queuedItems.erase(epgURI.toString());
    }

    const string& epgId = epgURI.toString();

    uint32_t tunPort = GetTunnelPort();

    {
        // write static port security flows that do not depend on any
        // particular EPG
        FlowEntryList fixedFlows;

        // Drop IP traffic that doesn't have the correct source
        // address
        FlowEntry *dropARP = new FlowEntry();
        SetSecurityMatchEpArp(dropARP, 25, OFPP_ANY, NULL, 0);
        fixedFlows.push_back(FlowEntryPtr(dropARP));

        FlowEntry *dropIPv4 = new FlowEntry();
        SetSecurityMatchIP(dropIPv4, 25, true);
        fixedFlows.push_back(FlowEntryPtr(dropIPv4));

        FlowEntry *dropIPv6 = new FlowEntry();
        SetSecurityMatchIP(dropIPv6, 25, false);
        fixedFlows.push_back(FlowEntryPtr(dropIPv6));

        // Allow DHCP requests but not replies
        FlowEntry *allowDHCP = new FlowEntry();
        SetMatchDHCP(allowDHCP, FlowManager::SEC_TABLE_ID, 27, true);
        SetSecurityActionAllow(allowDHCP);
        fixedFlows.push_back(FlowEntryPtr(allowDHCP));

        // Allow IPv6 autoconfiguration (DHCP & router solicitiation)
        // requests but not replies
        FlowEntry *allowDHCPv6 = new FlowEntry();
        SetMatchDHCP(allowDHCPv6, FlowManager::SEC_TABLE_ID, 27, false);
        SetSecurityActionAllow(allowDHCPv6);
        fixedFlows.push_back(FlowEntryPtr(allowDHCPv6));

        FlowEntry *allowRouterSolit = new FlowEntry();
        SetSecurityMatchRouterSolit(allowRouterSolit, 27);
        SetSecurityActionAllow(allowRouterSolit);
        fixedFlows.push_back(FlowEntryPtr(allowRouterSolit));

        if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
            // allow all traffic from the tunnel uplink through the port
            // security table
            FlowEntry *allowTunnel = new FlowEntry();
            SetSecurityMatchEpMac(allowTunnel, 50, tunPort, NULL);
            SetSecurityActionAllow(allowTunnel);
            fixedFlows.push_back(FlowEntryPtr(allowTunnel));
        }

        WriteFlow("static", SEC_TABLE_ID, fixedFlows);
    }

    {
        FlowEntry *unknownTunnelBr = NULL;
        FlowEntry *unknownTunnelRt = NULL;
        if (fallbackMode == FALLBACK_PROXY && tunPort != OFPP_NONE &&
            encapType != ENCAP_NONE) {
            // Output to the tunnel interface, bypassing policy
            // note that if the flood domain is set to flood unknown, then
            // there will be a higher-priority rule installed for that
            // flood domain.
            unknownTunnelBr = new FlowEntry();
            unknownTunnelBr->entry->priority = 1;
            unknownTunnelBr->entry->table_id = BRIDGE_TABLE_ID;
            SetDestActionOutputToTunnel(unknownTunnelBr, encapType,
                                        GetTunnelDst(), tunPort);

            if (virtualRouterEnabled) {
                unknownTunnelRt = new FlowEntry();
                unknownTunnelRt->entry->priority = 1;
                unknownTunnelRt->entry->table_id = ROUTE_TABLE_ID;
                SetDestActionOutputToTunnel(unknownTunnelRt, encapType,
                                            GetTunnelDst(), tunPort);
            }
        }
        WriteFlow("static", BRIDGE_TABLE_ID, unknownTunnelBr);
        WriteFlow("static", ROUTE_TABLE_ID, unknownTunnelRt);
    }
    {
        FlowEntryList outFlows;
        {
            FlowEntry* outputReg = new FlowEntry();
            outputReg->entry->table_id = OUT_TABLE_ID;
            outputReg->entry->priority = 1;
            match_set_metadata_masked(&outputReg->entry->match,
                                      0, htonll(METADATA_OUT_MASK));
            ActionBuilder ab;
            ab.SetOutputReg(MFF_REG7);
            ab.Build(outputReg->entry);

            outFlows.push_back(FlowEntryPtr(outputReg));
        }

        WriteFlow("static", OUT_TABLE_ID, outFlows);
    }

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {  // EPG removed
        WriteFlow(epgId, SRC_TABLE_ID, NULL);
        WriteFlow(epgId, POL_TABLE_ID, NULL);
        WriteFlow(epgId, OUT_TABLE_ID, NULL);
        updateMulticastList(boost::none, epgURI);
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, bdURI, rdURI;
    if (!GetGroupForwardingInfo(epgURI, epgVnid, rdURI, rdId,
                                bdURI, bdId, fgrpURI, fgrpId)) {
        return;
    }

    FlowEntryList uplinkMatch;
    if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
        // In flood mode we send all traffic from the uplink to the
        // learning table.  Otherwise move to the destination mapper
        // table as normal.  Multicast traffic still goes to the
        // destination table, however.

        uint8_t floodMode = UnknownFloodModeEnumT::CONST_DROP;
        optional<shared_ptr<FloodDomain> > epgFd = polMgr.getFDForGroup(epgURI);
        if (epgFd) {
            floodMode = epgFd.get()
                ->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
        }

        uint8_t nextTable = FlowManager::BRIDGE_TABLE_ID;
        if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD)
            nextTable = FlowManager::LEARN_TABLE_ID;

        // Assign the source registers based on the VNID from the
        // tunnel uplink
        FlowEntry *e0 = new FlowEntry();
        SetSourceMatchEpg(e0, encapType, 149, tunPort, epgVnid);
        SetSourceAction(e0, epgVnid, bdId, fgrpId, rdId,
                        nextTable, encapType);
        uplinkMatch.push_back(FlowEntryPtr(e0));

        if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
            FlowEntry *e1 = new FlowEntry();
            SetSourceMatchEpg(e1, encapType, 150, tunPort, epgVnid);
            match_set_dl_dst_masked(&e1->entry->match,
                                    packets::MAC_ADDR_BROADCAST,
                                    packets::MAC_ADDR_MULTICAST);
            SetSourceAction(e1, epgVnid, bdId, fgrpId, rdId,
                            FlowManager::BRIDGE_TABLE_ID, encapType);
            uplinkMatch.push_back(FlowEntryPtr(e1));
        }
    }
    WriteFlow(epgId, SRC_TABLE_ID, uplinkMatch);

    uint8_t intraGroup = IntraGroupPolicyEnumT::CONST_ALLOW;
    optional<shared_ptr<EpGroup> > epg =
        EpGroup::resolve(agent.getFramework(), epgURI);
    if (epg && epg.get()->isIntraGroupPolicySet()) {
        intraGroup = epg.get()->getIntraGroupPolicy().get();
    }

    if (intraGroup == IntraGroupPolicyEnumT::CONST_ALLOW) {
        FlowEntry *e0 = new FlowEntry();
        SetPolicyMatchGroup(e0, 100, epgVnid, epgVnid);
        SetPolicyActionAllow(e0);
        WriteFlow(epgId, POL_TABLE_ID, e0);
    } else {
        WriteFlow(epgId, POL_TABLE_ID, NULL);
    }

    if (virtualRouterEnabled && rdId != 0 && bdId != 0) {
        UpdateGroupSubnets(epgURI, bdId, rdId);

        FlowEntryList bridgel;
        uint8_t routingMode =
            getEffectiveRoutingMode(agent.getPolicyManager(), epgURI);

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
                SetDestMatchNd(r, 20, NULL, bdId, rdId,
                               FlowManager::GetNDCookie(),
                               ND_ROUTER_SOLICIT);
                SetActionController(r);
                bridgel.push_back(FlowEntryPtr(r));

                if (!isSyncing)
                    advertManager.scheduleInitialRouterAdv();
            }
        }
        WriteFlow(bdURI.get().toString(), BRIDGE_TABLE_ID, bridgel);
    }

    FlowEntryList egOutFlows;
    {
        // Output table action to resubmit the flow to the bridge
        // table with source registers set to the current EPG
        FlowEntry* resubmitDst = new FlowEntry();
        resubmitDst->entry->table_id = OUT_TABLE_ID;
        resubmitDst->entry->priority = 10;
        match_set_reg(&resubmitDst->entry->match, 7 /* REG7 */, epgVnid);
        match_set_metadata_masked(&resubmitDst->entry->match,
                                  htonll(METADATA_RESUBMIT_DST),
                                  htonll(METADATA_OUT_MASK));
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
    WriteFlow(epgId, OUT_TABLE_ID, egOutFlows);

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
FlowManager::UpdateGroupSubnets(const URI& egURI, uint32_t bdId, uint32_t rdId) {
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(egURI, subnets);

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
                    optional<const string&> routerIpStr = sn->getVirtualRouterIp();
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
                    address_v6::bytes_type rip;
                    packets::construct_auto_ip(address_v6::from_string("fe80::"),
                                               routerMac,
                                               (struct in6_addr*)rip.data());
                    routerIp = address_v6(rip);
                    hasRouterIp = true;
                }
            }
            if (hasRouterIp) {
                if (routerIp.is_v4()) {
                    FlowEntry *e0 = new FlowEntry();
                    SetDestMatchArp(e0, 20, routerIp, bdId, rdId);
                    SetDestActionArpReply(e0, GetRouterMacAddr(), routerIp);
                    el.push_back(FlowEntryPtr(e0));
                } else {
                    FlowEntry *e0 = new FlowEntry();
                    SetDestMatchNd(e0, 20, &routerIp, bdId, rdId,
                                   FlowManager::GetNDCookie());
                    SetActionController(e0);
                    el.push_back(FlowEntryPtr(e0));
                }
            }
        }
        WriteFlow(sn->getURI().toString(), BRIDGE_TABLE_ID, el);
    }
}

static
string getMaskedAddress(const std::string& addrStr, uint16_t prefixLen) {
    boost::system::error_code ec;
    string maskedIpStr;
    address ip = address::from_string(addrStr, ec);
    if (ec) {
        LOG(ERROR) << "Invalid IP address: " << addrStr << ": "
            << ec.message();
        return maskedIpStr;
    }
    if (ip.is_v4()) {
        uint32_t mask = (prefixLen != 0)
            ? (~((uint32_t)0) << (32 - prefixLen))
            : 0;
        maskedIpStr =
            boost::asio::ip::address_v4(ip.to_v4().to_ulong() & mask)
                .to_string();
    } else {
       struct in6_addr mask;
       struct in6_addr addr6;
       packets::compute_ipv6_subnet(ip.to_v6(), prefixLen, &mask, &addr6);
       boost::asio::ip::address_v6::bytes_type data;
       std::memcpy(data.data(), &addr6, sizeof(addr6));
       maskedIpStr = boost::asio::ip::address_v6(data).to_string();
    }
    return maskedIpStr;
}

void
FlowManager::HandleRoutingDomainUpdate(const URI& rdURI) {
    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(agent.getFramework(), rdURI);

    if (!rd) {
        LOG(DEBUG) << "Cleaning up for RD: " << rdURI;
        WriteFlow(rdURI.toString(), NAT_IN_TABLE_ID, NULL);
        WriteFlow(rdURI.toString(), ROUTE_TABLE_ID, NULL);
        idGen.erase(GetIdNamespace(RoutingDomain::CLASS_ID), rdURI);
        return;
    }
    LOG(DEBUG) << "Updating routing domain " << rdURI;

    FlowEntryList rdRouteFlows;
    FlowEntryList rdNatFlows;
    boost::system::error_code ec;
    uint32_t tunPort = GetTunnelPort();
    uint32_t rdId = GetId(RoutingDomain::CLASS_ID, rdURI);

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
            string maskedAddr = getMaskedAddress(subnet->getAddress().get(),
                                                 subnet->getPrefixLen().get());
            if (!maskedAddr.empty()) {
                intSubnets.insert(
                    make_pair(maskedAddr, subnet->getPrefixLen().get()));
            }
        }
    }
    boost::shared_ptr<const RDConfig> rdConfig =
        agent.getExtraConfigManager().getRDConfig(rdURI);
    if (rdConfig) {
        BOOST_FOREACH(const std::string& cidrSn,
                      rdConfig->getInternalSubnets()) {
            vector<string> comps;
            split(comps, cidrSn, boost::is_any_of("/"));
            if (comps.size() == 2) {
                try {
                    uint8_t prefixLen =
                        boost::lexical_cast<uint16_t>(comps[1]);
                    string maskedAddr = getMaskedAddress(comps[0], prefixLen);
                    if (!maskedAddr.empty()) {
                        intSubnets.insert(make_pair(maskedAddr, prefixLen));
                    }
                } catch (const boost::bad_lexical_cast& e) {
                    LOG(ERROR) << "Invalid CIDR subnet prefix length: "
                               << comps[1] << ": " << e.what();
                }
            } else {
                LOG(ERROR) << "Invalid CIDR subnet: " << cidrSn;
            }
        }
    }
    BOOST_FOREACH(const subnet_t& sn, intSubnets) {
        address addr = address::from_string(sn.first, ec);
        if (ec) continue;

        FlowEntry *snr = new FlowEntry();
        SetMatchSubnet(snr, rdId, 300, ROUTE_TABLE_ID, addr,
                       sn.second, false);
        if (fallbackMode == FALLBACK_PROXY && tunPort != OFPP_NONE &&
            encapType != ENCAP_NONE) {
            SetDestActionOutputToTunnel(snr, encapType,
                                        GetTunnelDst(), tunPort);
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
                    SetMatchSubnet(snr, rdId, 150, ROUTE_TABLE_ID, addr,
                                   extsub->getPrefixLen(0), false);

                    if (natRef) {
                        // For external networks mapped to a NAT EPG,
                        // set the next hop action to NAT_OUT
                        ActionBuilder ab;
                        if (natEpgVnid) {
                            ab.SetRegLoad(MFF_REG2, netVnid);
                            ab.SetRegLoad(MFF_REG7, natEpgVnid.get());
                            ab.SetWriteMetadata(METADATA_NAT_OUT,
                                                METADATA_OUT_MASK);
                            ab.SetGotoTable(POL_TABLE_ID);
                            ab.Build(snr->entry);
                        }
                        // else drop until resolved
                    } else if (fallbackMode == FALLBACK_PROXY &&
                               tunPort != OFPP_NONE &&
                               encapType != ENCAP_NONE) {
                        // For other external networks, output to the tunnel
                        SetDestActionOutputToTunnel(snr, encapType,
                                                    GetTunnelDst(), tunPort);
                    }
                    // else drop the packets
                    rdRouteFlows.push_back(FlowEntryPtr(snr));
                }
                {
                    FlowEntry *snn = new FlowEntry();
                    SetMatchSubnet(snn, rdId, 150, NAT_IN_TABLE_ID, addr,
                                   extsub->getPrefixLen(0), true);
                    ActionBuilder ab;
                    ab.SetRegLoad(MFF_REG0, netVnid);
                    ab.SetGotoTable(POL_TABLE_ID);
                    ab.Build(snn->entry);
                    rdNatFlows.push_back(FlowEntryPtr(snn));
                }
            }
        }
    }

    WriteFlow(rdURI.toString(), NAT_IN_TABLE_ID, rdNatFlows);
    WriteFlow(rdURI.toString(), ROUTE_TABLE_ID, rdRouteFlows);

    unordered_set<string> uuids;
    agent.getServiceManager().getAnycastServicesByDomain(rdURI, uuids);
    BOOST_FOREACH (const string& uuid, uuids) {
        anycastServiceUpdated(uuid);
    }
}

void
FlowManager::HandleDomainUpdate(class_id_t cid, const URI& domURI) {
    {
        unique_lock<mutex> guard(queueMutex);
        queuedItems.erase(domURI.toString());
    }

    switch (cid) {
    case RoutingDomain::CLASS_ID:
        HandleRoutingDomainUpdate(domURI);
        break;
    case Subnet::CLASS_ID:
        if (!Subnet::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for Subnet: " << domURI;
            WriteFlow(domURI.toString(), BRIDGE_TABLE_ID, NULL);
        }
        break;
    case BridgeDomain::CLASS_ID:
        if (!BridgeDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for BD: " << domURI;
            WriteFlow(domURI.toString(), BRIDGE_TABLE_ID, NULL);
            idGen.erase(GetIdNamespace(cid), domURI);
        }
        break;
    case FloodDomain::CLASS_ID:
        if (!FloodDomain::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for FD: " << domURI;
            idGen.erase(GetIdNamespace(cid), domURI);
        }
        break;
    case FloodContext::CLASS_ID:
        if (!FloodContext::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for FloodContext: " << domURI;
            removeFromMulticastList(domURI);
        }
        break;
    case L3ExternalNetwork::CLASS_ID:
        if (!L3ExternalNetwork::resolve(agent.getFramework(), domURI)) {
            LOG(DEBUG) << "Cleaning up for L3ExtNet: " << domURI;
            idGen.erase(GetIdNamespace(cid), domURI);
        }
        break;
    }
}

/**
 * Construct a bucket object with the specified bucket ID.
 */
static
ofputil_bucket *CreateBucket(uint32_t bucketId) {
    ofputil_bucket *bkt = (ofputil_bucket *)malloc(sizeof(ofputil_bucket));
    bkt->weight = 0;
    bkt->bucket_id = bucketId;
    bkt->watch_port = OFPP_ANY;
    bkt->watch_group = OFPG11_ANY;
    return bkt;
}

GroupEdit::Entry
FlowManager::CreateGroupMod(uint16_t type, uint32_t groupId,
                            const Ep2PortMap& ep2port, bool onlyPromiscuous) {
    GroupEdit::Entry entry(new GroupEdit::GroupMod());
    entry->mod->command = type;
    entry->mod->group_id = groupId;

    BOOST_FOREACH(const Ep2PortMap::value_type& kv, ep2port) {
        if (onlyPromiscuous && !kv.second.second)
            continue;

        ofputil_bucket *bkt = CreateBucket(kv.second.first);
        ActionBuilder ab;
        ab.SetOutputToPort(kv.second.first);
        ab.Build(bkt);
        list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    uint32_t tunPort = GetTunnelPort();
    if (type != OFPGC11_DELETE && tunPort != OFPP_NONE &&
        encapType != ENCAP_NONE) {
        ofputil_bucket *bkt = CreateBucket(tunPort);
        ActionBuilder ab;
        address tunDst = GetTunnelDst();
        if (mcastTunDst)
            tunDst = mcastTunDst.get();
        SetActionTunnelMetadata(ab, encapType, tunDst);
        ab.SetOutputToPort(tunPort);
        ab.Build(bkt);
        list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    return entry;
}

uint32_t FlowManager::getPromId(uint32_t fgrpId) {
    return ((1<<31) | fgrpId);
}

void
FlowManager::UpdateEndpointFloodGroup(const opflex::modb::URI& fgrpURI,
                                      const Endpoint& endPoint, uint32_t epPort,
                                      bool isPromiscuous,
                                      optional<shared_ptr<FloodDomain> >& fd) {
    const std::string& epUUID = endPoint.getUUID();
    std::pair<uint32_t, bool> epPair(epPort, isPromiscuous);
    uint32_t fgrpId = GetId(FloodDomain::CLASS_ID, fgrpURI);
    string fgrpStrId = fgrpURI.toString();
    FloodGroupMap::iterator fgrpItr = floodGroupMap.find(fgrpURI);

    uint8_t floodMode = UnknownFloodModeEnumT::CONST_DROP;
    if (fd) {
        floodMode =
            fd.get()->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
    }

    if (fgrpItr != floodGroupMap.end()) {
        Ep2PortMap& epMap = fgrpItr->second;
        Ep2PortMap::iterator epItr = epMap.find(epUUID);

        if (epItr == epMap.end()) {
            /* EP not attached to this flood-group, check and remove
             * if it was attached to a different one */
            RemoveEndpointFromFloodGroup(epUUID);
        }
        if (epItr == epMap.end() || epItr->second != epPair) {
            epMap[epUUID] = epPair;
            GroupEdit::Entry e = CreateGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
            WriteGroupMod(e);
            GroupEdit::Entry e2 =
                CreateGroupMod(OFPGC11_MODIFY, getPromId(fgrpId), epMap, true);
            WriteGroupMod(e2);
        }
    } else {
        /* Remove EP attachment to old floodgroup, if any */
        RemoveEndpointFromFloodGroup(epUUID);
        floodGroupMap[fgrpURI][epUUID] = epPair;
        GroupEdit::Entry e =
            CreateGroupMod(OFPGC11_ADD, fgrpId, floodGroupMap[fgrpURI]);
        WriteGroupMod(e);
        GroupEdit::Entry e2 = CreateGroupMod(OFPGC11_ADD, getPromId(fgrpId),
                                             floodGroupMap[fgrpURI], true);
        WriteGroupMod(e2);
    }

    FlowEntryList grpDst;
    FlowEntryList learnDst;

    // deliver broadcast/multicast traffic to the group table
    FlowEntry *e0 = new FlowEntry();
    SetMatchFd(e0, 10, fgrpId, true, BRIDGE_TABLE_ID);
    SetActionFdBroadcast(e0, fgrpId);
    grpDst.push_back(FlowEntryPtr(e0));

    if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
        // go to the learning table on an unknown unicast
        // destination in flood mode
        FlowEntry *unicastLearn = new FlowEntry();
        SetMatchFd(unicastLearn, 5, fgrpId, false, BRIDGE_TABLE_ID);
        SetActionGotoLearn(unicastLearn);
        grpDst.push_back(FlowEntryPtr(unicastLearn));

        // Deliver unknown packets in the flood domain when
        // learning to the controller for reactive flow setup.
        FlowEntry *fdContr = new FlowEntry();
        SetMatchFd(fdContr, 5, fgrpId, false, LEARN_TABLE_ID,
                   NULL, GetProactiveLearnEntryCookie());
        SetActionController(fdContr);
        learnDst.push_back(FlowEntryPtr(fdContr));
    }
    WriteFlow(fgrpStrId, BRIDGE_TABLE_ID, grpDst);
    WriteFlow(fgrpStrId, LEARN_TABLE_ID, learnDst);
}

void FlowManager::RemoveEndpointFromFloodGroup(const std::string& epUUID) {
    for (FloodGroupMap::iterator itr = floodGroupMap.begin();
         itr != floodGroupMap.end();
         ++itr) {
        const URI& fgrpURI = itr->first;
        Ep2PortMap& epMap = itr->second;
        if (epMap.erase(epUUID) == 0) {
            continue;
        }
        uint32_t fgrpId = GetId(FloodDomain::CLASS_ID, fgrpURI);
        uint16_t type = epMap.empty() ?
                OFPGC11_DELETE : OFPGC11_MODIFY;
        GroupEdit::Entry e0 =
                CreateGroupMod(type, fgrpId, epMap);
        GroupEdit::Entry e1 =
                CreateGroupMod(type, getPromId(fgrpId), epMap, true);
        if (epMap.empty()) {
            WriteFlow(fgrpURI.toString(), BRIDGE_TABLE_ID, NULL);
            WriteFlow(fgrpURI.toString(), LEARN_TABLE_ID, NULL);
            floodGroupMap.erase(fgrpURI);
        }
        WriteGroupMod(e0);
        WriteGroupMod(e1);
        break;
    }
}

void FlowManager::HandleContractUpdate(const opflex::modb::URI& contractURI) {
    LOG(DEBUG) << "Updating contract " << contractURI;
    {
        unique_lock<mutex> guard(queueMutex);
        queuedItems.erase(contractURI.toString());
    }

    const string& contractId = contractURI.toString();
    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.contractExists(contractURI)) {  // Contract removed
        WriteFlow(contractId, POL_TABLE_ID, NULL);
        idGen.erase(GetIdNamespace(Contract::CLASS_ID), contractURI);
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
    uint64_t conCookie = GetId(Contract::CLASS_ID, contractURI);

    BOOST_FOREACH(const IdMap::value_type& pid, provIds) {
        const uint32_t& pvnid = pid.first;
        const uint32_t& prdid = pid.second;
        BOOST_FOREACH(const IdMap::value_type& cid, consIds) {
            const uint32_t& cvnid = cid.first;
            const uint32_t& crdid = cid.second;
            uint16_t prio = MAX_POLICY_RULE_PRIORITY;
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
                    AddEntryForClassifier(cls.get(), pc->getAllow(),
                                          prio, conCookie,
                                          cvnid, pvnid, crdid, entryList);
                }
                if (dir == DirectionEnumT::CONST_OUT ||
                    dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
                    AddEntryForClassifier(cls.get(), pc->getAllow(),
                                          prio, conCookie,
                                          pvnid, cvnid, prdid, entryList);
                }
                --prio;
            }
        }
    }
    WriteFlow(contractId, POL_TABLE_ID, entryList);
}

void FlowManager::initPlatformConfig() {

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

void FlowManager::HandleConfigUpdate(const opflex::modb::URI& configURI) {
    LOG(DEBUG) << "Updating platform config " << configURI;
    initPlatformConfig();

    /* Directly update the group-table */
    UpdateGroupTable();
}

static void SetEntryProtocol(FlowEntry *fe, L24Classifier *classifier) {
    using namespace modelgbp::arp;
    using namespace modelgbp::l2;

    match *m = &(fe->entry->match);
    uint8_t arpOpc =
            classifier->getArpOpc(OpcodeEnumT::CONST_UNSPECIFIED);
    uint16_t ethT =
            classifier->getEtherT(EtherTypeEnumT::CONST_UNSPECIFIED);
    if (arpOpc != OpcodeEnumT::CONST_UNSPECIFIED) {
        match_set_nw_proto(m, arpOpc);
    }
    if (ethT != EtherTypeEnumT::CONST_UNSPECIFIED) {
        match_set_dl_type(m, htons(ethT));
    }
    if (classifier->isProtSet()) {
        match_set_nw_proto(m, classifier->getProt().get());
    }
}

static void SetEntryTcpFlags(FlowEntry *fe, uint32_t tcpFlags) {
    using namespace modelgbp::l4;
    ovs_be16 flags = 0;
    if (tcpFlags & TcpFlagsEnumT::CONST_FIN) flags |= 0x01;
    if (tcpFlags & TcpFlagsEnumT::CONST_SYN) flags |= 0x02;
    if (tcpFlags & TcpFlagsEnumT::CONST_RST) flags |= 0x04;
    if (tcpFlags & TcpFlagsEnumT::CONST_ACK) flags |= 0x10;
    match_set_tcp_flags_masked(&fe->entry->match,
                               htons(flags), htons(flags));
}

void FlowManager::AddEntryForClassifier(L24Classifier *clsfr, bool allow,
                                        uint16_t priority, uint64_t cookie,
                                        uint32_t svnid, uint32_t dvnid,
                                        uint32_t srdid,
                                        FlowEntryList& entries) {
    using namespace modelgbp::l4;

    ovs_be64 ckbe = htonll(cookie);
#if 0
    bool reflexive =
        clsfr->isConnectionTrackingSet() &&
        clsfr->getConnectionTracking().get() == ConnTrackEnumT::CONST_REFLEXIVE;
    uint16_t ctZone = (uint16_t)(srdid & 0xffff);
#endif
    MaskList srcPorts;
    MaskList dstPorts;
    RangeMask::getMasks(clsfr->getSFromPort(), clsfr->getSToPort(), srcPorts);
    RangeMask::getMasks(clsfr->getDFromPort(), clsfr->getDToPort(), dstPorts);

    /* Add a "ignore" mask to empty ranges - makes the loop later easy */
    if (srcPorts.empty()) {
        srcPorts.push_back(Mask(0x0, 0x0));
    }
    if (dstPorts.empty()) {
        dstPorts.push_back(Mask(0x0, 0x0));
    }

    vector<uint32_t> tcpFlagsVec;
    uint32_t tcpFlags = clsfr->getTcpFlags(TcpFlagsEnumT::CONST_UNSPECIFIED);
    if (tcpFlags & TcpFlagsEnumT::CONST_ESTABLISHED) {
        tcpFlagsVec.push_back(0 + TcpFlagsEnumT::CONST_ACK);
        tcpFlagsVec.push_back(0 + TcpFlagsEnumT::CONST_RST);
    } else {
        tcpFlagsVec.push_back(tcpFlags);
    }

    BOOST_FOREACH (const Mask& sm, srcPorts) {
        BOOST_FOREACH(const Mask& dm, dstPorts) {
            BOOST_FOREACH(uint32_t flagMask, tcpFlagsVec) {
                FlowEntry *e0 = new FlowEntry();
                e0->entry->cookie = ckbe;

                SetPolicyMatchGroup(e0, priority, svnid, dvnid);
                SetEntryProtocol(e0, clsfr);
                if (tcpFlags != TcpFlagsEnumT::CONST_UNSPECIFIED)
                    SetEntryTcpFlags(e0, flagMask);
                match *m = &(e0->entry->match);
                match_set_tp_src_masked(m, htons(sm.first), htons(sm.second));
                match_set_tp_dst_masked(m, htons(dm.first), htons(dm.second));

                if (allow) {
#if 0
                    if (reflexive) {
                        SetPolicyActionConntrack(e0, ctZone,
                                                 NX_CT_F_COMMIT, true);
                    } else {
#endif
                        SetPolicyActionAllow(e0);
#if 0
                    }
#endif
                }
                entries.push_back(FlowEntryPtr(e0));
            }
        }
    }
#if 0
    if (reflexive && allow) {    /* add the flows for reverse direction */
        FlowEntry *e1 = new FlowEntry();
        e1->entry->cookie = ckbe;
        SetPolicyMatchGroup(e1, priority, dvnid, svnid);
        SetEntryProtocol(e1, clsfr);
        match_set_conn_state_masked(&e1->entry->match, 0x0, 0x80);
        SetPolicyActionConntrack(e1, ctZone, NX_CT_F_RECIRC, false);
        entries.push_back(FlowEntryPtr(e1));

        FlowEntry *e2 = new FlowEntry();
        e2->entry->cookie = ckbe;
        SetPolicyMatchGroup(e2, priority, dvnid, svnid);
        SetEntryProtocol(e2, clsfr);
        match_set_conn_state_masked(&e2->entry->match, 0x82, 0x83);
        SetPolicyActionAllow(e2);
        entries.push_back(FlowEntryPtr(e2));
    }
#endif
}

void FlowManager::UpdateGroupTable() {
    BOOST_FOREACH (FloodGroupMap::value_type& kv, floodGroupMap) {
        const URI& fgrpURI = kv.first;
        uint32_t fgrpId = GetId(FloodDomain::CLASS_ID, fgrpURI);
        Ep2PortMap& epMap = kv.second;

        GroupEdit::Entry e1 = CreateGroupMod(OFPGC11_MODIFY, fgrpId, epMap);
        WriteGroupMod(e1);
        GroupEdit::Entry e2 = CreateGroupMod(OFPGC11_MODIFY,
                                             getPromId(fgrpId), epMap, true);
        WriteGroupMod(e2);
    }
}

void FlowManager::HandlePortStatusUpdate(const string& portName,
                                         uint32_t portNo) {
    LOG(DEBUG) << "Port-status update for " << portName;
    if (portName == encapIface) {
        initPlatformConfig();

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
        UpdateGroupTable();
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

void FlowManager::DiffTableState(int tableId, const FlowEntryList& el,
                                 /* out */ FlowEdit& diffs) {
    const TableState& tab = flowTables[tableId];
    tab.DiffSnapshot(el, diffs);
}

bool
FlowManager::WriteFlow(const string& objId, int tableId,
        FlowEntryList& el) {
    assert(tableId >= 0 && tableId < NUM_FLOW_TABLES);
    TableState& tab = flowTables[tableId];

    FlowEdit diffs;
    bool success = true;
    /*
     * If a sync is in progress, don't write to the flow tables while we are
     * reading and reconciling with the current flows.
     */
    if (!isSyncing) {
        tab.DiffEntry(objId, el, diffs);
        success = executor->Execute(diffs);
    }
    if (success) {
        tab.Update(objId, el);
    } else {
        LOG(ERROR) << "Writing flows for " << objId << " failed";
    }
    el.clear();
    return success;
}

bool
FlowManager::WriteFlow(const string& objId, int tableId, FlowEntry *el) {
    FlowEntryList tmpEl;
    if (el != NULL) {
        tmpEl.push_back(FlowEntryPtr(el));
    }
    return WriteFlow(objId, tableId, tmpEl);
}

bool
FlowManager::WriteGroupMod(const GroupEdit::Entry& e) {
    /*
     * If a sync is in progress, don't write to the group table while we are
     * reading and reconciling with the current groups.
     */
    if (isSyncing) {
        return true;
    }

    GroupEdit ge;
    ge.edits.push_back(e);
    bool success = executor->Execute(ge);
    if (!success) {
        LOG(ERROR) << "Group mod failed for group-id=" << e->mod->group_id;
    }
    return success;
}

void FlowManager::getGroupVnidAndRdId(const unordered_set<URI>& uris,
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
            ids[vnid.get()] = GetId(RoutingDomain::CLASS_ID, rd.get()->getURI());
        }
    }
}

void FlowManager::HandleConnection(SwitchConnection *swConn) {
    if (opflexPeerConnected) {
        LOG(DEBUG) << "Handling new connection to switch";
    } else {
        LOG(INFO) << "Opflex peer not present, ignoring new "
            "connection to switch";
        return;
    }

    if (connectTimer) {
        LOG(DEBUG) << "Sync state with switch will begin in "
            << connectTimer->expires_from_now();
        connectTimer->async_wait(
            bind(&FlowManager::OnConnectTimer, this, error));
    } else {
        OnConnectTimer(boost::system::error_code());
    }
}

void FlowManager::OnConnectTimer(const boost::system::error_code& ec) {
    connectTimer.reset();
    if (!ec)
        flowSyncer.Sync();
}

void FlowManager::OnIdCleanupTimer(const boost::system::error_code& ec) {
    if (ec) return;

    idGen.cleanup();
    for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
        string ns(ID_NAMESPACES[i]);
        IdGenerator::garbage_cb_t gcb =
            bind(ID_NAMESPACE_CB[i], ref(agent.getFramework()), _1, _2);
        agent.getAgentIOService()
            .dispatch(bind(&IdGenerator::collectGarbage, ref(idGen),
                           ns, gcb));
    }

    idCleanupTimer->expires_from_now(milliseconds(3*60*1000));
    idCleanupTimer->async_wait(bind(&FlowManager::OnIdCleanupTimer,
                                    this, error));
}

const char * FlowManager::GetIdNamespace(class_id_t cid) {
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

uint32_t FlowManager::GetId(class_id_t cid, const URI& uri) {
    return idGen.getId(GetIdNamespace(cid), uri);
}

uint32_t FlowManager::getExtNetVnid(const opflex::modb::URI& uri) {
    // External networks are assigned private VNIDs that have bit 31 (MSB)
    // set to 1. This is fine because legal VNIDs are 24-bits or less.
    return (GetId(L3ExternalNetwork::CLASS_ID, uri) | (1 << 31));
}

void FlowManager::updateMulticastList(const optional<string>& mcastIp,
                                      const URI& uri) {
    if ((encapType != ENCAP_VXLAN && encapType != ENCAP_IVXLAN) ||
        GetTunnelPort() == OFPP_NONE ||
        !mcastIp) {
        removeFromMulticastList(uri);
        return;
    }

    boost::system::error_code ec;
    address ip(address::from_string(mcastIp.get(), ec));
    if (ec || !ip.is_v4() || !ip.to_v4().is_multicast()) {
        LOG(WARNING) << "Ignoring invalid/unsupported multicast "
            "subscription IP: " << mcastIp.get();
        return;
    }
    MulticastMap::iterator itr = mcastMap.find(mcastIp.get());
    if (itr != mcastMap.end()) {
        UriSet& uris = itr->second;
        UriSet::iterator jtr = uris.find(uri);
        if (jtr == uris.end()) {
            removeFromMulticastList(uri);   // remove old association, if any
            uris.insert(uri);
        }
    } else {
        removeFromMulticastList(uri);   // remove old association, if any
        mcastMap[mcastIp.get()].insert(uri);
        if (!isSyncing) {
            changeMulticastSubscription(mcastIp.get(), false);
        }
    }
}

void FlowManager::removeFromMulticastList(const URI& uri) {
    BOOST_FOREACH (MulticastMap::value_type& kv, mcastMap) {
        UriSet& uris = kv.second;
        if (uris.erase(uri) > 0 && uris.empty()) {
            if (!isSyncing) {
                changeMulticastSubscription(kv.first, true);
            }
            mcastMap.erase(kv.first);
            break;
        }
    }
}

void FlowManager::changeMulticastSubscription(const string& mcastIp,
                                              bool leave) {
    static const string cmdJoin("dpif/tnl/igmp-join");
    static const string cmdLeave("dpif/tnl/igmp-leave");
    if (!jsonCmdExecutor) {
        return;
    }

    LOG(INFO) << (leave ? "Leav" : "Join") << "ing multicast group with "
            "IP: " << mcastIp;
    vector<string> params;
    params.push_back(connection->getSwitchName());
    params.push_back(tunnelPortStr);   // XXX will be replaced with iface name
    params.push_back(mcastIp);

    const string& cmd = leave ? cmdLeave : cmdJoin;
    string res;
    if (jsonCmdExecutor->execute(cmd, params, res) == false) {
        LOG(ERROR) << "Error " << (leave ? "leav" : "join")
            << "ing multicast group: " << mcastIp << ", error: " << res;
    }
}

void FlowManager::fetchMulticastSubscription(unordered_set<string>& mcastIps) {
    static const string cmdDump("dpif/tnl/igmp-dump");
    if (!jsonCmdExecutor) {
        return;
    }
    vector<string> params;
    params.push_back(connection->getSwitchName());
    params.push_back(tunnelPortStr);   // XXX will be replaced with iface name
    mcastIps.clear();

    string res;
    if (jsonCmdExecutor->execute(cmdDump, params, res) == false) {
        LOG(ERROR) << "Error while fetching multicast subscriptions, "
                "error: " << res;
    } else {
        trim(res);
        if (!res.empty()) {
            split(mcastIps, res, boost::is_any_of(" \t\n\r"),
                  boost::token_compress_on);
        }
    }
}

FlowManager::FlowSyncer::FlowSyncer(FlowManager& fm) :
    flowManager(fm), syncInProgress(false), syncPending(false) {
}

void FlowManager::FlowSyncer::Sync() {
    if (flowManager.reader == NULL) {
        LOG(DEBUG) << "FlowManager doesn't have a flow reader";
        flowManager.isSyncing = false;
        return;
    }

    if (syncInProgress) {
        LOG(DEBUG) << "Sync is already in progress, marking it as pending";
        syncPending = true;
        return;
    }
    syncInProgress = true;
    syncPending = false;
    InitiateSync();
}

void FlowManager::FlowSyncer::InitiateSync() {
    flowManager.isSyncing = true;
    LOG(INFO) << "Syncing flow-manager with switch state, "
        "current mode changed to syncing";

    recvGroups.clear();
    groupsDone = false;
    FlowReader& reader = *(flowManager.reader);
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; ++i) {
        recvFlows[i].clear();
        tableDone[i] = false;
    }

    FlowReader::GroupCb gcb =
        bind(&FlowManager::FlowSyncer::GotGroups, this, _1, _2);
    reader.getGroups(gcb);

    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; ++i) {
        FlowReader::FlowCb cb =
            bind(&FlowManager::FlowSyncer::GotFlows, this, i, _1, _2);
        reader.getFlows(i, cb);
    }
}

void FlowManager::FlowSyncer::GotGroups(const GroupEdit::EntryList& groups,
    bool done) {
    BOOST_FOREACH (const GroupEdit::Entry& e, groups) {
        recvGroups[e->mod->group_id] = e;
    }
    groupsDone = done;
    if (done) {
        LOG(DEBUG) << "Got all groups, #groups=" << recvGroups.size();
        CheckRecvDone();
    }
}

void FlowManager::FlowSyncer::GotFlows(int tableNum,
    const FlowEntryList& flows, bool done) {
    assert(tableNum >= 0 && tableNum < FlowManager::NUM_FLOW_TABLES);

    FlowEntryList& fl = recvFlows[tableNum];
    fl.insert(fl.end(), flows.begin(), flows.end());
    tableDone[tableNum] = done;
    if (done) {
        LOG(DEBUG) << "Got all entries for table=" << tableNum
            << ", #flows=" << fl.size();
        CheckRecvDone();
    }
}

void FlowManager::FlowSyncer::CheckRecvDone() {
    bool allDone = groupsDone;
    for (int i = 0; allDone && i < FlowManager::NUM_FLOW_TABLES; ++i) {
        allDone = allDone && tableDone[i];
    }

    if (allDone) {
        LOG(DEBUG) << "Got all group and flow tables, starting reconciliation";
        flowManager.agent.getAgentIOService()
            .dispatch(bind(&FlowManager::FlowSyncer::CompleteSync, this));
    }
}

void FlowManager::FlowSyncer::CompleteSync() {
    assert(syncInProgress == true);
    ReconcileGroups();
    ReconcileFlows();
    reconcileMulticastList();
    syncInProgress = false;
    flowManager.isSyncing = false;
    LOG(INFO) << "Sync complete, current mode changed to normal execution";

    if (syncPending) {
        flowManager.agent.getAgentIOService()
            .dispatch(bind(&FlowManager::FlowSyncer::Sync, this));
    }

    flowManager.advertManager.start();
}

void FlowManager::FlowSyncer::ReconcileFlows() {
    /* special handling for learning table - reconcile using
       PacketInHandler reactive reconciler */
    FlowEntryList learnFlows;
    recvFlows[FlowManager::LEARN_TABLE_ID].swap(learnFlows);
    BOOST_FOREACH (const FlowEntryPtr& fe, learnFlows) {
        if (!flowManager.pktInHandler.reconcileReactiveFlow(fe)) {
            recvFlows[FlowManager::LEARN_TABLE_ID].push_back(fe);
        }
    }

    FlowEdit diffs[FlowManager::NUM_FLOW_TABLES];
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; ++i) {
        flowManager.flowTables[i].DiffSnapshot(recvFlows[i], diffs[i]);
        LOG(DEBUG) << "Table=" << i << ", snapshot has "
                   << diffs[i].edits.size() << " diff(s)";
        BOOST_FOREACH(const FlowEdit::Entry& e, diffs[i].edits) {
            LOG(DEBUG) << e;
        }
    }

    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; ++i) {
        bool success = flowManager.executor->Execute(diffs[i]);
        if (!success) {
            LOG(ERROR) << "Failed to execute diffs on table=" << i;
        }
        recvFlows[i].clear();
        tableDone[i] = false;
    }
}

void FlowManager::FlowSyncer::CheckGroupEntry(uint32_t groupId,
        const Ep2PortMap& epMap, bool prom, GroupEdit& ge) {
    GroupMap::iterator itr;
    itr = recvGroups.find(groupId);
    uint16_t comm = OFPGC11_ADD;
    GroupEdit::Entry recv;
    if (itr != recvGroups.end()) {
        comm = OFPGC11_MODIFY;
        recv = itr->second;
    }
    GroupEdit::Entry e0 =
        flowManager.CreateGroupMod(comm, groupId, epMap, prom);
    if (!GroupEdit::GroupEq(e0, recv)) {
        ge.edits.push_back(e0);
    }
    if (itr != recvGroups.end()) {
        recvGroups.erase(itr);
    }
}

void FlowManager::FlowSyncer::ReconcileGroups() {
    GroupEdit ge;
    BOOST_FOREACH (FloodGroupMap::value_type& kv, flowManager.floodGroupMap) {
        const URI& fgrpURI = kv.first;
        Ep2PortMap& epMap = kv.second;

        uint32_t fgrpId = flowManager.GetId(FloodDomain::CLASS_ID, fgrpURI);
        CheckGroupEntry(fgrpId, epMap, false, ge);

        uint32_t promFdId = getPromId(fgrpId);
        CheckGroupEntry(promFdId, epMap, true, ge);
    }
    Ep2PortMap tmp;
    BOOST_FOREACH (const GroupMap::value_type& kv, recvGroups) {
        GroupEdit::Entry e0 =
            flowManager.CreateGroupMod(OFPGC11_DELETE, kv.first, tmp);
        ge.edits.push_back(e0);
    }
    bool success = flowManager.executor->Execute(ge);
    if (!success) {
        LOG(ERROR) << "Failed to execute group table changes";
    }
    recvGroups.clear();
    groupsDone = false;
}

void FlowManager::FlowSyncer::reconcileMulticastList() {
    unordered_set<string> mcastIps;
    flowManager.fetchMulticastSubscription(mcastIps);
    LOG(DEBUG) << "Currently subscribed to " << mcastIps.size()
        << " multicast groups";
    BOOST_FOREACH (const MulticastMap::value_type& kv, flowManager.mcastMap) {
        /* Always resubscribe to ensure we join desired multicast groups */
        flowManager.changeMulticastSubscription(kv.first, false);
        mcastIps.erase(kv.first);
    }
    BOOST_FOREACH (const string& ip, mcastIps) {
        flowManager.changeMulticastSubscription(ip, true);
    }
}

} // namespace ovsagent
