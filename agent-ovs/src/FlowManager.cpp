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
                     POL_TABLE_ID = 3;

static const uint8_t MAC_ADDR_BROADCAST[6] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

const uint16_t FlowManager::MAX_POLICY_RULE_PRIORITY = 8196;     // arbitrary

FlowManager::FlowManager(ovsagent::Agent& ag) :
        agent(ag) {
    MAC("de:ad:be:ef:ce:de").toUIntArray(routerMac); // read from config file
    tunnelPort = 1024;                               // read from config file
    ParseIpv4Addr("10.10.10.10", &tunnelDstIpv4);    // read from config file
    portMapper = NULL;
}

void FlowManager::Start()
{
    agent.getEndpointManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);
}

void FlowManager::Stop()
{
    agent.getEndpointManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);
}

/** Source table helper functions */
static void
SetSourceAction(FlowEntry *fe, uint32_t epgId,
                uint32_t bdId,  uint32_t fdId,  uint32_t l3Id)
{
    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG0, epgId);
    ab.SetRegLoad(MFF_REG4, bdId);
    ab.SetRegLoad(MFF_REG5, fdId);
    ab.SetRegLoad(MFF_REG6, l3Id);
    ab.SetGotoTable(DST_TABLE_ID);

    ab.Build(fe->entry);
}

static void
SetSourceMatchEp(FlowEntry *fe, uint16_t prio, uint32_t ofPort,
        const uint8_t *mac) {
    fe->entry->table_id = SRC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, ofPort);
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
    match_set_dl_src(&fe->entry->match, epMac);
}

static void
SetSecurityMatchEpIpv4(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac, uint32_t epIpv4) {
    fe->entry->table_id = SEC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, port);
    match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_IP));
    match_set_nw_src(&fe->entry->match, epIpv4);
}

static void
SetSecurityMatchEpArp(FlowEntry *fe, uint16_t prio, uint32_t port,
        const uint8_t *epMac, uint32_t epIpv4) {
    fe->entry->table_id = SEC_TABLE_ID;
    fe->entry->priority = prio;
    match_set_in_port(&fe->entry->match, port);
    match_set_dl_src(&fe->entry->match, epMac);
    match_set_dl_type(&fe->entry->match, htons(ETH_TYPE_ARP));
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
        uint32_t& rdId, uint32_t& bdId, uint32_t& fdId) {
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
    fdId = epgFd ?
            GetId(FloodDomain::CLASS_ID, epgFd.get()->getURI()) : 0;
    return true;
}

void
FlowManager::endpointUpdated(const string& uuid) {

    EndpointManager& epMgr = agent.getEndpointManager();
    optional<const Endpoint&> epWrapper = epMgr.getEndpoint(uuid);

    if (!epWrapper) {   // EP removed
        WriteFlow(uuid, portSecurityTable, NULL);
        WriteFlow(uuid, sourceTable, NULL);
        WriteFlow(uuid, destinationTable, NULL);
        return;
    }
    const Endpoint& endPoint = epWrapper.get();
    uint8_t macAddr[6];
    endPoint.getMAC().toUIntArray(macAddr);

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
    optional<string> ofPortName = endPoint.getInterfaceName();
    if (ofPortName && portMapper) {
        ofPort = portMapper->FindPort(ofPortName.get());
    }
    bool isLocalEp = (ofPort != OFPP_NONE);

    /* Port security flows */
    if (isLocalEp) {
        FlowEntryList el;

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
        // TODO IPv6
        WriteFlow(uuid, portSecurityTable, el);
    }

    optional<URI> epgURI = endPoint.getEgURI();
    if (!epgURI) {      // can't do much without EPG
        return;
    }

    uint32_t epgVnid, rdId, bdId, fdId;
    if (!GetGroupForwardingInfo(epgURI.get(), epgVnid, rdId, bdId, fdId)) {
        return;
    }

    /* Source Table flows; applicable only to local endpoints */
    if (isLocalEp) {
        FlowEntry *e0 = new FlowEntry();
        SetSourceMatchEp(e0, 140, ofPort, macAddr);
        SetSourceAction(e0, epgVnid, bdId, fdId, rdId);
        WriteFlow(uuid, sourceTable, e0);
    }

    FlowEntryList elDst;
    uint32_t epPort = isLocalEp ? ofPort : GetTunnelPort();

    if (bdId != 0) {
        FlowEntry *e0 = new FlowEntry();
        SetDestMatchEpMac(e0, 10, macAddr, bdId);
        SetDestActionEpMac(e0, isLocalEp, epgVnid, epPort,
                GetTunnelDstIpv4());
        elDst.push_back(e0);
    }

    if (rdId != 0) {
        BOOST_FOREACH (uint32_t ipAddr, ipv4Addresses) {
            FlowEntry *e0 = new FlowEntry();
            SetDestMatchEpIpv4(e0, 15, GetRouterMacAddr(), ipAddr, rdId);
            // XXX for remote EP, dl_dst needs to be the "next-hop" MAC
            const uint8_t *dstMac = isLocalEp ? macAddr : NULL;
            SetDestActionEpIpv4(e0, isLocalEp, epgVnid, epPort,
                    GetTunnelDstIpv4(), GetRouterMacAddr(), dstMac);
            elDst.push_back(e0);

            // ARP optimization: broadcast -> unicast
            FlowEntry *e1 = new FlowEntry();
            SetDestMatchArpIpv4(e1, 20, ipAddr, rdId);
            SetDestActionEpArpIpv4(e1, isLocalEp, epgVnid, epPort,
                    GetTunnelDstIpv4(), macAddr);
            elDst.push_back(e1);
        }
        // TODO IPv6 address
        WriteFlow(uuid, destinationTable, elDst);
    }

    // TODO Group-table flow
}

void
FlowManager::egDomainUpdated(const URI& epgURI) {

    const string& epgId = epgURI.toString();

    PolicyManager& polMgr = agent.getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {  // EPG removed
        WriteFlow(epgId, sourceTable, NULL);
        WriteFlow(epgId, policyTable, NULL);
        return;
    }

    uint32_t epgVnid, rdId, bdId, fdId;
    if (!GetGroupForwardingInfo(epgURI, epgVnid, rdId, bdId, fdId)) {
        return;
    }

    FlowEntry *e0 = new FlowEntry();
    SetSourceMatchEpg(e0, 150, GetTunnelPort(), epgVnid);
    SetSourceAction(e0, epgVnid, bdId, fdId, rdId);
    WriteFlow(epgId, sourceTable, e0);

    bool allowIntraGroup = true;        // XXX read from EPG
    if (allowIntraGroup) {
        e0 = new FlowEntry();
        SetPolicyMatchEpgs(e0, 100, epgVnid, epgVnid);
        SetPolicyActionAllow(e0);
        WriteFlow(epgId, policyTable, e0);
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
        optional<string> routerIpStr;
        uint32_t routerIpv4 = 0;
        // XXX add this to model
        // routerIpStr = sn->getRouterAddress();
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

}   // namespace enforcer
}   // namespace opflex
