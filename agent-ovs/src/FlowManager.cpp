/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
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

#include <netinet/icmp6.h>

#include <modelgbp/arp/OpcodeEnumT.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>
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

using namespace std;
using namespace boost;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::posix_time::seconds;

namespace ovsagent {

static const uint8_t MAC_ADDR_BROADCAST[6] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t MAC_ADDR_MULTICAST[6] =
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t MAC_ADDR_IPV6MULTICAST[6] =
    {0x33, 0x33, 0x00, 0x00, 0x00, 0x01};
static const address_v6 ALL_NODES_IP(address_v6::from_string("ff02::1"));

const uint16_t FlowManager::MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary
const long DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC = 5000;

static const char * ID_NMSPC_FD  = "floodDomain";
static const char * ID_NMSPC_BD  = "bridgeDomain";
static const char * ID_NMSPC_RD  = "routingDomain";
static const char * ID_NMSPC_CON = "contract";

FlowManager::FlowManager(ovsagent::Agent& ag) :
        agent(ag), connection(NULL), executor(NULL), portMapper(NULL),
        reader(NULL), jsonCmdExecutor(NULL), fallbackMode(FALLBACK_PROXY),
        encapType(ENCAP_NONE), floodScope(FLOOD_DOMAIN), tunnelPortStr("4789"),
        virtualRouterEnabled(true), routerAdv(true),
        isSyncing(false), flowSyncer(*this), pktInHandler(agent, *this),
        stopping(false), connectDelayMs(DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC),
        opflexPeerConnected(false) {
    memset(routerMac, 0, sizeof(routerMac));
    memset(dhcpMac, 0, sizeof(dhcpMac));
    tunnelDst = address::from_string("127.0.0.1");
    portMapper = NULL;
}

void FlowManager::Start()
{
    initialAdverts = 0;
    advertTimer.reset(new deadline_timer(agent.getAgentIOService(),
                                         seconds(3)));

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

    workQ.start();

    initPlatformConfig();
}

void FlowManager::Stop()
{
    stopping = true;
    if (connectTimer) {
        connectTimer->cancel();
    }
    if (advertTimer) {
        advertTimer->cancel();
    }
    if (portMapper) {
        portMapper->unregisterPortStatusListener(this);
    }
    workQ.stop();
}

void FlowManager::registerConnection(SwitchConnection *conn) {
    this->connection = conn;
    conn->RegisterOnConnectListener(this);
    conn->RegisterMessageHandler(OFPTYPE_PACKET_IN, &pktInHandler);
}

void FlowManager::unregisterConnection(SwitchConnection *conn) {
    this->connection = NULL;
    conn->UnregisterOnConnectListener(this);
    conn->UnregisterMessageHandler(OFPTYPE_PACKET_IN, &pktInHandler);
}

void FlowManager::registerModbListeners() {
    agent.getEndpointManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
}

void FlowManager::unregisterModbListeners() {
    agent.getEndpointManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);
}

