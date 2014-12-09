/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <netinet/ip.h>

#include <string>
#include <cstdlib>
#include <cstdio>
#include <boost/foreach.hpp>

#include <modelgbp/arp/OpcodeEnumT.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/IntraGroupPolicyEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>

#include "logging.h"
#include "Endpoint.h"
#include "EndpointManager.h"
#include "EndpointListener.h"
#include "ovs.h"
#include "FlowManager.h"
#include "TableState.h"
#include "ActionBuilder.h"

using namespace std;
using namespace boost;
using namespace opflex::modb;
using namespace ovsagent;
using namespace opflex::enforcer::flow;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;

namespace opflex {
namespace enforcer {


static const uint8_t SEC_TABLE_ID = 0,
                     SRC_TABLE_ID = 1,
                     DST_TABLE_ID = 2,
                     LEARN_TABLE_ID = 3,
                     POL_TABLE_ID = 4;

static const uint8_t MAC_ADDR_BROADCAST[6] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t MAC_ADDR_MULTICAST[6] =
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint16_t FlowManager::MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary

FlowManager::FlowManager(ovsagent::Agent& ag) :
    agent(ag), virtualRouterEnabled(true) {
    memset(routerMac, 0, sizeof(routerMac));
    ParseIpv4Addr("127.0.0.1", &tunnelDstIpv4);
    portMapper = NULL;
}

void FlowManager::Start()
{
    agent.getEndpointManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
    connection->RegisterMessageHandler(OFPTYPE_PACKET_IN, this);
}

void FlowManager::Stop()
{
    agent.getEndpointManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);
    connection->UnregisterMessageHandler(OFPTYPE_PACKET_IN, this);

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
    uint32_t ip;
    if (ParseIpv4Addr(tunnelRemoteIp, &ip)) {
        tunnelDstIpv4 = ip;
    } else {
        LOG(ERROR) << "Ignoring bad tunnel destination IP: " << tunnelRemoteIp;
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

/** Source table helper functions */
static void
SetSourceAction(FlowEntry *fe, uint32_t epgId,
                uint32_t bdId,  uint32_t fdId,  uint32_t l3Id,
                uint8_t nextTable = DST_TABLE_ID)
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
    fe->entry->table_id = SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, ofPort);
    if (mac)
        match_set_dl_src(&fe->entry->match, mac);
}

static void
SetSourceMatchEpg(FlowEntry *fe, uint16_t prio, uint32_t tunPort,
        uint32_t epgId) {
    fe->entry->table_id = SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, tunPort);
    match_set_tun_id(&fe->entry->match, htonll(epgId));
}

/** Destination table helper functions */
static void
SetDestMatchEpMac(FlowEntry *fe, uint16_t prio, const uint8_t *mac,
        uint32_t bdId) {
    fe->entry->table_id = DST_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_dst(&fe->entry->match, mac);
    match_set_reg(&fe->entry->match, 4 /* REG4 */, bdId);
}

