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
#include <boost/system/error_code.hpp>
#include <boost/foreach.hpp>

#include <modelgbp/arp/OpcodeEnumT.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/AddressResModeEnumT.hpp>

#include "logging.h"
#include "Endpoint.h"
#include "EndpointManager.h"
#include "EndpointListener.h"
#include "SwitchConnection.h"
#include "FlowManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "Packets.h"

using namespace std;
using namespace boost;
using namespace opflex::modb;
using namespace ovsagent;
using namespace opflex::enforcer::flow;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using boost::asio::deadline_timer;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;

namespace opflex {
namespace enforcer {

static const uint8_t MAC_ADDR_BROADCAST[6] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t MAC_ADDR_MULTICAST[6] =
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint16_t FlowManager::MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary
const long DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC = 5000;

static const char * ID_NMSPC_FD  = "floodDomain";
static const char * ID_NMSPC_BD  = "bridgeDomain";
static const char * ID_NMSPC_RD  = "routingDomain";
static const char * ID_NMSPC_CON = "contract";

FlowManager::FlowManager(ovsagent::Agent& ag) :
        agent(ag), connection(NULL), executor(NULL), portMapper(NULL),
        reader(NULL), fallbackMode(FALLBACK_PROXY), encapType(ENCAP_NONE),
        virtualRouterEnabled(true), isSyncing(false), flowSyncer(*this),
        connectDelayMs(DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC),
        opflexPeerConnected(false) {
    memset(routerMac, 0, sizeof(routerMac));
    tunnelDst = address::from_string("127.0.0.1");
    portMapper = NULL;
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

    workQ.start();
}

void FlowManager::Stop()
{
    if (portMapper) {
        portMapper->unregisterPortStatusListener(this);
    }
    workQ.stop();
}

void FlowManager::registerConnection(SwitchConnection *conn) {
    this->connection = conn;
    conn->RegisterOnConnectListener(this);
    conn->RegisterMessageHandler(OFPTYPE_PACKET_IN, this);
}

void FlowManager::unregisterConnection(SwitchConnection *conn) {
    this->connection = NULL;
    conn->UnregisterOnConnectListener(this);
    conn->UnregisterMessageHandler(OFPTYPE_PACKET_IN, this);
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

void FlowManager::SetVirtualRouter(bool virtualRouterEnabled) {
    this->virtualRouterEnabled = virtualRouterEnabled;
}

void FlowManager::SetVirtualRouterMac(const string& virtualRouterMac) {
    try {
        MAC(virtualRouterMac).toUIntArray(routerMac);
    } catch (std::invalid_argument) {
        LOG(ERROR) << "Invalid virtual router MAC: " << virtualRouterMac;
    }
}

void FlowManager::SetSyncDelayOnConnect(long delay) {
    connectDelayMs = delay;
}

void FlowManager::SetFlowIdCache(const std::string& flowIdCache) {
    this->flowIdCache = flowIdCache;
}

ovs_be64 FlowManager::GetLearnEntryCookie() {
    return htonll((uint64_t)1 << 63 | 1);
}

ovs_be64 FlowManager::GetNDCookie() {
    return htonll((uint64_t)1 << 63 | 2);
}

/** Source table helper functions */
static void
SetSourceAction(FlowEntry *fe, uint32_t epgId,
                uint32_t bdId,  uint32_t fdId,  uint32_t l3Id,
                uint8_t nextTable = FlowManager::DST_TABLE_ID)
{
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG0, epgId);
    ab.SetRegLoad(MFF_REG4, bdId);
    ab.SetRegLoad(MFF_REG5, fdId);
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
        match_set_vlan_vid(&fe->entry->match, htons(0xfff & epgId));
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
SetMatchFd(FlowEntry *fe, uint16_t prio, uint32_t fdId, bool broadcast, 
           uint8_t tableId, uint8_t* dstMac = NULL) {
    fe->entry->table_id = tableId;
    fe->entry->priority = prio;
    match *m = &fe->entry->match;
    match_set_reg(&fe->entry->match, 5 /* REG5 */, fdId);
    if (broadcast)
        match_set_dl_dst_masked(m, MAC_ADDR_MULTICAST, MAC_ADDR_MULTICAST);
    if (dstMac)
        match_set_dl_dst(m, dstMac);
}

static void
SetActionTunnelMetadata(ActionBuilder& ab, FlowManager::EncapType type, 
                        const address& tunDst) {
    switch (type) {
    case FlowManager::ENCAP_VLAN:
        ab.SetPushVlan();
        ab.SetRegMove(MFF_REG0, MFF_VLAN_VID);
        break;
    case FlowManager::ENCAP_VXLAN:
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        if (tunDst.is_v4()) {
            ab.SetRegLoad(MFF_TUN_DST, htonl(tunDst.to_v4().to_ulong()));
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
        SetActionTunnelMetadata(ab, type, tunDst);
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
        SetActionTunnelMetadata(ab, type, tunDst);
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
        SetActionTunnelMetadata(ab, type, tunDst);
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
SetDestActionFdBroadcast(FlowEntry *fe, uint32_t fdId) {
    ActionBuilder ab;
    ab.SetGroup(fdId);
    ab.Build(fe->entry);
}

static void
SetDestActionOutputToTunnel(FlowEntry *fe, FlowManager::EncapType type,
                            const address& tunDst, uint32_t tunPort) {
    ActionBuilder ab;
    SetActionTunnelMetadata(ab, type, tunDst);
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
SetActionController(FlowEntry *fe, uint16_t max_len = 128) {
    ActionBuilder ab;
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
SetSecurityMatchEpDHCP(FlowEntry *fe, uint16_t prio, bool v4) {
    fe->entry->table_id = FlowManager::SEC_TABLE_ID;
    fe->entry->priority = prio;
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
    WorkItem w = bind(&FlowManager::HandleEndpointUpdate, this, uuid);
    workQ.enqueue(w);
}

void FlowManager::egDomainUpdated(const opflex::modb::URI& egURI) {
    PeerConnected();
    WorkItem w = bind(&FlowManager::HandleEndpointGroupDomainUpdate, this,
                      egURI);
    workQ.enqueue(w);
}

void FlowManager::contractUpdated(const opflex::modb::URI& contractURI) {
    WorkItem w = bind(&FlowManager::HandleContractUpdate, this, contractURI);
    workQ.enqueue(w);
}

void FlowManager::Connected(SwitchConnection *swConn) {
    WorkItem w = bind(&FlowManager::HandleConnection, this, swConn);
    workQ.enqueue(w);
}

void FlowManager::portStatusUpdate(const string& portName, uint32_t portNo) {
    WorkItem w = bind(&FlowManager::HandlePortStatusUpdate, this,
                      portName, portNo);
    workQ.enqueue(w);
}

void FlowManager::PeerConnected() {
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

    rdId = 0;
    bdId = epgBd ?
            GetId(BridgeDomain::CLASS_ID, epgBd.get()->getURI()) : 0;
    fdId = 0;
    if (epgFd) {
        fdURI = epgFd.get()->getURI();
        fdId = GetId(FloodDomain::CLASS_ID, fdURI.get());
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
        RemoveEndpointFromFloodDomain(uuid);
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

    optional<URI> epgURI = endPoint.getEgURI();
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    uint32_t epgVnid, rdId, bdId, fdId;
    optional<URI> fdURI, rdURI;
    if (!GetGroupForwardingInfo(epgURI.get(), epgVnid, rdURI, rdId,
                                bdId, fdURI, fdId)) {
        return;
    }

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
            SetSourceAction(e0, epgVnid, bdId, fdId, rdId);
            src.push_back(FlowEntryPtr(e0));
        }

        if (endPoint.isPromiscuousMode()) {
            // if the source is unknown, but the interface is
            // promiscuous we allow the traffic into the learning
            // table
            FlowEntry *e1 = new FlowEntry();
            SetSourceMatchEp(e1, 139, ofPort, NULL);
            SetSourceAction(e1, epgVnid, bdId, fdId, rdId, LEARN_TABLE_ID);
            src.push_back(FlowEntryPtr(e1));
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

    if (fdURI && isLocalEp) {
        UpdateEndpointFloodDomain(fdURI.get(), endPoint, ofPort,
                                  endPoint.isPromiscuousMode(),
                                  fd);
    } else {
        RemoveEndpointFromFloodDomain(uuid);
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
        SetSecurityMatchEpDHCP(allowDHCP, 27, true);
        SetSecurityActionAllow(allowDHCP);
        fixedFlows.push_back(FlowEntryPtr(allowDHCP));

        // Allow IPv6 autoconfiguration (DHCP & router solicitiation)
        // requests but not replies
        FlowEntry *allowDHCPv6 = new FlowEntry();
        SetSecurityMatchEpDHCP(allowDHCPv6, 27, false);
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
        return;
    }

    uint32_t epgVnid, rdId, bdId, fdId;
    optional<URI> fdURI, rdURI;
    if (!GetGroupForwardingInfo(epgURI, epgVnid, rdURI, 
                                rdId, bdId, fdURI, fdId)) {
        return;
    }

    if (tunPort != OFPP_NONE && encapType != ENCAP_NONE) {
        // Assign the source registers based on the VNID from the
        // tunnel uplink
        FlowEntry *e0 = new FlowEntry();
        SetSourceMatchEpg(e0, encapType, 150, tunPort, epgVnid);
        SetSourceAction(e0, epgVnid, bdId, fdId, rdId);
        WriteFlow(epgId, SRC_TABLE_ID, e0);
    } else {
        WriteFlow(epgId, SRC_TABLE_ID, NULL);
    }

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

        // XXX this leaks flows for RDs that are not tied to any epgs
        FlowEntry *e0 = new FlowEntry();
        SetDestMatchNd(e0, 20, NULL, rdId, FlowManager::GetNDCookie(),
                       ND_ROUTER_SOLICIT);
        SetActionController(e0, 0xffff);
        WriteFlow(rdURI.get().toString(), DST_TABLE_ID, e0);
    }

    unordered_set<string> epUuids;
    agent.getEndpointManager().getEndpointsForGroup(epgURI, epUuids);
    BOOST_FOREACH(const string& ep, epUuids) {
        endpointUpdated(ep);
    }
}

void
FlowManager::UpdateGroupSubnets(const URI& egURI, uint32_t routingDomainId) {
    PolicyManager::subnet_vector_t subnets;
    agent.getPolicyManager().getSubnetsForGroup(egURI, subnets);

    // XXX this leaks subnet-related flows when no group is using a subnet
    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets) {
        FlowEntryList el;
        optional<const string&> routerIpStr = sn->getVirtualRouterIp();
        if (routerIpStr) {
            boost::system::error_code ec;
            address routerIp = address::from_string(routerIpStr.get(), ec);
            if (ec) {
                LOG(WARNING) << "Invalid router IP: "
                             << routerIpStr << ": " << ec.message();
                continue;
            } else if (virtualRouterEnabled) {
                if (routerIp.is_v4()) {
                    FlowEntry *e0 = new FlowEntry();
                    SetDestMatchArp(e0, 20, routerIp, routingDomainId);
                    SetDestActionSubnetArp(e0, GetRouterMacAddr(), routerIp);
                    el.push_back(FlowEntryPtr(e0));
                } else {
                    FlowEntry *e0 = new FlowEntry();
                    SetDestMatchNd(e0, 20, &routerIp, routingDomainId, 
                                   FlowManager::GetNDCookie());
                    SetActionController(e0, 0xffff);
                    el.push_back(FlowEntryPtr(e0));
                }
            }
        }
        WriteFlow(sn->getURI().toString(), DST_TABLE_ID, el);
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
        SetActionTunnelMetadata(ab, encapType, GetTunnelDst());
        ab.SetOutputToPort(tunPort);
        ab.Build(bkt);
        list_push_back(&entry->mod->buckets, &bkt->list_node);
    }
    return entry;
}

static uint32_t getPromId(uint32_t fdId) {
    return ((1<<31) | fdId);
}

void
FlowManager::UpdateEndpointFloodDomain(const opflex::modb::URI& fdURI,
                                       const Endpoint& endPoint, uint32_t epPort,
                                       bool isPromiscuous, 
                                       optional<shared_ptr<FloodDomain> >& fd) {
    const std::string& epUUID = endPoint.getUUID();
    std::pair<uint32_t, bool> epPair(epPort, isPromiscuous);
    uint32_t fdId = GetId(FloodDomain::CLASS_ID, fdURI);
    string fdStrId = fdURI.toString();
    FdMap::iterator fdItr = fdMap.find(fdURI);

    uint8_t floodMode = UnknownFloodModeEnumT::CONST_DROP;
    if (fd) {
        floodMode = 
            fd.get()->getUnknownFloodMode(UnknownFloodModeEnumT::CONST_DROP);
    }

    optional<URI> oldFdURI;
    if (fdItr != fdMap.end()) {
        Ep2PortMap& epMap = fdItr->second;
        Ep2PortMap::iterator epItr = epMap.find(epUUID);

        if (epItr == epMap.end()) {
            /* EP not attached to this FD, check/remove if it was attached
             * to a different one */
            RemoveEndpointFromFloodDomain(epUUID);
        }
        if (epItr == epMap.end() || epItr->second != epPair) {
            epMap[epUUID] = epPair;
            GroupEdit::Entry e = CreateGroupMod(OFPGC11_MODIFY, fdId, epMap);
            WriteGroupMod(e);
            GroupEdit::Entry e2 = CreateGroupMod(OFPGC11_MODIFY, getPromId(fdId), 
                                                 epMap, true);
            WriteGroupMod(e2);
        }
    } else {
        fdMap[fdURI][epUUID] = epPair;
        GroupEdit::Entry e = CreateGroupMod(OFPGC11_ADD, fdId, fdMap[fdURI]);
        WriteGroupMod(e);
        GroupEdit::Entry e2 = CreateGroupMod(OFPGC11_ADD, getPromId(fdId), 
                                             fdMap[fdURI], true);
        WriteGroupMod(e2);

    }

    FlowEntryList entryList;
    FlowEntryList epLearn;
    // deliver broadcast/multicast traffic to the group table
    FlowEntry *e0 = new FlowEntry();
    SetMatchFd(e0, 10, fdId, true, DST_TABLE_ID);
    SetDestActionFdBroadcast(e0, fdId);
    entryList.push_back(FlowEntryPtr(e0));

    if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
        // go to the learning table on an unknown unicast
        // destination in flood mode
        FlowEntry *unicastLearn = new FlowEntry();
        SetMatchFd(unicastLearn, 5, fdId, false, DST_TABLE_ID);
        SetActionGotoLearn(unicastLearn);
        entryList.push_back(FlowEntryPtr(unicastLearn));

        // Deliver unknown packets in the flood domain when
        // learning to the controller for reactive flow setup.
        FlowEntry *fdContr = new FlowEntry();
        SetMatchFd(fdContr, 5, fdId, false, LEARN_TABLE_ID);
        SetActionController(fdContr);
        WriteFlow(fdStrId, LEARN_TABLE_ID, fdContr);

        // Prepopulate a stage1 learning entry for known EPs
        if (endPoint.getMAC() && endPoint.getInterfaceName()) {
            if (epPort != OFPP_NONE) {
                uint8_t macAddr[6];
                endPoint.getMAC().get().toUIntArray(macAddr);

                FlowEntry* learnEntry = new FlowEntry();
                SetMatchFd(learnEntry, 101, fdId, false, LEARN_TABLE_ID,
                           macAddr);
                ActionBuilder ab;
                ab.SetOutputToPort(epPort);
                ab.SetController();
                ab.Build(learnEntry->entry);
                epLearn.push_back(FlowEntryPtr(learnEntry));
            }
        }
    }
    WriteFlow(fdStrId, DST_TABLE_ID, entryList);
    WriteFlow(epUUID, LEARN_TABLE_ID, epLearn);
}

void FlowManager::RemoveEndpointFromFloodDomain(const std::string& epUUID) {
    for (FdMap::iterator itr = fdMap.begin(); itr != fdMap.end(); ++itr) {
        const URI& fdURI = itr->first;
        Ep2PortMap& epMap = itr->second;
        if (epMap.erase(epUUID) == 0) {
            continue;
        }
        uint32_t fdId = GetId(FloodDomain::CLASS_ID, fdURI);
        uint16_t type = epMap.empty() ?
                OFPGC11_DELETE : OFPGC11_MODIFY;
        GroupEdit::Entry e0 =
                CreateGroupMod(type, fdId, epMap);
        GroupEdit::Entry e1 =
                CreateGroupMod(type, getPromId(fdId), epMap, true);
        if (epMap.empty()) {
            WriteFlow(fdURI.toString(), DST_TABLE_ID, NULL);
            WriteFlow(fdURI.toString(), LEARN_TABLE_ID, NULL);
            fdMap.erase(fdURI);
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

    unordered_set<uint32_t> provVnids;
    unordered_set<uint32_t> consVnids;
    GetGroupVnids(provURIs, provVnids);
    GetGroupVnids(consURIs, consVnids);

    PolicyManager::rule_list_t rules;
    polMgr.getContractRules(contractURI, rules);

    LOG(INFO) << "Update for contract " << contractURI
            << ", #prov=" << provVnids.size()
            << ", #cons=" << consVnids.size()
            << ", #rules=" << rules.size();

    FlowEntryList entryList;
    BOOST_FOREACH(uint32_t pvnid, provVnids) {
        BOOST_FOREACH(uint32_t cvnid, consVnids) {
            uint16_t prio = MAX_POLICY_RULE_PRIORITY;
            BOOST_FOREACH(shared_ptr<L24Classifier>& cls, rules) {
                uint8_t dir =
                        cls->getDirection(DirectionEnumT::CONST_BIDIRECTIONAL);
                if (dir == DirectionEnumT::CONST_IN ||
                    dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
                    AddEntryForClassifier(cls.get(), prio, conCookie,
                            cvnid, pvnid, entryList);
                }
                if (dir == DirectionEnumT::CONST_OUT ||
                    dir == DirectionEnumT::CONST_BIDIRECTIONAL) {
                    AddEntryForClassifier(cls.get(), prio, conCookie,
                            pvnid, cvnid, entryList);
                }
                --prio;
            }
        }
    }
    WriteFlow(contractId, POL_TABLE_ID, entryList);
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

void FlowManager::AddEntryForClassifier(L24Classifier *clsfr,
        uint16_t priority, uint64_t cookie, uint32_t& svnid, uint32_t& dvnid,
        flow::FlowEntryList& entries) {
    ovs_be64 ckbe = htonll(cookie);
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

            SetPolicyActionAllow(e0);
            entries.push_back(FlowEntryPtr(e0));
        }
    }
}

void FlowManager::HandlePortStatusUpdate(const string& portName,
                                         uint32_t portNo) {
    LOG(DEBUG) << "Port-status update for " << portName;
    if (portName == encapIface) {
        PolicyManager::uri_set_t epgURIs;
        agent.getPolicyManager().getGroups(epgURIs);
        BOOST_FOREACH (const URI& epg, epgURIs) {
            HandleEndpointGroupDomainUpdate(epg);
        }
        /* Directly update the group-table */
        BOOST_FOREACH (FdMap::value_type& kv, fdMap) {
            const URI& fdURI = kv.first;
            Ep2PortMap& epMap = kv.second;

            uint32_t fdId = GetId(FloodDomain::CLASS_ID, fdURI);
            GroupEdit::Entry e1 = CreateGroupMod(OFPGC11_MODIFY, fdId, epMap);
            WriteGroupMod(e1);
            GroupEdit::Entry e2 = CreateGroupMod(OFPGC11_MODIFY,
                getPromId(fdId), epMap, true);
            WriteGroupMod(e2);
        }
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

void FlowManager::GetGroupVnids(const unordered_set<URI>& egURIs,
    /* out */unordered_set<uint32_t>& egVnids) {
    PolicyManager& pm = agent.getPolicyManager();
    BOOST_FOREACH(const URI& u, egURIs) {
        optional<uint32_t> vnid = pm.getVnidForGroup(u);
        if (vnid) {
            egVnids.insert(vnid.get());
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
    flowSyncer.Sync();
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

static bool writeLearnFlow(SwitchConnection *conn, 
                           ofputil_protocol& proto,
                           struct ofputil_packet_in& pi,
                           struct flow& flow,
                           bool stage2) {
    bool dstKnown = (0 != pi.fmd.regs[7]);
    if (stage2 && !dstKnown) return false;

    struct ofputil_flow_mod fm;
    memset(&fm, 0, sizeof(fm));
    fm.buffer_id = UINT32_MAX;
    fm.table_id = FlowManager::LEARN_TABLE_ID;
    fm.priority = stage2 ? 150 : 100;
    fm.idle_timeout = 300;
    fm.command = OFPFC_ADD;
    fm.new_cookie = FlowManager::GetLearnEntryCookie();

    match_set_reg(&fm.match, 5 /* REG5 */, pi.fmd.regs[5]);
    if (stage2) {
        match_set_dl_dst(&fm.match, flow.dl_dst);
        match_set_dl_src(&fm.match, flow.dl_src);
    } else {
        match_set_dl_dst(&fm.match, flow.dl_src);
    }

    ActionBuilder ab;
    // Set destination epg == source epg
    ab.SetRegLoad(MFF_REG2, pi.fmd.regs[0]);
    // Set the output register
    ab.SetRegLoad(MFF_REG7, stage2 ? pi.fmd.regs[7] : pi.fmd.in_port);
    if (stage2) {
        ab.SetGotoTable(FlowManager::POL_TABLE_ID);
    } else {
        ab.SetOutputToPort(stage2 ? pi.fmd.regs[7] : pi.fmd.in_port);
        ab.SetController();
    }
    ab.Build(&fm);

    struct ofpbuf * message = ofputil_encode_flow_mod(&fm, proto);
    int error = conn->SendMessage(message);
    free(fm.ofpacts);
    if (error) {
        LOG(ERROR) << "Could not write flow mod: " << ovs_strerror(error);
    }
    return true;
}

static void handleLearnPktIn(SwitchConnection *conn,
                             struct ofputil_packet_in& pi,
                             ofputil_protocol& proto,
                             struct ofpbuf& pkt,
                             struct flow& flow) {
    writeLearnFlow(conn, proto, pi, flow, false);
    if (writeLearnFlow(conn, proto, pi, flow, true))
        return;

    {
        // install a forward flow to flood to the promiscuous ports in
        // the flood domain until we learn the correct reverse path
        struct ofputil_flow_mod fm;
        memset(&fm, 0, sizeof(fm));
        fm.buffer_id = pi.buffer_id;
        fm.table_id = FlowManager::LEARN_TABLE_ID;
        fm.priority = 50;
        fm.idle_timeout = 5;
        fm.hard_timeout = 60;
        fm.command = OFPFC_ADD;
        fm.new_cookie = FlowManager::GetLearnEntryCookie();

        match_set_in_port(&fm.match, pi.fmd.in_port);
        match_set_reg(&fm.match, 5 /* REG5 */, pi.fmd.regs[5]);
        match_set_dl_src(&fm.match, flow.dl_src);

        ActionBuilder ab;
        ab.SetGroup(getPromId(pi.fmd.regs[5]));
        ab.Build(&fm);

        struct ofpbuf* message = ofputil_encode_flow_mod(&fm, proto);
        int error = conn->SendMessage(message);
        free(fm.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write flow mod: " << ovs_strerror(error);
        }
    }

    if (pi.buffer_id == UINT32_MAX) {
        // Send packet out if needed
        struct ofputil_packet_out po;
        po.buffer_id = UINT32_MAX;
        po.packet = ofpbuf_data(&pkt);
        po.packet_len = ofpbuf_size(&pkt);
        po.in_port = pi.fmd.in_port;

        ActionBuilder ab;
        ab.SetGroup(getPromId(pi.fmd.regs[5]));
        ab.Build(&po);

        struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
        int error = conn->SendMessage(message);
        free(po.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
        }
    }
}

static void handleNDPktIn(Agent& agent,
                          SwitchConnection *conn,
                          struct ofputil_packet_in& pi,
                          ofputil_protocol& proto,
                          struct ofpbuf& pkt,
                          struct flow& flow,
                          const uint8_t* virtualRouterMac) {
    uint32_t epgId = (uint32_t)pi.fmd.regs[0];
    PolicyManager& polMgr = agent.getPolicyManager();
    optional<URI> egUri = polMgr.getGroupForVnid(epgId);
    if (!egUri)
        return;

    if (flow.dl_type != htons(ETH_TYPE_IPV6) ||
        flow.nw_proto != 58 /* ICMPv6 */)
        return;

    size_t l4_size = ofpbuf_l4_size(&pkt);
    if (l4_size < sizeof(struct icmp6_hdr))
        return;
    struct icmp6_hdr* icmp = (struct icmp6_hdr*) ofpbuf_l4(&pkt);
    if (icmp->icmp6_code != 0)
        return;

    struct ofpbuf* b = NULL;

    if (icmp->icmp6_type == ND_NEIGHBOR_SOLICIT) {
        /* Neighbor solicitation */
        struct nd_neighbor_solicit* neigh_sol = 
            (struct nd_neighbor_solicit*) ofpbuf_l4(&pkt);
        if (l4_size < sizeof(*neigh_sol))
            return;

        // we only get neighbor solicitation packets for our router
        // IP, so we just always reply with our router MAC to the
        // requested IP.
        b = Packets::compose_icmp6_neigh_ad(ND_NA_FLAG_ROUTER | 
                                            ND_NA_FLAG_SOLICITED | 
                                            ND_NA_FLAG_OVERRIDE,
                                            virtualRouterMac,
                                            flow.dl_src,
                                            &neigh_sol->nd_ns_target,
                                            &flow.ipv6_src);

    } else if (icmp->icmp6_type == ND_ROUTER_SOLICIT) {
        LOG(INFO) << "Router solicit";
        /* Router solicitation */
        struct nd_router_solicit* router_sol = 
            (struct nd_router_solicit*) ofpbuf_l4(&pkt);
        if (l4_size < sizeof(*router_sol))
            return;

        b = Packets::compose_icmp6_router_ad(virtualRouterMac,
                                             flow.dl_src,
                                             &flow.ipv6_src,
                                             egUri.get(),
                                             polMgr);
    }

    if (b) {
        // send reply as packet-out
        struct ofputil_packet_out po;
        po.buffer_id = UINT32_MAX;
        po.packet = ofpbuf_data(b);
        po.packet_len = ofpbuf_size(b);
        po.in_port = pi.fmd.in_port;
        
        ActionBuilder ab;
        ab.SetOutputToPort(OFPP_IN_PORT);
        ab.Build(&po);
    
        struct ofpbuf* message = ofputil_encode_packet_out(&po, proto);
        int error = conn->SendMessage(message);
        free(po.ofpacts);
        if (error) {
            LOG(ERROR) << "Could not write packet-out: " << ovs_strerror(error);
        }
        
        ofpbuf_delete(b);
    }
}

/**
 * Perform MAC learning for flood domains that require flooding for
 * unknown unicast traffic.  Note that this will only manage the
 * reactive flows associated with MAC learning; there are static flows
 * to enabling MAC learning elsewhere.
 */
void FlowManager::Handle(SwitchConnection *conn, 
                         ofptype msgType, ofpbuf *msg) {
    assert(msgType == OFPTYPE_PACKET_IN);

    const struct ofp_header *oh = (ofp_header *)ofpbuf_data(msg);
    struct ofputil_packet_in pi;

    enum ofperr err = ofputil_decode_packet_in(&pi, oh);
    if (err) {
        LOG(ERROR) << "Failed to decode packet-in: " << ovs_strerror(err);
        return;
    }

    if (pi.reason != OFPR_ACTION)
        return;

    struct ofpbuf pkt;
    struct flow flow;
    ofpbuf_use_const(&pkt, pi.packet, pi.packet_len);
    flow_extract(&pkt, NULL, &flow);

    ofputil_protocol proto = 
        ofputil_protocol_from_ofp_version(conn->GetProtocolVersion());
    assert(ofputil_protocol_is_valid(proto));

    if (pi.cookie == GetLearnEntryCookie())
        handleLearnPktIn(conn, pi, proto, pkt, flow);
    else if (pi.cookie == GetNDCookie())
        handleNDPktIn(agent, conn, pi, proto, pkt, flow, routerMac);

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
    syncInProgress = false;
    flowManager.isSyncing = false;
    LOG(INFO) << "Sync complete, current mode changed to normal execution";

    if (syncPending) {
        WorkItem w = bind(&FlowManager::FlowSyncer::Sync, this);
        flowManager.workQ.enqueue(w);
    }
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
    BOOST_FOREACH (FdMap::value_type& kv, flowManager.fdMap) {
        const URI& fdURI = kv.first;
        Ep2PortMap& epMap = kv.second;

        uint32_t fdId = flowManager.GetId(FloodDomain::CLASS_ID, fdURI);
        CheckGroupEntry(fdId, epMap, false, ge);

        uint32_t promFdId = getPromId(fdId);
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

}   // namespace enforcer
}   // namespace opflex