void FlowManager::SetPortMapper(PortMapper *m) {
    portMapper = m;
    portMapper->registerPortStatusListener(this);
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
                   << tunnelRemoteIp << ": " << ec.message();;
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

/** Source table helper functions */
static void
SetSourceAction(FlowEntry *fe, uint32_t epgId,
                uint32_t bdId,  uint32_t fgrpId,  uint32_t l3Id,
                uint8_t nextTable = FlowManager::DST_TABLE_ID,
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
    fe->entry->table_id = FlowManager::DST_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_dst(&fe->entry->match, mac);
    match_set_reg(&fe->entry->match, 4 /* REG4 */, bdId);
}

static inline void fill_in6_addr(struct in6_addr& addr, const address_v6& ip) {
    address_v6::bytes_type bytes = ip.to_bytes();
    std::memcpy(&addr, bytes.data(), bytes.size());
}

static void
SetDestMatchEp(FlowEntry *fe, uint16_t prio, const uint8_t *specialMac,
               const address& ip, uint32_t l3Id) {
    fe->entry->table_id = FlowManager::DST_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_dst(&fe->entry->match, specialMac);
    if (ip.is_v4()) {
        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
        match_set_nw_dst(&fe->entry->match, htonl(ip.to_v4().to_ulong()));
    } else {
        struct in6_addr addr;
        fill_in6_addr(addr, ip.to_v6());

        match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
        match_set_ipv6_dst(&fe->entry->match, &addr);
    }
    match_set_reg(&fe->entry->match, 6 /* REG6 */, l3Id);
}

static void
SetDestMatchArp(FlowEntry *fe, uint16_t prio, const address& ip,
        uint32_t l3Id) {
    fe->entry->table_id = FlowManager::DST_TABLE_ID;
    fe->entry->priority = prio;
    match *m = &fe->entry->match;
    match_set_dl_type(m, htons(ETH_TYPE_ARP));
    match_set_nw_proto(m, ARP_OP_REQUEST);
    match_set_dl_dst(m, MAC_ADDR_BROADCAST);
    match_set_nw_dst(m, htonl(ip.to_v4().to_ulong()));
    match_set_reg(m, 6 /* REG6 */, l3Id);
}

static void
SetDestMatchNd(FlowEntry *fe, uint16_t prio, const address* ip,
               uint32_t l3Id, uint64_t cookie = 0, 
               uint8_t type = ND_NEIGHBOR_SOLICIT) {
    fe->entry->table_id = FlowManager::DST_TABLE_ID;
    if (cookie != 0)
        fe->entry->cookie = cookie;
    fe->entry->priority = prio;
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IPV6));
    match_set_nw_proto(&fe->entry->match, 58 /* ICMP */);
    // OVS uses tp_src for ICMPV6_TYPE
    match_set_tp_src(&fe->entry->match, htons(type));
    // OVS uses tp_dst for ICMPV6_CODE
    match_set_tp_dst(&fe->entry->match, 0);
    match_set_reg(&fe->entry->match, 6 /* REG6 */, l3Id);
    match_set_dl_dst_masked(&fe->entry->match, 
                            MAC_ADDR_MULTICAST, MAC_ADDR_MULTICAST);

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
        match_set_dl_dst_masked(m, MAC_ADDR_MULTICAST, MAC_ADDR_MULTICAST);
    if (dstMac)
        match_set_dl_dst(m, dstMac);
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
SetDestActionEpMac(FlowEntry *fe, FlowManager::EncapType type, bool isLocal, 
                   uint32_t epgId, uint32_t port, const address& tunDst) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    if (!isLocal) {
        FlowManager::SetActionTunnelMetadata(ab, type, tunDst);
    }
    ab.SetGotoTable(FlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionEp(FlowEntry *fe, FlowManager::EncapType type, bool isLocal,  
                uint32_t epgId, uint32_t port,
                const address& tunDst, const uint8_t *specialMac, 
                const uint8_t *dstMac) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    if (!isLocal) {
        FlowManager::SetActionTunnelMetadata(ab, type, tunDst);
    }
    ab.SetEthSrcDst(specialMac, dstMac);
    ab.SetDecNwTtl();
    ab.SetGotoTable(FlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionEpArp(FlowEntry *fe, FlowManager::EncapType type, bool isLocal, 
                   uint32_t epgId, uint32_t port, const address& tunDst, 
                   const uint8_t *dstMac) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    if (!isLocal) {
        FlowManager::SetActionTunnelMetadata(ab, type, tunDst);
    }
    ab.SetEthSrcDst(NULL, dstMac);
    ab.SetGotoTable(FlowManager::POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionSubnetArp(FlowEntry *fe, const uint8_t *specialMac,
                       const address& gwIp) {
    ActionBuilder ab;
    ab.SetRegMove(MFF_ETH_SRC, MFF_ETH_DST);
    ab.SetRegLoad(MFF_ETH_SRC, specialMac);
    ab.SetRegLoad16(MFF_ARP_OP, ARP_OP_REPLY);
    ab.SetRegMove(MFF_ARP_SHA, MFF_ARP_THA);
    ab.SetRegLoad(MFF_ARP_SHA, specialMac);
    ab.SetRegMove(MFF_ARP_SPA, MFF_ARP_TPA);
    ab.SetRegLoad(MFF_ARP_SPA, gwIp.to_v4().to_ulong());
    ab.SetOutputToPort(OFPP_IN_PORT);

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
                    uint16_t max_len = 0xffff) {
    ActionBuilder ab;
    if (epgId != 0)
        ab.SetRegLoad(MFF_REG0, epgId);
    ab.SetController(max_len);
    ab.Build(fe->entry);
}

/** Policy table */
static void
SetPolicyActionAllow(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetOutputReg(MFF_REG7);
    ab.Build(fe->entry);
}

static void
SetPolicyActionConntrack(FlowEntry *fe, uint16_t zone, uint16_t flags,
                         bool output) {
    ActionBuilder ab;
    ab.SetConntrack(zone, flags);
    if (output) {
        ab.SetOutputReg(MFF_REG7);
    }
    ab.Build(fe->entry);
}

static void
SetPolicyMatchEpgs(FlowEntry *fe, uint16_t prio,
        uint32_t sEpgId, uint32_t dEpgId) {
    fe->entry->table_id = FlowManager::POL_TABLE_ID;
    fe->entry->priority = prio;
    match_set_reg(&fe->entry->match, 0 /* REG0 */, sEpgId);
    match_set_reg(&fe->entry->match, 2 /* REG2 */, dEpgId);
}

/** Security table */

static void
SetSecurityMatchEpMac(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
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
    match_set_in_port(&fe->entry->match, port);
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
        match_set_ipv6_src(&fe->entry->match, &addr);

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
SetSecurityActionAllow(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetGotoTable(FlowManager::SRC_TABLE_ID);
    ab.Build(fe->entry);
}

void FlowManager::endpointUpdated(const std::string& uuid) {
    if (stopping) return;
    WorkItem w = bind(&FlowManager::HandleEndpointUpdate, this, uuid);
    workQ.enqueue(w);
}

void FlowManager::egDomainUpdated(const opflex::modb::URI& egURI) {
    if (stopping) return;
    WorkItem w = bind(&FlowManager::HandleEndpointGroupDomainUpdate, this,
                      egURI);
    workQ.enqueue(w);
}

void FlowManager::domainUpdated(class_id_t cid, const URI& domURI) {
    if (stopping) return;
    WorkItem w = bind(&FlowManager::HandleDomainUpdate, this, cid, domURI);
    workQ.enqueue(w);
}

void FlowManager::contractUpdated(const opflex::modb::URI& contractURI) {
    if (stopping) return;
    WorkItem w = bind(&FlowManager::HandleContractUpdate, this, contractURI);
    workQ.enqueue(w);
}

void FlowManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    PeerConnected();
    WorkItem w = bind(&FlowManager::HandleConfigUpdate, this, configURI);
    workQ.enqueue(w);
}

void FlowManager::Connected(SwitchConnection *swConn) {
    if (stopping) return;
    WorkItem w = bind(&FlowManager::HandleConnection, this, swConn);
    workQ.enqueue(w);
}

void FlowManager::portStatusUpdate(const string& portName, uint32_t portNo) {
    if (stopping) return;
    WorkItem w = bind(&FlowManager::HandlePortStatusUpdate, this,
                      portName, portNo);
    workQ.enqueue(w);
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

            WorkItem w = bind(&FlowManager::HandleConnection, this,
                              connection);
            workQ.enqueue(w);
        }
    }
}

bool
FlowManager::GetGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
                                    optional<URI>& rdURI, uint32_t& rdId, 
                                    uint32_t& bdId,
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

    rdId = 0;
    bdId = epgBd ?
            GetId(BridgeDomain::CLASS_ID, epgBd.get()->getURI()) : 0;
    fgrpId = 0;
    if (epgFd) {    // FD present -> flooding is desired
        if (floodScope == ENDPOINT_GROUP) {
            fgrpURI = epgURI;
        } else  {
            fgrpURI = epgFd.get()->getURI();
        }
        fgrpId = GetId(FloodDomain::CLASS_ID, fgrpURI.get());
    }
    if (epgRd) {
        rdURI = epgRd.get()->getURI();
        rdId = GetId(RoutingDomain::CLASS_ID, rdURI.get());
    }
    return true;
}

