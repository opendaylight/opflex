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

#include <internal/modb.h>
#include <internal/model.h>
#include <ovs.h>
#include <flowManager.h>
#include <flow/tableState.h>
#include <flow/actionBuilder.h>


namespace opflex {
namespace enforcer {

using namespace std;
using namespace opflex::modb;
using namespace opflex::enforcer::flow;

static const uint8_t SEC_TABLE_ID = 0,
                     SRC_TABLE_ID = 1,
                     DST_TABLE_ID = 2,
                     POL_TABLE_ID = 3;

static const uint8_t MAC_ADDR_BROADCAST[6] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

FlowManager::FlowManager(Inventory& im, ChangeList& cii) :
    invtManager(im), changeInfoIn(cii),
    sourceTable(flow::TableState::SOURCE_FILTER),
    destinationTable(flow::TableState::DESTINATION_FILTER),
    policyTable(flow::TableState::POLICY),
    portSecurityTable(flow::TableState::PORT_SECURITY) {
    ParseMacAddr("de:ad:be:ef:ce:de", routerMac);   // read from config file
    tunnelPort = 1024;                              // read from config file
    ParseIpv4Addr("10.10.10.10", &tunnelDstIpv4);   // read from config file
}

void FlowManager::Start()
{
}

void FlowManager::Stop()
{
}

void FlowManager::Generate()
{
    ChangeList cl;
    cl.swap(changeInfoIn);

    BOOST_FOREACH(ChangeInfo& ci, cl) {
        switch (ci.GetClass()) {
        case model::CLASS_ID_ENDPOINT:
            HandleEndpoint(ci);
            break;
        case model::CLASS_ID_ENDPOINT_GROUP:
            HandleEndpointGroup(ci);
            break;
        case model::CLASS_ID_SUBNET:
            HandleSubnet(ci);
            break;
        case model::CLASS_ID_FD:
            HandleFloodDomain(ci);
            break;
        case model::CLASS_ID_POLICY:
            HandlePolicy(ci);
            break;
        }
    }
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
SetSourceMatchEp(FlowEntry *fe, uint16_t prio, uint32_t ofPort, const uint8_t *mac) {
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
SetDestMatchEpMac(FlowEntry *fe, uint16_t prio, const uint8_t *mac, uint32_t bdId) {
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
SetDestActionEpArpIpv4(FlowEntry *fe, bool isLocal, uint32_t epgId, uint32_t port,
        uint32_t tunDst, const uint8_t *dstMac) {
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
    match_set_nw_dst(&fe->entry->match, epIpv4);
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

void
FlowManager::HandleEndpoint(ChangeInfo& chgInfo) {
    const MoBasePtr& obj = chgInfo.GetObject();
    const URI& uri = chgInfo.GetUri();
    bool isRemove = (chgInfo.changeType == ChangeInfo::REMOVE);

    const string& macAddrStr = obj->Get("macAddr");
    const string& ipAddrStr = obj->Get("ipAddr");

    uint8_t macAddr[6];
    bool macOk = false;
    if (!macAddrStr.empty()) {
        macOk = ParseMacAddr(macAddrStr, macAddr);
    }

    if (!macOk) {
        return;
    }

    uint32_t ipAddr = 0;
    bool ipv4Ok = false;
    if (!ipAddrStr.empty()) {
        ipv4Ok = ParseIpv4Addr(ipAddrStr, &ipAddr);
    }

    const string& ofPortStr = obj->Get("ofport");
    uint32_t ofPort = -1;
    bool isLocalEp = false;
    if (!ofPortStr.empty()) {
        ofPort = atoi(ofPortStr.c_str());
        isLocalEp = true;
    }

    /*
     * Port security flows
     */
    if (isLocalEp) {
        EntryList el;
        if (!isRemove) {
            FlowEntry *e0 = new FlowEntry();
            SetSecurityMatchEpMac(e0, 20, ofPort, macAddr);
            SetSecurityActionAllow(e0);
            el.push_back(e0);

            if (ipv4Ok) {
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
        WriteFlow(uri, portSecurityTable, el);
    }

    URI epgUri(obj->Get("epgUri"));
    MoBasePtr epg = invtManager.Find(epgUri);
    if (epg == NULL) {
        // can't do anything more
        return;
    }
    uint32_t epgId = atoi(epg->Get("id").c_str());
    URI netDomUri(epg->Get("networkDomainUri"));
    MoBasePtr fd = invtManager.FindNetObj(netDomUri, model::CLASS_ID_FD);
    MoBasePtr bd = invtManager.FindNetObj(netDomUri, model::CLASS_ID_BD);
    MoBasePtr l3domain = invtManager.FindNetObj(netDomUri, model::CLASS_ID_L3CTX);

    if (fd == NULL && bd == NULL && l3domain == NULL) {
        return;
    }

    uint32_t fdId = 0, bdId = 0, l3Id = 0;
    if (fd != NULL) {
        fdId = atoi(fd->Get("id").c_str());
    }
    if (bd != NULL) {
        bdId = atoi(bd->Get("id").c_str());
    }
    if (l3domain != NULL) {
        l3Id = atoi(l3domain->Get("id").c_str());
    }

    /*
     * Source Table flows.
     * Applicable only to local endpoints
     */
    if (isLocalEp) {
        FlowEntry *e0 = NULL;
        if (!isRemove) {
            e0 = new FlowEntry();
            SetSourceMatchEp(e0, 140, ofPort, macAddr);
            SetSourceAction(e0, epgId, bdId, fdId, l3Id);
        }
        WriteFlow(uri, sourceTable, e0);
    }

    /*
     * Destination Table flows.
     * local EP:
     * [dMac=ep.mac,NxReg4=ep->epg.bdId] ->
     *     NxReg2=ep->epg.Id, NxReg3=ep.cgId(ep->epg), NxReg7=ep.ofport
     * [dMac=ROUTER_MAC,dIp=ep.ip,NxReg6=ep->epg.L3Id] ->
     *     NxReg2=ep->epg.Id, NxReg3=ep.cgId(ep->epg),NxReg7=ep.ofport,
     *     sMac=ROUTER_MAC,dMac=ep.mac,--ttl
     *
     * Remote EP:
     * [dMac=ep.mac,NxReg4=ep->epg.bdId] ->
     *     NxReg2=ep->epg.Id, NxReg3=ep.cgId(ep->epg),NxReg7=TUN_PORT,
     *     TUN_ID=NxReg0,TUN_DST=tunIpv4
     * [dMac=ROUTER_MAC,dIp=ep.ip,NxReg6=ep->epg.L3Id] ->
     *     NxReg2=ep->epg.Id, NxReg3=ep.cgId(ep->epg),NxReg7=TUN_PORT,
     *     TUN_ID=NxReg0,TUN_DST=tunIpv4,sMac=ROUTER_MAC,--ttl
     *
     * ARP optimization
     * [etherType=ARP,arpOp=1,arpTpa=ep.ip,dMac=BROADCAST] =>
     *     NxReg2=ep->epg.Id, NxReg3=ep.cgId(ep->epg), NxReg7=ep.ofport
     *     (or NxReg7=TUN_PORT,TUN_ID=NxReg0,TUN_DST=tunIp), dMac=ep.mac
     *
     */
    if (isRemove) {
        WriteFlow(uri, destinationTable, NULL);
    } else {
        EntryList el;
        uint32_t epPort = isLocalEp ? ofPort : GetTunnelPort();

        if (bd != NULL) {
            FlowEntry *e0 = new FlowEntry();
            SetDestMatchEpMac(e0, 10, macAddr, bdId);
            SetDestActionEpMac(e0, isLocalEp, epgId, epPort,
                    GetTunnelDstIpv4());
            el.push_back(e0);
        }

        if (ipv4Ok && l3domain != NULL) {
            FlowEntry *e0 = new FlowEntry();
            SetDestMatchEpIpv4(e0, 15, GetRouterMacAddr(), ipAddr, l3Id);
            // XXX for remote EP, dl_dst needs to be the "next-hop" MAC
            const uint8_t *dstMac = isLocalEp ? macAddr : NULL;
            SetDestActionEpIpv4(e0, isLocalEp, epgId, epPort,
                    GetTunnelDstIpv4(), GetRouterMacAddr(), dstMac);
            el.push_back(e0);

            // ARP optimization: broadcast -> unicast
            FlowEntry *e1 = new FlowEntry();
            SetDestMatchArpIpv4(e1, 20, ipAddr, l3Id);
            SetDestActionEpArpIpv4(e1, isLocalEp, epgId, epPort,
                    GetTunnelDstIpv4(), macAddr);
            el.push_back(e1);
        }

        // TODO IPv6 address

        WriteFlow(uri, destinationTable, el);
    }

    // TODO Group-table flow
    if (fd != NULL) {
    }

}

void
FlowManager::HandleEndpointGroup(ChangeInfo& chgInfo) {
    const MoBasePtr& epg = chgInfo.GetObject();
    const URI& uri = chgInfo.GetUri();
    bool isRemove = (chgInfo.changeType == ChangeInfo::REMOVE);

    uint32_t epgId = atoi(epg->Get("id").c_str());
    URI netDomUri(epg->Get("networkDomainUri"));
    MoBasePtr fd = invtManager.FindNetObj(netDomUri, model::CLASS_ID_FD);
    MoBasePtr bd = invtManager.FindNetObj(netDomUri, model::CLASS_ID_BD);
    MoBasePtr l3domain = invtManager.FindNetObj(netDomUri, model::CLASS_ID_L3CTX);

    if (fd == NULL && bd == NULL && l3domain == NULL) {
        return;
    }

    uint32_t fdId = 0, bdId = 0, l3Id = 0;
    if (fd != NULL) {
        fdId = atoi(fd->Get("id").c_str());
    }
    if (bd != NULL) {
        bdId = atoi(bd->Get("id").c_str());
    }
    if (l3domain != NULL) {
        l3Id = atoi(l3domain->Get("id").c_str());
    }

    FlowEntry *e0 = NULL;
    if (!isRemove) {
        e0 = new FlowEntry();
        SetSourceMatchEpg(e0, 150, GetTunnelPort(), epgId);
        SetSourceAction(e0, epgId, bdId, fdId, l3Id);
    }
    WriteFlow(uri, sourceTable, e0);

    bool allowIntraGroup = true;        // XXX read from EPG
    e0 = NULL;
    if (!isRemove && allowIntraGroup) {
        e0 = new FlowEntry();
        SetPolicyMatchEpgs(e0, 100, epgId, epgId);
        SetPolicyActionAllow(e0);
    }
    WriteFlow(uri, policyTable, e0);
}

void
FlowManager::HandleSubnet(ChangeInfo& chgInfo) {
    const MoBasePtr& sn = chgInfo.GetObject();
    const URI& uri = chgInfo.GetUri();
    bool isRemove = (chgInfo.changeType == ChangeInfo::REMOVE);

    const string& gatewayIpStr = sn->Get("gateway");
    uint32_t gwIpv4 = 0;
    bool gwIpv4Ok = false;
    if (!gatewayIpStr.empty()) {
        gwIpv4Ok = ParseIpv4Addr(gatewayIpStr, &gwIpv4);
    }

    URI parentUri(sn->Get("parentUri"));
    MoBasePtr l3domain = invtManager.FindNetObj(parentUri, model::CLASS_ID_L3CTX);

    if (!gwIpv4Ok || l3domain == NULL) {
        return;
    }

    uint32_t l3Id = atoi(l3domain->Get("id").c_str());

    /* Subnet Router ARP */
   FlowEntry *e0 = NULL;
   if (!isRemove) {
       e0 = new FlowEntry();
       SetDestMatchArpIpv4(e0, 20, gwIpv4, l3Id);
       SetDestActionSubnetArpIpv4(e0, GetRouterMacAddr(), gwIpv4);
   }
   WriteFlow(uri, destinationTable, e0);
}

void
FlowManager::HandleFloodDomain(ChangeInfo& chgInfo) {
    const MoBasePtr& fd = chgInfo.GetObject();
    uint32_t fdId = atoi(fd->Get("id").c_str());

    // Group table mod
}

void
FlowManager::HandlePolicy(ChangeInfo& chgInfo) {
    const MoBasePtr& plcy = chgInfo.GetObject();
    const URI& uri = chgInfo.GetUri();
    bool isRemove = (chgInfo.changeType == ChangeInfo::REMOVE);

    URI sEpgUri(plcy->Get("sEpgUri"));
    MoBasePtr sEpg = invtManager.Find(sEpgUri);
    URI dEpgUri(plcy->Get("dEpgUri"));
    MoBasePtr dEpg = invtManager.Find(dEpgUri);

    if (sEpg == NULL || dEpg == NULL) {
        return;
    }
    uint32_t sEpgId = atoi(sEpg->Get("id").c_str());
    uint32_t dEpgId = atoi(dEpg->Get("id").c_str());

    // TODO Conditions
    // TODO Other criteria

    const string& actions = plcy->Get("actions");
    bool drop = actions.find("drop") != string::npos;

    FlowEntry *e0 = NULL;
    if (!isRemove && !drop) {
        e0 = new FlowEntry();
        SetPolicyMatchEpgs(e0, 110, sEpgId, dEpgId);
        SetPolicyActionAllow(e0);
    }
    WriteFlow(uri, policyTable, e0);
}

bool
FlowManager::WriteFlow(const URI& uri, TableState& tab, EntryList& el) {
    FlowEdit diffs;
    tab.DiffEntry(uri, el, diffs);

    bool success = executor->Execute(tab.GetType(), diffs);
    if (success) {
        tab.Update(uri, el);
    }
    // clean-up old entry data (or new entry data in case of failure)
    for (int i = 0; i < el.size(); ++i) {
        delete el[i];
    }
    el.clear();
    return success;
}

bool
FlowManager::WriteFlow(const URI& uri, TableState& tab, Entry *el) {
    EntryList tmpEl;
    if (el != NULL) {
        tmpEl.push_back(el);
    }
    WriteFlow(uri, tab, tmpEl);
}

bool
FlowManager::ParseMacAddr(const string& str, uint8_t *mac) {
    memset(mac, 0, sizeof(uint8_t)*6);
    sscanf(str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            mac, mac+1, mac+2, mac+3, mac+4, mac+5);
    return true;
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

}   // namespace enforcer
}   // namespace opflex