static void
SetDestMatchEpIpv4(FlowEntry *fe, uint16_t prio, const uint8_t *specialMac,
        uint32_t ipv4, uint32_t l3Id) {
    fe->entry->table_id = DST_TABLE_ID;
    fe->entry->priority = prio;
    match_set_dl_dst(&fe->entry->match, specialMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
    match_set_nw_dst(&fe->entry->match, ipv4);
    match_set_reg(&fe->entry->match, 6 /* REG6 */, l3Id);
}

static void
SetDestMatchArpIpv4(FlowEntry *fe, uint16_t prio, uint32_t ipv4,
        uint32_t l3Id) {
    fe->entry->table_id = DST_TABLE_ID;
    fe->entry->priority = prio;
    match *m = &fe->entry->match;
    match_set_dl_type(m, htons(ETH_TYPE_ARP));
    match_set_nw_proto(m, ARP_OP_REQUEST);
    match_set_dl_dst(m, MAC_ADDR_BROADCAST);
    match_set_nw_dst(m, ipv4);
    match_set_reg(&fe->entry->match, 6 /* REG6 */, l3Id);
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
SetDestActionEpMac(FlowEntry *fe, bool isLocal, uint32_t epgId,
        uint32_t port, uint32_t tunDst) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    if (!isLocal) {
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        ab.SetRegLoad(MFF_TUN_DST, tunDst);
    }
    ab.SetGotoTable(POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionEpIpv4(FlowEntry *fe, bool isLocal, uint32_t epgId, uint32_t port,
        uint32_t tunDst, const uint8_t *specialMac, const uint8_t *dstMac) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    if (!isLocal) {
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        ab.SetRegLoad(MFF_TUN_DST, tunDst);
    }
    ab.SetEthSrcDst(specialMac, dstMac);
    ab.SetDecNwTtl();
    ab.SetGotoTable(POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionEpArpIpv4(FlowEntry *fe, bool isLocal, uint32_t epgId,
        uint32_t port, uint32_t tunDst, const uint8_t *dstMac) {
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG2, epgId);
    ab.SetRegLoad(MFF_REG7, port);
    if (!isLocal) {
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        ab.SetRegLoad(MFF_TUN_DST, tunDst);
    }
    ab.SetEthSrcDst(NULL, dstMac);
    ab.SetGotoTable(POL_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetDestActionSubnetArpIpv4(FlowEntry *fe, const uint8_t *specialMac,
        uint32_t gwIpv4) {
    ActionBuilder ab;
    ab.SetRegMove(MFF_ETH_SRC, MFF_ETH_DST);
    ab.SetRegLoad(MFF_ETH_SRC, specialMac);
    ab.SetRegLoad(MFF_ARP_OP, ARP_OP_REPLY);
    ab.SetRegMove(MFF_ARP_SHA, MFF_ARP_THA);
    ab.SetRegLoad(MFF_ARP_SHA, specialMac);
    ab.SetRegMove(MFF_ARP_SPA, MFF_ARP_TPA);
    ab.SetRegLoad(MFF_ARP_SPA, gwIpv4);
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
SetActionGotoLearn(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetGotoTable(LEARN_TABLE_ID);
    ab.Build(fe->entry);
}

static void
SetActionController(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetController();
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
    fe->entry->table_id = POL_TABLE_ID;
    fe->entry->priority = prio;
    match_set_reg(&fe->entry->match, 0 /* REG0 */, sEpgId);
    match_set_reg(&fe->entry->match, 2 /* REG2 */, dEpgId);
}

/** Security table */

static void
SetSecurityMatchEpMac(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac) {
    fe->entry->table_id = SEC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
}

static void
SetSecurityMatchEpIpv4(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac, uint32_t epIpv4) {
    fe->entry->table_id = SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
    if (epIpv4 != 0)
        match_set_nw_src(&fe->entry->match, epIpv4);
}

static void
SetSecurityMatchEpArp(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac, uint32_t epIpv4) {
    fe->entry->table_id = SEC_TABLE_ID;
    fe->entry->priority = prio;
    if (port != OFPP_ANY)
        match_set_in_port(&fe->entry->match, port);
    if (epMac)
        match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_ARP));
    if (epIpv4 != 0)
        match_set_nw_src(&fe->entry->match, epIpv4);
}

static void
SetSecurityActionAllow(FlowEntry *fe) {
    ActionBuilder ab;
    ab.SetGotoTable(SRC_TABLE_ID);
    ab.Build(fe->entry);
}

bool
FlowManager::GetGroupForwardingInfo(const URI& epgURI, uint32_t& vnid,
        uint32_t& rdId, uint32_t& bdId, optional<URI>& fdURI, uint32_t& fdId) {
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

    rdId = epgRd ?
            GetId(RoutingDomain::CLASS_ID, epgRd.get()->getURI()) : 0;
    bdId = epgBd ?
            GetId(BridgeDomain::CLASS_ID, epgBd.get()->getURI()) : 0;
    fdId = 0;
    if (epgFd) {
        fdURI = epgFd.get()->getURI();
        fdId = GetId(FloodDomain::CLASS_ID, fdURI.get());
    }
    return true;
}

void
FlowManager::endpointUpdated(const string& uuid) {
    EndpointManager& epMgr = agent.getEndpointManager();
    shared_ptr<const Endpoint> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        WriteFlow(uuid, portSecurityTable, NULL);
        WriteFlow(uuid, sourceTable, NULL);
        WriteFlow(uuid, destinationTable, NULL);
        RemoveEndpointFromFloodDomain(uuid);
        return;
    }
    const Endpoint& endPoint = *epWrapper.get();
    uint8_t macAddr[6];
    if (endPoint.getMAC())
        endPoint.getMAC().get().toUIntArray(macAddr);

    /* check and parse the IP-addresses */
    std::vector<uint32_t> ipv4Addresses;
    std::vector<in6_addr> ipv6Addresses;
    BOOST_FOREACH(const string& ipStr, endPoint.getIPs()) {
        uint32_t ipv4;
        in6_addr ipv6;
        if (ParseIpv4Addr(ipStr, &ipv4)) {
            ipv4Addresses.push_back(ipv4);
        } else if (ParseIpv6Addr(ipStr, &ipv6)) {
            ipv6Addresses.push_back(ipv6);
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
            el.push_back(e0);
        } else {
            FlowEntry *e0 = new FlowEntry();
            SetSecurityMatchEpMac(e0, 20, ofPort, macAddr);
            SetSecurityActionAllow(e0);
            el.push_back(e0);

            BOOST_FOREACH(uint32_t ipAddr, ipv4Addresses) {
                FlowEntry *e1 = new FlowEntry();
                SetSecurityMatchEpIpv4(e1, 30, ofPort, macAddr, ipAddr);
                SetSecurityActionAllow(e1);
                el.push_back(e1);

                FlowEntry *e2 = new FlowEntry();
                SetSecurityMatchEpArp(e2, 40, ofPort, macAddr, ipAddr);
                SetSecurityActionAllow(e2);
                el.push_back(e2);
            }
        }
        // TODO IPv6
        WriteFlow(uuid, portSecurityTable, el);
    }

    optional<URI> epgURI = endPoint.getEgURI();
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    uint32_t epgVnid, rdId, bdId, fdId;
    optional<URI> fdURI;
    if (!GetGroupForwardingInfo(epgURI.get(), epgVnid, rdId, bdId,
            fdURI, fdId)) {
        return;
    }

    /* Source Table flows; applicable only to local endpoints */
    if (isLocalEp) {
        FlowEntryList src;
        FlowEntry *e0 = new FlowEntry();
        SetSourceMatchEp(e0, 140, ofPort, macAddr);
        SetSourceAction(e0, epgVnid, bdId, fdId, rdId);
        src.push_back(e0);

        if (endPoint.isPromiscuousMode()) {
            // if the source is unknown, but the interface is
            // promiscuous we allow the traffic into the learning
            // table
            FlowEntry *e1 = new FlowEntry();
            SetSourceMatchEp(e1, 139, ofPort, NULL);
            SetSourceAction(e1, epgVnid, bdId, fdId, rdId, LEARN_TABLE_ID);
            src.push_back(e1);
        }

        WriteFlow(uuid, sourceTable, src);
    }

    FlowEntryList elDst;
    uint32_t epPort = isLocalEp ? ofPort : GetTunnelPort();

    if (bdId != 0 && epPort != OFPP_NONE) {
        FlowEntry *e0 = new FlowEntry();
        SetDestMatchEpMac(e0, 10, macAddr, bdId);
        SetDestActionEpMac(e0, isLocalEp, epgVnid, epPort,
                GetTunnelDstIpv4());
        elDst.push_back(e0);
    }

    if (rdId != 0 && epPort != OFPP_NONE) {
        BOOST_FOREACH (uint32_t ipAddr, ipv4Addresses) {
            FlowEntry *e0 = new FlowEntry();
            SetDestMatchEpIpv4(e0, 15, GetRouterMacAddr(), ipAddr, rdId);
            // XXX for remote EP, dl_dst needs to be the "next-hop" MAC
            const uint8_t *dstMac = isLocalEp ? macAddr : NULL;
            SetDestActionEpIpv4(e0, isLocalEp, epgVnid, epPort,
                    GetTunnelDstIpv4(), GetRouterMacAddr(), dstMac);
            elDst.push_back(e0);

            // ARP optimization: broadcast -> unicast
            // XXX TODO - implement ARP policy from flood domain
            FlowEntry *e1 = new FlowEntry();
            SetDestMatchArpIpv4(e1, 20, ipAddr, rdId);
            SetDestActionEpArpIpv4(e1, isLocalEp, epgVnid, epPort,
                    GetTunnelDstIpv4(), macAddr);
            elDst.push_back(e1);
        }
        // TODO IPv6 address
    }
    WriteFlow(uuid, destinationTable, elDst);

    if (fdURI && isLocalEp) {
        UpdateEndpointFloodDomain(fdURI.get(), endPoint, ofPort,
                                  endPoint.isPromiscuousMode());
    } else {
        RemoveEndpointFromFloodDomain(uuid);
    }
}

void
FlowManager::egDomainUpdated(const URI& epgURI) {
    const string& epgId = epgURI.toString();

    uint32_t tunPort = GetTunnelPort();

    {
        // write static port security flows that do not depend on any
        // particular EPG
        FlowEntryList fixedFlows;

        FlowEntry *dropARP = new FlowEntry();
        SetSecurityMatchEpArp(dropARP, 25, OFPP_ANY, NULL, 0);
        fixedFlows.push_back(dropARP);

        FlowEntry *dropIPv4 = new FlowEntry();
        SetSecurityMatchEpIpv4(dropIPv4, 25, OFPP_ANY, NULL, 0);
        fixedFlows.push_back(dropIPv4);

        if (tunPort != OFPP_NONE) {
            // allow all traffic from the tunnel uplink through the port
            // security table
            FlowEntry *allowTunnel = new FlowEntry();
            SetSecurityMatchEpMac(allowTunnel, 50, tunPort, NULL);
            SetSecurityActionAllow(allowTunnel);
            fixedFlows.push_back(allowTunnel);
        }

        // TODO IPv6
        WriteFlow("static", portSecurityTable, fixedFlows);
    }

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {  // EPG removed
        WriteFlow(epgId, sourceTable, NULL);
        WriteFlow(epgId, policyTable, NULL);
        return;
    }

    uint32_t epgVnid, rdId, bdId, fdId;
    optional<URI> fdURI;
    if (!GetGroupForwardingInfo(epgURI, epgVnid, rdId, bdId, fdURI, fdId)) {
        return;
    }

    if (tunPort != OFPP_NONE) {
        // Assign the source registers based on the VNID from the
        // tunnel uplink
        FlowEntry *e0 = new FlowEntry();
        SetSourceMatchEpg(e0, 150, tunPort, epgVnid);
        SetSourceAction(e0, epgVnid, bdId, fdId, rdId);
        WriteFlow(epgId, sourceTable, e0);
    } else {
        WriteFlow(epgId, sourceTable, NULL);
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
        WriteFlow(epgId, policyTable, e0);
    } else {
        WriteFlow(epgId, policyTable, NULL);
    }

    if (rdId != 0) {
        UpdateGroupSubnets(epgURI, rdId);
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
        optional<const string&> routerIpStr = sn->getVirtualRouterIp();
        uint32_t routerIpv4 = 0;
        if (!routerIpStr ||
            !ParseIpv4Addr(routerIpStr.get(), &routerIpv4)) {
            continue;
        }

        FlowEntry *e0 = new FlowEntry();
        SetDestMatchArpIpv4(e0, 20, routerIpv4, routingDomainId);
        SetDestActionSubnetArpIpv4(e0, GetRouterMacAddr(), routerIpv4);
        WriteFlow(sn->getURI().toString(), destinationTable, e0);
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
    if (type != OFPGC11_DELETE && tunPort != OFPP_NONE) {
        ofputil_bucket *bkt = CreateBucket(tunPort);
        ActionBuilder ab;
        ab.SetRegMove(MFF_REG0, MFF_TUN_ID);
        ab.SetRegLoad(MFF_TUN_DST, GetTunnelDstIpv4());
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
        const Endpoint& endPoint, uint32_t epPort, bool isPromiscuous) {
    const std::string& epUUID = endPoint.getUUID();
    std::pair<uint32_t, bool> epPair(epPort, isPromiscuous);
    uint32_t fdId = GetId(FloodDomain::CLASS_ID, fdURI);
    string fdStrId = fdURI.toString();
    FdMap::iterator fdItr = fdMap.find(fdURI);

    optional<shared_ptr<FloodDomain> > fd = FloodDomain::resolve(fdURI);
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
    entryList.push_back(e0);

    if (floodMode == UnknownFloodModeEnumT::CONST_FLOOD) {
        // go to the learning table on an unknown unicast
        // destination in flood mode
        FlowEntry *unicastLearn = new FlowEntry();
        SetMatchFd(unicastLearn, 5, fdId, false, DST_TABLE_ID);
        SetActionGotoLearn(unicastLearn);
        entryList.push_back(unicastLearn);

        // Deliver unknown packets in the flood domain when
        // learning to the controller for reactive flow setup.
        FlowEntry *fdContr = new FlowEntry();
        SetMatchFd(fdContr, 5, fdId, false, LEARN_TABLE_ID);
        SetActionController(fdContr);
        WriteFlow(fdStrId, learningTable, fdContr);

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
                epLearn.push_back(learnEntry);
            }
        }
    }
    WriteFlow(fdStrId, destinationTable, entryList);
    WriteFlow(epUUID, learningTable, epLearn);
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
            WriteFlow(fdURI.toString(), destinationTable, NULL);
            WriteFlow(fdURI.toString(), learningTable, NULL);
            fdMap.erase(fdURI);
        }
        WriteGroupMod(e0);
        WriteGroupMod(e1);
        break;
    }
}

void FlowManager::contractUpdated(const opflex::modb::URI& contractURI) {
    const string& contractId = contractURI.toString();
    uint64_t conCookie = GetId(Contract::CLASS_ID, contractURI);

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.contractExists(contractURI)) {  // Contract removed
        WriteFlow(contractId, policyTable, NULL);
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
    WriteFlow(contractId, policyTable, entryList);
}

void FlowManager::AddEntryForClassifier(L24Classifier *classifier,
        uint16_t priority, uint64_t cookie, uint32_t& svnid, uint32_t& dvnid,
        flow::FlowEntryList& entries) {
    using namespace modelgbp::arp;
    using namespace modelgbp::l2;

    FlowEntry *e0 = new FlowEntry();
    match *m = &(e0->entry->match);
    e0->entry->cookie = htonll(cookie);
    SetPolicyMatchEpgs(e0, priority, svnid, dvnid);
    SetPolicyActionAllow(e0);
    entries.push_back(e0);

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
    if (classifier->isSFromPortSet()) {
        match_set_tp_src(m, htons(classifier->getSFromPort().get()));
    }
    if (classifier->isDFromPortSet()) {
        match_set_tp_dst(m, htons(classifier->getDFromPort().get()));
    }
}

bool
FlowManager::WriteFlow(const string& objId, TableState& tab,
        FlowEntryList& el) {
    FlowEdit diffs;
    tab.DiffEntry(objId, el, diffs);

    bool success = executor->Execute(diffs);
    if (success) {
        tab.Update(objId, el);
    } else {
        LOG(ERROR) << "Writing flows for " << objId << " failed";
    }
    // clean-up old entry data (or new entry data in case of failure)
    for (int i = 0; i < el.size(); ++i) {
        delete el[i];
    }
    el.clear();
    return success;
}

bool
FlowManager::WriteFlow(const string& objId, TableState& tab, FlowEntry *el) {
    FlowEntryList tmpEl;
    if (el != NULL) {
        tmpEl.push_back(el);
    }
    WriteFlow(objId, tab, tmpEl);
}

bool
FlowManager::WriteGroupMod(const GroupEdit::Entry& e) {
    GroupEdit ge;
    ge.edits.push_back(e);
    bool success = executor->Execute(ge);
    if (!success) {
        LOG(ERROR) << "Group mod failed for group_id=" << e->mod->group_id;
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

bool
FlowManager::ParseIpv4Addr(const string& str, uint32_t *ip) {
    struct in_addr addr;
    if (!inet_pton(AF_INET, str.c_str(), &addr)) {
        return false;
    }
    *ip = addr.s_addr;
    return true;
}

bool
FlowManager::ParseIpv6Addr(const string& str, in6_addr *ip) {
    if (!inet_pton(AF_INET6, str.c_str(), ip)) {
        return false;
    }
    return true;
}

uint32_t FlowManager::GetId(class_id_t cid, const URI& uri) {
    IdMap *idMap = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   idMap = &routingDomainIds; break;
    case BridgeDomain::CLASS_ID:    idMap = &bridgeDomainIds; break;
    case FloodDomain::CLASS_ID:     idMap = &floodDomainIds; break;
    case Contract::CLASS_ID:        idMap = &contractIds; break;
    default:
        assert(false);
    }
    return idMap->FindOrGenerate(uri);
}

uint32_t FlowManager::IdMap::FindOrGenerate(const URI& uri) {
    unordered_map<URI, uint32_t>::const_iterator it = ids.find(uri);
    if (it == ids.end()) {
        ids[uri] = ++lastUsedId;
        return lastUsedId;
    }
    return it->second;
}

void FlowManager::registerConnection(SwitchConnection* connection) {
    this->connection = connection;
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
    fm.table_id = LEARN_TABLE_ID;
    fm.priority = stage2 ? 150 : 100;
    fm.idle_timeout = 300;

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
        ab.SetGotoTable(POL_TABLE_ID);
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

    writeLearnFlow(conn, proto, pi, flow, false);
    if (writeLearnFlow(conn, proto, pi, flow, true))
        return;

    {
        // install a forward flow to flood to the promiscuous ports in
        // the flood domain until we learn the correct reverse path
        struct ofputil_flow_mod fm;
        memset(&fm, 0, sizeof(fm));
        fm.buffer_id = pi.buffer_id;
        fm.table_id = LEARN_TABLE_ID;
        fm.priority = 50;
        fm.idle_timeout = 5;
        fm.hard_timeout = 60;

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

}   // namespace enforcer
}   // namespace opflex