void
FlowManager::HandleEndpointUpdate(const string& uuid) {

    LOG(DEBUG) << "Updating endpoint " << uuid;

    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        WriteFlow(uuid, SEC_TABLE_ID, NULL);
        WriteFlow(uuid, SRC_TABLE_ID, NULL);
        WriteFlow(uuid, DST_TABLE_ID, NULL);
        RemoveEndpointFromFloodGroup(uuid);
        return;
    }
    const Endpoint& endPoint = *epWrapper.get();
    uint8_t macAddr[6];
    bool hasMac = endPoint.getMAC() != boost::none;
    if (hasMac)
        endPoint.getMAC().get().toUIntArray(macAddr);

    /* check and parse the IP-addresses */
    std::vector<address> ipAddresses;
    boost::system::error_code ec;
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
    bool isLocalEp = (ofPort != OFPP_NONE);

    /* Port security flows */
    if (isLocalEp) {
        FlowEntryList el;

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
                    SetSecurityMatchEpND(e2, 40, ofPort, macAddr, &ipAddr, false);
                    SetSecurityActionAllow(e2);
                    el.push_back(FlowEntryPtr(e2));
                }
            }
        }
        WriteFlow(uuid, SEC_TABLE_ID, el);
    }

    optional<URI> epgURI = epMgr.getComputedEPG(uuid);
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, rdURI;
    if (!GetGroupForwardingInfo(epgURI.get(), epgVnid, rdURI, rdId,
                                bdId, fgrpURI, fgrpId)) {
        return;
    }

    /**
     * Irrespective of flooding scope (epg vs. flood-domain), the
     * properties of the flood-domain object decide how flooding is
     * done.
     */
    optional<shared_ptr<FloodDomain> > fd = 
        agent.getPolicyManager().getFDForGroup(epgURI.get());
    uint8_t arpMode = AddressResModeEnumT::CONST_UNICAST;
    if (fd && fd.get()->isArpModeSet()) {
        arpMode = fd.get()->getArpMode().get();
    }
    uint8_t ndMode = AddressResModeEnumT::CONST_UNICAST;
    if (fd && fd.get()->isNeighborDiscModeSet()) {
        ndMode = fd.get()->getNeighborDiscMode().get();
    }

    /* Source Table flows; applicable only to local endpoints */
    if (isLocalEp) {
        FlowEntryList src;
        if (hasMac) {
            FlowEntry *e0 = new FlowEntry();
            SetSourceMatchEp(e0, 140, ofPort, macAddr);
            SetSourceAction(e0, epgVnid, bdId, fgrpId, rdId);
            src.push_back(FlowEntryPtr(e0));
        }

        if (endPoint.isPromiscuousMode()) {
            // if the source is unknown, but the interface is
            // promiscuous we allow the traffic into the learning
            // table
            FlowEntry *e1 = new FlowEntry();
            SetSourceMatchEp(e1, 138, ofPort, NULL);
            SetSourceAction(e1, epgVnid, bdId, fgrpId, rdId, LEARN_TABLE_ID);
            src.push_back(FlowEntryPtr(e1));

            // Multicast traffic from promiscuous ports is delivered
            // normally
            FlowEntry *e2 = new FlowEntry();
            SetSourceMatchEp(e2, 139, ofPort, NULL);
            match_set_dl_dst_masked(&e2->entry->match, 
                                    MAC_ADDR_BROADCAST,
                                    MAC_ADDR_MULTICAST);
            SetSourceAction(e1, epgVnid, bdId, fgrpId, rdId);
            src.push_back(FlowEntryPtr(e2));
        }

        if (virtualDHCPEnabled && hasMac) {
            optional<Endpoint::DHCPv4Config> v4c = endPoint.getDHCPv4Config();
            optional<Endpoint::DHCPv6Config> v6c = endPoint.getDHCPv6Config();

            if (v4c) {
                FlowEntry* dhcp = new FlowEntry();
                SetSourceMatchEp(dhcp, 150, ofPort, macAddr);
                SetMatchDHCP(dhcp, FlowManager::SRC_TABLE_ID, 150, true,
                             GetDHCPCookie(true));
                SetActionController(dhcp, epgVnid, 0xffff);
                src.push_back(FlowEntryPtr(dhcp));
            }
            if (v6c) {
                FlowEntry* dhcp = new FlowEntry();
                SetSourceMatchEp(dhcp, 150, ofPort, macAddr);
                SetMatchDHCP(dhcp, FlowManager::SRC_TABLE_ID, 150, false,
                             GetDHCPCookie(false));
                SetActionController(dhcp, epgVnid, 0xffff);
                src.push_back(FlowEntryPtr(dhcp));
            }
        }

        WriteFlow(uuid, SRC_TABLE_ID, src);
    }

    FlowEntryList elDst;
    uint32_t epPort = isLocalEp ? ofPort : GetTunnelPort();

    if (bdId != 0 && hasMac && epPort != OFPP_NONE) {
        FlowEntry *e0 = new FlowEntry();
        SetDestMatchEpMac(e0, 10, macAddr, bdId);
        SetDestActionEpMac(e0, encapType, isLocalEp, epgVnid, epPort,
                           GetTunnelDst());
        elDst.push_back(FlowEntryPtr(e0));
    }

    if (rdId != 0 && epPort != OFPP_NONE) {
        BOOST_FOREACH (const address& ipAddr, ipAddresses) {
            // XXX for remote EP, dl_dst needs to be the "next-hop" MAC
            if (virtualRouterEnabled) {
                const uint8_t *dstMac = isLocalEp ?
                    (hasMac ? macAddr : NULL) : NULL;
                FlowEntry *e0 = new FlowEntry();
                SetDestMatchEp(e0, 15, GetRouterMacAddr(), ipAddr, rdId);
                SetDestActionEp(e0, encapType, isLocalEp, epgVnid, epPort,
                                GetTunnelDst(), GetRouterMacAddr(),
                                dstMac);
                elDst.push_back(FlowEntryPtr(e0));
            }

            if (arpMode != AddressResModeEnumT::CONST_FLOOD && ipAddr.is_v4()) {
                FlowEntry *e1 = new FlowEntry();
                SetDestMatchArp(e1, 20, ipAddr, rdId);
                if (arpMode == AddressResModeEnumT::CONST_UNICAST) {
                    // ARP optimization: broadcast -> unicast
                    SetDestActionEpArp(e1, encapType, isLocalEp, epgVnid,
                                       epPort, GetTunnelDst(), 
                                       hasMac ? macAddr : NULL);
                }
                // else drop the ARP packet
                elDst.push_back(FlowEntryPtr(e1));
            }

            if (ndMode != AddressResModeEnumT::CONST_FLOOD && ipAddr.is_v6()) {
                FlowEntry *e1 = new FlowEntry();
                SetDestMatchNd(e1, 20, &ipAddr, rdId);
                if (ndMode == AddressResModeEnumT::CONST_UNICAST) {
                    // neighbor discovery optimization: broadcast -> unicast
                    SetDestActionEpArp(e1, encapType, isLocalEp, epgVnid,
                                       epPort, GetTunnelDst(), 
                                       hasMac ? macAddr : NULL);
                }
                // else drop the ND packet
                elDst.push_back(FlowEntryPtr(e1));
            }
        }
    }
    WriteFlow(uuid, DST_TABLE_ID, elDst);

    if (fgrpURI && isLocalEp) {
        UpdateEndpointFloodGroup(fgrpURI.get(), endPoint, ofPort,
                                 endPoint.isPromiscuousMode(),
                                 fd);
    } else {
        RemoveEndpointFromFloodGroup(uuid);
    }
}

void
FlowManager::HandleEndpointGroupDomainUpdate(const URI& epgURI) {
    LOG(DEBUG) << "Updating endpoint-group " << epgURI;

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

    FlowEntry *unknownTunnel = NULL;
    if (fallbackMode == FALLBACK_PROXY && tunPort != OFPP_NONE && 
        encapType != ENCAP_NONE) {
        // Output to the tunnel interface, bypassing policy
        // note that if the flood domain is set to flood unknown, then
        // there will be a higher-priority rule installed for that
        // flood domain.
        unknownTunnel = new FlowEntry();
        unknownTunnel->entry->priority = 1;
        unknownTunnel->entry->table_id = DST_TABLE_ID;
        SetDestActionOutputToTunnel(unknownTunnel, encapType,
                                    GetTunnelDst(), tunPort);
    }
    WriteFlow("static", DST_TABLE_ID, unknownTunnel);

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {  // EPG removed
        WriteFlow(epgId, SRC_TABLE_ID, NULL);
        WriteFlow(epgId, POL_TABLE_ID, NULL);
        updateMulticastList(boost::none, epgURI);
        return;
    }

    uint32_t epgVnid, rdId, bdId, fgrpId;
    optional<URI> fgrpURI, rdURI;
    if (!GetGroupForwardingInfo(epgURI, epgVnid, rdURI, 
                                rdId, bdId, fgrpURI, fgrpId)) {
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

        uint8_t nextTable = FlowManager::DST_TABLE_ID;
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
                                    MAC_ADDR_BROADCAST,
                                    MAC_ADDR_MULTICAST);
            SetSourceAction(e1, epgVnid, bdId, fgrpId, rdId,
                            FlowManager::DST_TABLE_ID, encapType);
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
        SetPolicyMatchEpgs(e0, 100, epgVnid, epgVnid);
        SetPolicyActionAllow(e0);
        WriteFlow(epgId, POL_TABLE_ID, e0);
    } else {
        WriteFlow(epgId, POL_TABLE_ID, NULL);
    }

    if (rdId != 0) {
        UpdateGroupSubnets(epgURI, rdId);

        if (virtualRouterEnabled && routerAdv) {
            FlowEntry *e0 = new FlowEntry();
            SetDestMatchNd(e0, 20, NULL, rdId, FlowManager::GetNDCookie(),
                           ND_ROUTER_SOLICIT);
            SetActionController(e0);
            WriteFlow(rdURI.get().toString(), DST_TABLE_ID, e0);

            if (!isSyncing) {
                initialAdverts = 0;
                advertTimer->expires_from_now(seconds(1));
                advertTimer->async_wait(bind(&FlowManager::OnAdvertTimer, this, error));
            }
        }
    }

    unordered_set<string> epUuids;
    agent.getEndpointManager().getEndpointsForGroup(epgURI, epUuids);
    BOOST_FOREACH(const string& ep, epUuids) {
        endpointUpdated(ep);
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
FlowManager::UpdateGroupSubnets(const URI& egURI, uint32_t routingDomainId) {
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(egURI, subnets);

    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets) {
        FlowEntryList el;

        optional<const string&> networkAddrStr = sn->getAddress();
        bool hasRouterIp = false;
        boost::system::error_code ec;
        if (virtualRouterEnabled && networkAddrStr) {
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
                    SetDestMatchArp(e0, 20, routerIp, routingDomainId);
                    SetDestActionSubnetArp(e0, GetRouterMacAddr(), routerIp);
                    el.push_back(FlowEntryPtr(e0));
                } else {
                    FlowEntry *e0 = new FlowEntry();
                    SetDestMatchNd(e0, 20, &routerIp, routingDomainId,
                                   FlowManager::GetNDCookie());
                    SetActionController(e0);
                    el.push_back(FlowEntryPtr(e0));
                }
            }
        }
        WriteFlow(sn->getURI().toString(), DST_TABLE_ID, el);
    }
}

void
FlowManager::HandleDomainUpdate(class_id_t cid, const URI& domURI) {
    switch (cid) {
    case RoutingDomain::CLASS_ID:
        if (!RoutingDomain::resolve(agent.getFramework(), domURI)) {
            LOG(INFO) << "Cleaning up for RD: " << domURI;
            WriteFlow(domURI.toString(), DST_TABLE_ID, NULL);
            idGen.erase(GetIdNamespace(RoutingDomain::CLASS_ID), domURI);
        }
        break;
    case Subnet::CLASS_ID:
        if (!Subnet::resolve(agent.getFramework(), domURI)) {
            LOG(INFO) << "Cleaning up for Subnet: " << domURI;
            WriteFlow(domURI.toString(), DST_TABLE_ID, NULL);
        }
        break;
    case BridgeDomain::CLASS_ID:
        if (!BridgeDomain::resolve(agent.getFramework(), domURI)) {
            LOG(INFO) << "Cleaning up for BD: " << domURI;
            idGen.erase(GetIdNamespace(cid), domURI);
        }
        break;
    case FloodDomain::CLASS_ID:
        if (!FloodDomain::resolve(agent.getFramework(), domURI)) {
            LOG(INFO) << "Cleaning up for FD: " << domURI;
            idGen.erase(GetIdNamespace(cid), domURI);
        }
        break;
    case FloodContext::CLASS_ID:
        if (!FloodContext::resolve(agent.getFramework(), domURI)) {
            LOG(INFO) << "Cleaning up for FloodContext: " << domURI;
            removeFromMulticastList(domURI);
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
    bkt->weight = 1;
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
    FlowEntryList epLearn;

    // deliver broadcast/multicast traffic to the group table
    FlowEntry *e0 = new FlowEntry();
    SetMatchFd(e0, 10, fgrpId, true, DST_TABLE_ID);
    SetActionFdBroadcast(e0, fgrpId);
    grpDst.push_back(FlowEntryPtr(e0));

    if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
        // go to the learning table on an unknown unicast
        // destination in flood mode
        FlowEntry *unicastLearn = new FlowEntry();
        SetMatchFd(unicastLearn, 5, fgrpId, false, DST_TABLE_ID);
        SetActionGotoLearn(unicastLearn);
        grpDst.push_back(FlowEntryPtr(unicastLearn));

        // Deliver unknown packets in the flood domain when
        // learning to the controller for reactive flow setup.
        FlowEntry *fdContr = new FlowEntry();
        SetMatchFd(fdContr, 5, fgrpId, false, LEARN_TABLE_ID,
                   NULL, GetProactiveLearnEntryCookie());
        SetActionController(fdContr);
        WriteFlow(fgrpStrId, LEARN_TABLE_ID, fdContr);

        // Prepopulate a stage1 learning entry for known EPs
        if (endPoint.getMAC() && endPoint.getInterfaceName()) {
            if (epPort != OFPP_NONE) {
                uint8_t macAddr[6];
                endPoint.getMAC().get().toUIntArray(macAddr);

                FlowEntry* learnEntry = new FlowEntry();
                SetMatchFd(learnEntry, 101, fgrpId, false, LEARN_TABLE_ID,
                           macAddr, GetProactiveLearnEntryCookie());
                ActionBuilder ab;
                ab.SetRegLoad(MFF_REG7, epPort);
                ab.SetOutputToPort(epPort);
                ab.SetController();
                ab.Build(learnEntry->entry);
                epLearn.push_back(FlowEntryPtr(learnEntry));
            }
        }
    }
    WriteFlow(fgrpStrId, DST_TABLE_ID, grpDst);
    WriteFlow(epUUID, LEARN_TABLE_ID, epLearn);
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
            WriteFlow(fgrpURI.toString(), DST_TABLE_ID, NULL);
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

    const string& contractId = contractURI.toString();
    uint64_t conCookie = GetId(Contract::CLASS_ID, contractURI);

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
    getEpgVnidAndRdId(provURIs, provIds);
    getEpgVnidAndRdId(consURIs, consIds);

    PolicyManager::rule_list_t rules;
    polMgr.getContractRules(contractURI, rules);

    LOG(INFO) << "Update for contract " << contractURI
            << ", #prov=" << provIds.size()
            << ", #cons=" << consIds.size()
            << ", #rules=" << rules.size();

    FlowEntryList entryList;

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

void FlowManager::AddEntryForClassifier(L24Classifier *clsfr, bool allow,
                                        uint16_t priority, uint64_t cookie,
                                        uint32_t svnid, uint32_t dvnid,
                                        uint32_t srdid,
                                        FlowEntryList& entries) {
    ovs_be64 ckbe = htonll(cookie);
    bool reflexive =
        clsfr->isConnectionTrackingSet() &&
        clsfr->getConnectionTracking().get() == ConnTrackEnumT::CONST_REFLEXIVE;
    uint16_t ctZone = (uint16_t)(srdid & 0xffff);
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

    BOOST_FOREACH (const Mask& sm, srcPorts) {
        BOOST_FOREACH(const Mask& dm, dstPorts) {
            FlowEntry *e0 = new FlowEntry();
            e0->entry->cookie = ckbe;

            SetPolicyMatchEpgs(e0, priority, svnid, dvnid);
            SetEntryProtocol(e0, clsfr);
            match *m = &(e0->entry->match);
            match_set_tp_src_masked(m, htons(sm.first), htons(sm.second));
            match_set_tp_dst_masked(m, htons(dm.first), htons(dm.second));

            if (allow) {
                if (reflexive) {
                    SetPolicyActionConntrack(e0, ctZone, NX_CT_F_COMMIT, true);
                } else {
                    SetPolicyActionAllow(e0);
                }
            }
            entries.push_back(FlowEntryPtr(e0));
        }
    }

    if (reflexive && allow) {    /* add the flows for reverse direction */
        FlowEntry *e1 = new FlowEntry();
        e1->entry->cookie = ckbe;
        SetPolicyMatchEpgs(e1, priority, dvnid, svnid);
        SetEntryProtocol(e1, clsfr);
        match_set_conn_state_masked(&e1->entry->match, 0x0, 0x80);
        SetPolicyActionConntrack(e1, ctZone, NX_CT_F_RECIRC, false);
        entries.push_back(FlowEntryPtr(e1));

        FlowEntry *e2 = new FlowEntry();
        e2->entry->cookie = ckbe;
        SetPolicyMatchEpgs(e2, priority, dvnid, svnid);
        SetEntryProtocol(e2, clsfr);
        match_set_conn_state_masked(&e2->entry->match, 0x82, 0x83);
        SetPolicyActionAllow(e2);
        entries.push_back(FlowEntryPtr(e2));
    }
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
            HandleEndpointGroupDomainUpdate(epg);
        }
        /* Directly update the group-table */
        UpdateGroupTable();
    } else {
        unordered_set<string> epUUIDs;
        agent.getEndpointManager().getEndpointsByIface(portName, epUUIDs);
        BOOST_FOREACH (const string& ep, epUUIDs) {
            HandleEndpointUpdate(ep);
        }
    }
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

void FlowManager::getEpgVnidAndRdId(const unordered_set<URI>& egURIs,
    /* out */unordered_map<uint32_t, uint32_t>& egids) {
    PolicyManager& pm = agent.getPolicyManager();
    BOOST_FOREACH(const URI& u, egURIs) {
        optional<uint32_t> vnid = pm.getVnidForGroup(u);
        if (vnid) {
            optional<shared_ptr<RoutingDomain> > rd = pm.getRDForGroup(u);
            egids[vnid.get()] =
                rd ? GetId(RoutingDomain::CLASS_ID, rd.get()->getURI()) : 0;
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

void FlowManager::OnConnectTimer(const system::error_code& ec) {
    connectTimer.reset();
    if (!ec)
        flowSyncer.Sync();
}

void FlowManager::OnAdvertTimer(const system::error_code& ec) {
    if (ec)
        return;
    if (!virtualRouterEnabled || !routerAdv)
        return;
    if (!connection)
        return;

    int nextInterval = 16;

    if (connection->IsConnected()) {
        EndpointManager& epMgr = agent.getEndpointManager();
        PolicyManager& polMgr = agent.getPolicyManager();

        int minInterval = 200;
        PolicyManager::uri_set_t epgURIs;
        polMgr.getGroups(epgURIs);

        BOOST_FOREACH(const URI& epg, epgURIs) {
            unordered_set<string> eps;
            unordered_set<uint32_t> out_ports;
            epMgr.getEndpointsForGroup(epg, eps);
            BOOST_FOREACH(const string& uuid, eps) {
                shared_ptr<const Endpoint> ep = epMgr.getEndpoint(uuid);
                if (!ep) continue;

                const boost::optional<std::string>& iface = ep->getInterfaceName();
                if (!iface) continue;

                uint32_t port = portMapper->FindPort(iface.get());
                if (port != OFPP_NONE)
                    out_ports.insert(port);
            }

            if (out_ports.size() == 0) continue;

            address_v6::bytes_type bytes = ALL_NODES_IP.to_bytes();
            struct ofpbuf* b = packets::compose_icmp6_router_ad(routerMac,
                                                                MAC_ADDR_IPV6MULTICAST,
                                                                (struct in6_addr*)bytes.data(),
                                                                epg, polMgr);
            if (b == NULL) continue;

            struct ofputil_packet_out po;
            po.buffer_id = UINT32_MAX;
            po.packet = ofpbuf_data(b);
            po.packet_len = ofpbuf_size(b);
            po.in_port = OFPP_CONTROLLER;

            ActionBuilder ab;
            BOOST_FOREACH(uint32_t p, out_ports) {
                ab.SetOutputToPort(p);
            }
            ab.Build(&po);

            ofputil_protocol proto = 
                ofputil_protocol_from_ofp_version(connection->GetProtocolVersion());
            assert(ofputil_protocol_is_valid(proto));
            struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
            int error = connection->SendMessage(message);
            free(po.ofpacts);
            if (error) {
                LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
            } else {
                LOG(DEBUG) << "Sent router advertisement for " << epg;
            }
            ofpbuf_delete(b);
        }

        if (initialAdverts >= 3)
            nextInterval = minInterval;
        else 
            initialAdverts += 1;
    } else {
        initialAdverts = 0;
    }

    if (!stopping) {
        advertTimer->expires_from_now(seconds(nextInterval));
        advertTimer->async_wait(bind(&FlowManager::OnAdvertTimer, this, error));
    }
}

const char * FlowManager::GetIdNamespace(class_id_t cid) {
    const char *nmspc = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   nmspc = ID_NMSPC_RD; break;
    case BridgeDomain::CLASS_ID:    nmspc = ID_NMSPC_BD; break;
    case FloodDomain::CLASS_ID:     nmspc = ID_NMSPC_FD; break;
    case Contract::CLASS_ID:        nmspc = ID_NMSPC_CON; break;
    default:
        assert(false);
    }
    return nmspc;
}

uint32_t FlowManager::GetId(class_id_t cid, const URI& uri) {
    return idGen.getId(GetIdNamespace(cid), uri);
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
        algorithm::trim(res);
        if (!res.empty()) {
            algorithm::split(mcastIps, res, boost::is_any_of(" \t\n\r"),
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
        WorkItem w = bind(&FlowManager::FlowSyncer::CompleteSync, this);
        flowManager.workQ.enqueue(w);
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
        WorkItem w = bind(&FlowManager::FlowSyncer::Sync, this);
        flowManager.workQ.enqueue(w);
    }

    flowManager.OnAdvertTimer(boost::system::error_code());
}

void FlowManager::FlowSyncer::ReconcileFlows() {
    /* special handling for learning table - ignore entries learnt reactively */
    FlowEntryList learnFlows;
    recvFlows[FlowManager::LEARN_TABLE_ID].swap(learnFlows);
    ovs_be64 learnCookie = FlowManager::GetLearnEntryCookie();
    BOOST_FOREACH (const FlowEntryPtr& fe, learnFlows) {
        if (fe->entry->cookie != learnCookie) {
            recvFlows[FlowManager::LEARN_TABLE_ID].push_back(fe);
        }
    }

    FlowEdit diffs[FlowManager::NUM_FLOW_TABLES];
    for (int i = 0; i < FlowManager::NUM_FLOW_TABLES; ++i) {
        flowManager.flowTables[i].DiffSnapshot(recvFlows[i], diffs[i]);
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
