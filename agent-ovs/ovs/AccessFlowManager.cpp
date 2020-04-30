/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AccessFlowManager.h"
#include "CtZoneManager.h"
#include "FlowBuilder.h"
#include "FlowUtils.h"
#include "FlowConstants.h"
#include "RangeMask.h"
#include "eth.h"
#include <opflexagent/logging.h>

#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/sub_range.hpp>
#include "ovs-ofputil.h"
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>

#include <string>
#include <vector>
#include <sstream>

namespace opflexagent {

using std::string;
using std::vector;
using std::unordered_set;
using std::shared_ptr;
using opflex::modb::URI;
typedef EndpointListener::uri_set_t uri_set_t;
using boost::algorithm::make_split_iterator;
using boost::algorithm::token_finder;
using boost::algorithm::is_any_of;
using boost::copy_range;
using boost::optional;

static const char* ID_NAMESPACES[] =
    {"secGroup", "secGroupSet"};

static const char* ID_NMSPC_SECGROUP     = ID_NAMESPACES[0];
static const char* ID_NMSPC_SECGROUP_SET = ID_NAMESPACES[1];

void AccessFlowManager::populateTableDescriptionMap(
        SwitchManager::TableDescriptionMap &fwdTblDescr) {
    // Populate descriptions of flow tables
#define TABLE_DESC(table_id, table_name, drop_reason) \
        fwdTblDescr.insert( \
                    std::make_pair(table_id, \
                            std::make_pair(table_name, drop_reason)));
    TABLE_DESC(GROUP_MAP_TABLE_ID, "GROUP_MAP_TABLE", "Access port incorrect")
    TABLE_DESC(SEC_GROUP_IN_TABLE_ID, "SEC_GROUP_IN_TABLE",
            "Egress Security group derivation missing/incorrect")
    TABLE_DESC(SEC_GROUP_OUT_TABLE_ID, "SEC_GROUP_OUT_TABLE",
            "Ingress security group missing/incorrect")
    TABLE_DESC(OUT_TABLE_ID, "OUT_TABLE", "Output port missing/incorrect")
#undef TABLE_DESC
}

AccessFlowManager::AccessFlowManager(Agent& agent_,
                                     SwitchManager& switchManager_,
                                     IdGenerator& idGen_,
                                     CtZoneManager& ctZoneManager_)
    : agent(agent_), switchManager(switchManager_), idGen(idGen_),
      ctZoneManager(ctZoneManager_), taskQueue(agent.getAgentIOService()),
      conntrackEnabled(false), stopping(false), dropLogRemotePort(0) {
    // set up flow tables
    switchManager.setMaxFlowTables(NUM_FLOW_TABLES);
    SwitchManager::TableDescriptionMap fwdTblDescr;
    populateTableDescriptionMap(fwdTblDescr);
    switchManager.setForwardingTableList(fwdTblDescr);
}

static string getSecGrpSetId(const uri_set_t& secGrps) {
   std::stringstream ss;
   bool notfirst = false;
   for (const URI& uri : secGrps) {
       if (notfirst) ss << ",";
       notfirst = true;
       ss << uri.toString();
   }
   return ss.str();
}

void AccessFlowManager::enableConnTrack() {
    conntrackEnabled = true;
}

void AccessFlowManager::start() {
    switchManager.getPortMapper().registerPortStatusListener(this);
    agent.getEndpointManager().registerListener(this);
    agent.getLearningBridgeManager().registerListener(this);
    agent.getPolicyManager().registerListener(this);

    for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
        idGen.initNamespace(ID_NAMESPACES[i]);
    }

    createStaticFlows();
}

void AccessFlowManager::stop() {
    stopping = true;
    switchManager.getPortMapper().unregisterPortStatusListener(this);
    agent.getEndpointManager().unregisterListener(this);
    agent.getLearningBridgeManager().unregisterListener(this);
    agent.getPolicyManager().unregisterListener(this);
}

void AccessFlowManager::endpointUpdated(const string& uuid) {
    if (stopping) return;
    taskQueue.dispatch(uuid, [=](){ handleEndpointUpdate(uuid); });
}

void AccessFlowManager::secGroupSetUpdated(const uri_set_t& secGrps) {
    if (stopping) return;
    const string id = getSecGrpSetId(secGrps);
    taskQueue.dispatch("set:" + id,
                       [=]() { handleSecGrpSetUpdate(secGrps, id); });
}

void AccessFlowManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    switchManager.enableSync();
}

void AccessFlowManager::secGroupUpdated(const opflex::modb::URI& uri) {
    if (stopping) return;
    taskQueue.dispatch("secgrp:" + uri.toString(),
                       [=]() { handleSecGrpUpdate(uri); });
}

void AccessFlowManager::portStatusUpdate(const string& portName,
                                         uint32_t portNo, bool) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch([=]() { handlePortStatusUpdate(portName, portNo); });
}

void AccessFlowManager::setDropLog(const string& dropLogPort, const string& dropLogRemoteIp,
        const uint16_t _dropLogRemotePort) {
    dropLogIface = dropLogPort;
    boost::system::error_code ec;
    address tunDst = address::from_string(dropLogRemoteIp, ec);
    if (ec) {
        LOG(ERROR) << "Invalid drop-log tunnel destination IP: "
                   << dropLogRemoteIp << ": " << ec.message();
    } else if (tunDst.is_v6()) {
        LOG(ERROR) << "IPv6 drop-log tunnel destinations are not supported";
    } else {
        dropLogDst = tunDst;
        LOG(INFO) << "DropLog port set to " << dropLogPort
                   << " tunnel destination: " << dropLogRemoteIp
                   << ":" <<_dropLogRemotePort;
    }
    dropLogRemotePort = _dropLogRemotePort;
}

static FlowEntryPtr flowEmptySecGroup(uint32_t emptySecGrpSetId) {
    FlowBuilder noSecGrp;
    flowutils::match_group(noSecGrp,
                           PolicyManager::MAX_POLICY_RULE_PRIORITY,
                           emptySecGrpSetId, 0);
    noSecGrp.action().go(AccessFlowManager::OUT_TABLE_ID);
    return noSecGrp.build();
}

static const uint64_t getPushVlanMeta(std::shared_ptr<const Endpoint>& ep) {
    return ep->isAccessAllowUntagged() ?
        flow::meta::access_out::UNTAGGED_AND_PUSH_VLAN :
        flow::meta::access_out::PUSH_VLAN;
}

static void flowBypassDhcpRequest(FlowEntryList& el, bool v4,
                                  bool skip_pop_vlan, uint32_t inport,
                                  uint32_t outport,
                                  std::shared_ptr<const Endpoint>& ep ) {
    FlowBuilder fb;
    if (ep->getAccessIfaceVlan() && !skip_pop_vlan) {
        fb.priority(201).inPort(inport);
    } else {
        fb.priority(200).inPort(inport);
    }

    flowutils::match_dhcp_req(fb, v4);
    fb.action().reg(MFF_REG7, outport);

    if (ep->getAccessIfaceVlan() && !skip_pop_vlan) {
        fb.vlan(ep->getAccessIfaceVlan().get());
        fb.action().metadata(flow::meta::access_out::POP_VLAN,
                             flow::meta::out::MASK);
    }

    if (skip_pop_vlan)
        fb.tci(0, 0x1fff);

    fb.action().go(AccessFlowManager::OUT_TABLE_ID);
    fb.build(el);
}

static void flowBypassFloatingIP(FlowEntryList& el, uint32_t inport,
                                 uint32_t outport, bool in,
                                 bool skip_pop_vlan,
                                 address& floatingIp,
                                 std::shared_ptr<const Endpoint>& ep ) {
    FlowBuilder fb;
    if (ep->getAccessIfaceVlan() && !skip_pop_vlan) {
        fb.priority(201).inPort(inport);
    } else {
        fb.priority(200).inPort(inport);
    }

    if (floatingIp.is_v4()) {
        fb.ethType(eth::type::IP);
    } else {
        fb.ethType(eth::type::IPV6);
    }

    if (in) {
        fb.ipSrc(floatingIp);
    } else {
        fb.ipDst(floatingIp);
    }

    fb.action().reg(MFF_REG7, outport);
    if (ep->getAccessIfaceVlan() && !skip_pop_vlan) {
        if (in) {
            fb.action()
                .reg(MFF_REG5, ep->getAccessIfaceVlan().get())
                .metadata(getPushVlanMeta(ep), flow::meta::out::MASK);
        } else {
            fb.vlan(ep->getAccessIfaceVlan().get());
            fb.action()
                .metadata(flow::meta::access_out::POP_VLAN,
                          flow::meta::out::MASK);
        }
    }

    if (skip_pop_vlan && !in)
        fb.tci(0, 0x1fff);

    fb.action().go(AccessFlowManager::OUT_TABLE_ID);
    fb.build(el);
}

void AccessFlowManager::createStaticFlows() {
    LOG(DEBUG) << "Writing static flows";
    {
        FlowEntryList outFlows;
        FlowBuilder()
            .priority(1)
            .metadata(flow::meta::access_out::POP_VLAN,
                      flow::meta::out::MASK)
            .tci(0x1000, 0x1000)
            .action()
            .popVlan().outputReg(MFF_REG7)
            .parent().build(outFlows);
        FlowBuilder()
            .priority(1)
            .metadata(flow::meta::access_out::PUSH_VLAN,
                      flow::meta::out::MASK)
            .action()
            .pushVlan().regMove(MFF_REG5, MFF_VLAN_VID).outputReg(MFF_REG7)
            .parent().build(outFlows);
        /*
         * The packet is replicated for a specical case of
         * Openshift bootstrap that does not use vlan 4094
         * This is ugly but they do not have iproute2/tc
         * installed to do this in a cleaner way
         * This duplication only happens when endpoint
         * file has "access-interface-vlan" attr
         */
        FlowBuilder()
            .priority(1)
            .metadata(flow::meta::access_out::UNTAGGED_AND_PUSH_VLAN,
                      flow::meta::out::MASK)
            .action()
            .outputReg(MFF_REG7)
            .pushVlan().regMove(MFF_REG5, MFF_VLAN_VID).outputReg(MFF_REG7)
            .parent().build(outFlows);
        outFlows.push_back(flowutils::default_out_flow());

        switchManager.writeFlow("static", OUT_TABLE_ID, outFlows);
    }
    {
        TlvEntryList tlvFlows;
        for(int i = 0; i <= 10; i++) {
            FlowBuilder().tlv(0xffff, i, 4, i).buildTlv(tlvFlows);
        }
        FlowBuilder().tlv(0xffff, 11, 16, 11).buildTlv(tlvFlows);
        FlowBuilder().tlv(0xffff, 12, 4, 12).buildTlv(tlvFlows);
        switchManager.writeTlv("DropLogStatic", tlvFlows);
    }
    {
        FlowEntryList dropLogFlows;
        FlowBuilder().priority(0)
                .action().go(GROUP_MAP_TABLE_ID)
                .parent().build(dropLogFlows);
        switchManager.writeFlow("static", DROP_LOG_TABLE_ID, dropLogFlows);
        /* Insert a flow at the end of every table to match dropped packets
         * and go to the drop table where it will be punted to a port when configured
         */
        for(unsigned table_id = GROUP_MAP_TABLE_ID; table_id < EXP_DROP_TABLE_ID; table_id++) {
            FlowEntryList dropLogFlow;
            FlowBuilder().priority(0).cookie(flow::cookie::TABLE_DROP_FLOW)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .action().dropLog(table_id)
                    .go(EXP_DROP_TABLE_ID)
                    .parent().build(dropLogFlow);
            switchManager.writeFlow("DropLogFlow", table_id, dropLogFlow);
        }
        handleDropLogPortUpdate();
    }

    // everything is allowed for endpoints with no security group set
    uint32_t emptySecGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET, "");
    switchManager.writeFlow("static", SEC_GROUP_OUT_TABLE_ID,
                            flowEmptySecGroup(emptySecGrpSetId));
    switchManager.writeFlow("static", SEC_GROUP_IN_TABLE_ID,
                            flowEmptySecGroup(emptySecGrpSetId));
}

void AccessFlowManager::handleEndpointUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating endpoint " << uuid;
    shared_ptr<const Endpoint> ep =
        agent.getEndpointManager().getEndpoint(uuid);
    if (!ep) {
        switchManager.clearFlows(uuid, GROUP_MAP_TABLE_ID);
        if (conntrackEnabled)
            ctZoneManager.erase(uuid);
        return;
    }

    uint32_t accessPort = OFPP_NONE;
    uint32_t uplinkPort = OFPP_NONE;
    const optional<string>& accessIface = ep->getAccessInterface();
    const optional<string>& uplinkIface = ep->getAccessUplinkInterface();
    if (accessIface)
        accessPort = switchManager.getPortMapper().FindPort(accessIface.get());
    if (uplinkIface) {
        uplinkPort = switchManager.getPortMapper().FindPort(uplinkIface.get());
    }

    uint32_t secGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET,
                                       getSecGrpSetId(ep->getSecurityGroups()));
    uint16_t zoneId = -1;
    if (conntrackEnabled) {
        zoneId = ctZoneManager.getId(uuid);
        if (zoneId == static_cast<uint16_t>(-1))
            LOG(ERROR) << "Could not allocate connection tracking zone for "
                       << uuid;
    }

    MaskList trunkVlans;
    if (ep->getInterfaceName()) {
        LearningBridgeManager& lbMgr = agent.getLearningBridgeManager();
        std::unordered_set<std::string> lbiUuids;
        lbMgr.getLBIfaceByIface(ep->getInterfaceName().get(), lbiUuids);

        for (auto& uuid : lbiUuids) {
            auto iface = lbMgr.getLBIface(uuid);
            if (!iface) continue;

            for (auto& range : iface->getTrunkVlans()) {
                MaskList masks;
                RangeMask::getMasks(range.first, range.second, masks);
                trunkVlans.insert(trunkVlans.end(),
                                  masks.begin(), masks.end());
            }
        }
    }

    FlowEntryList el;
    if (accessPort != OFPP_NONE && uplinkPort != OFPP_NONE) {
        {
            FlowBuilder in;
            in.priority(100).inPort(accessPort);
            if (zoneId != static_cast<uint16_t>(-1))
                in.action()
                    .reg(MFF_REG6, zoneId);

            in.action()
                .reg(MFF_REG0, secGrpSetId)
                .reg(MFF_REG7, uplinkPort);

            if (ep->getAccessIfaceVlan()) {
                in.vlan(ep->getAccessIfaceVlan().get());
                in.action()
                    .metadata(flow::meta::access_out::POP_VLAN,
                              flow::meta::out::MASK);
            } else {
                in.tci(0, 0x1fff);
            }

            in.action().go(SEC_GROUP_OUT_TABLE_ID);
            in.build(el);

        }

        /*
         * We allow without tags to handle Openshift bootstrap
         */
        if (ep->isAccessAllowUntagged() && ep->getAccessIfaceVlan()) {
            FlowBuilder inSkipVlan;

            inSkipVlan.priority(99).inPort(accessPort)
                      .tci(0, 0x1fff);
            if (zoneId != static_cast<uint16_t>(-1))
                inSkipVlan.action()
                    .reg(MFF_REG6, zoneId);

            inSkipVlan.action()
                .reg(MFF_REG0, secGrpSetId)
                .reg(MFF_REG7, uplinkPort)
                .go(SEC_GROUP_OUT_TABLE_ID);
            inSkipVlan.build(el);
        }

        /*
         * Allow DHCP request to bypass the access bridge policy when
         * virtual DHCP is enabled.
         * We allow both with / without tags to handle Openshift
         * bootstrap
         */
        optional<Endpoint::DHCPv4Config> v4c = ep->getDHCPv4Config();
        if (v4c) {
            flowBypassDhcpRequest(el, true, false, accessPort,
                                  uplinkPort, ep);
            if (ep->isAccessAllowUntagged() && ep->getAccessIfaceVlan())
                flowBypassDhcpRequest(el, true, true, accessPort,
                                      uplinkPort, ep);
        }

        optional<Endpoint::DHCPv6Config> v6c = ep->getDHCPv6Config();
        if(v6c) {
            flowBypassDhcpRequest(el, false, false, accessPort,
                                  uplinkPort, ep);
            if (ep->isAccessAllowUntagged() && ep->getAccessIfaceVlan())
                flowBypassDhcpRequest(el, false, true, accessPort,
                                      uplinkPort, ep);
        }

        {
            FlowBuilder out;
            if (zoneId != static_cast<uint16_t>(-1))
                out.action()
                    .reg(MFF_REG6, zoneId);

            out.priority(100).inPort(uplinkPort)
                .action()
                .reg(MFF_REG0, secGrpSetId)
                .reg(MFF_REG7, accessPort);
            if (ep->getAccessIfaceVlan()) {
                out.action()
                    .reg(MFF_REG5, ep->getAccessIfaceVlan().get())
                    .metadata(getPushVlanMeta(ep), flow::meta::out::MASK);
            }
            out.action().go(SEC_GROUP_IN_TABLE_ID);
            out.build(el);
        }

        // Bypass the access bridge for ports trunked by learning
        // bridge interfaces.
        for (const Mask& m : trunkVlans) {
            uint16_t tci = 0x1000 | m.first;
            uint16_t mask = 0x1000 | (0xfff & m.second);
            FlowBuilder().priority(500).inPort(accessPort)
                .tci(tci, mask)
                .action().output(uplinkPort)
                .parent().build(el);
            FlowBuilder().priority(500).inPort(uplinkPort)
                .tci(tci, mask)
                .action().output(accessPort)
                .parent().build(el);
        }

        // Bypass conntrack from endpoint reaching its
        // floating ips.
        for(const Endpoint::IPAddressMapping& ipm :
                ep->getIPAddressMappings()) {
            if (!ipm.getMappedIP() || !ipm.getEgURI())
                continue;

	    boost::system::error_code ec;
            address mappedIp =
                address::from_string(ipm.getMappedIP().get(), ec);
            if (ec) continue;

            address floatingIp;
            if (ipm.getFloatingIP()) {
                floatingIp =
                    address::from_string(ipm.getFloatingIP().get(), ec);
                if (ec) continue;
                if (floatingIp.is_v4() != mappedIp.is_v4()) continue;
                if (floatingIp.is_unspecified()) continue;
            }
            flowBypassFloatingIP(el, accessPort, uplinkPort, false,
                                 false, floatingIp, ep);
            flowBypassFloatingIP(el, uplinkPort, accessPort, true,
                                 false, floatingIp, ep);
            /*
             * We allow both with / without tags to handle Openshift
             * bootstrap
             */
            if (ep->isAccessAllowUntagged() && ep->getAccessIfaceVlan()) {
                flowBypassFloatingIP(el, accessPort, uplinkPort, false,
                                     true, floatingIp, ep);
                flowBypassFloatingIP(el, uplinkPort, accessPort, true,
                                     true, floatingIp, ep);
            }
        }
    }
    switchManager.writeFlow(uuid, GROUP_MAP_TABLE_ID, el);
}

void AccessFlowManager::handleDropLogPortUpdate() {
    FlowEntryList catchDropFlows;
    if(dropLogIface.empty() || !dropLogDst.is_v4()) {
        switchManager.clearFlows("static", EXP_DROP_TABLE_ID);
        LOG(WARNING) << "Ignoring dropLog port " << dropLogIface
        << " " << dropLogDst;
        return;
    }
    int dropLogPort = switchManager.getPortMapper().FindPort(dropLogIface);
    if(dropLogPort != OFPP_NONE) {
        FlowBuilder().priority(0)
                .metadata(flow::meta::DROP_LOG, flow::meta::DROP_LOG)
                .action()
                .reg(MFF_TUN_DST, dropLogDst.to_v4().to_ulong())
                .output(dropLogPort)
                .parent().build(catchDropFlows);
        switchManager.writeFlow("static", EXP_DROP_TABLE_ID, catchDropFlows);
    }
}

void AccessFlowManager::handlePortStatusUpdate(const string& portName,
                                               uint32_t) {
    unordered_set<std::string> eps;
    LOG(DEBUG) << "Port-status update for " << portName;
    agent.getEndpointManager().getEndpointsByAccessIface(portName, eps);
    agent.getEndpointManager().getEndpointsByAccessUplink(portName, eps);
    for (const std::string& ep : eps)
        endpointUpdated(ep);
    if(portName == dropLogIface) {
        handleDropLogPortUpdate();
    }
}

void AccessFlowManager::handleSecGrpUpdate(const opflex::modb::URI& uri) {
    unordered_set<uri_set_t> secGrpSets;
    agent.getEndpointManager().getSecGrpSetsForSecGrp(uri, secGrpSets);
    for (const uri_set_t& secGrpSet : secGrpSets)
        secGroupSetUpdated(secGrpSet);
}

void AccessFlowManager::handleSecGrpSetUpdate(const uri_set_t& secGrps,
                                              const string& secGrpsIdStr) {
    using modelgbp::gbpe::L24Classifier;
    using modelgbp::gbp::DirectionEnumT;
    using modelgbp::gbp::ConnTrackEnumT;
    using flowutils::CA_REFLEX_REV_ALLOW;
    using flowutils::CA_REFLEX_REV_TRACK;
    using flowutils::CA_REFLEX_FWD;
    using flowutils::CA_ALLOW;
    using flowutils::CA_REFLEX_FWD_TRACK;
    using flowutils::CA_REFLEX_FWD_EST;
    using flowutils::CA_REFLEX_REV_RELATED;

    LOG(DEBUG) << "Updating security group set \"" << secGrpsIdStr << "\"";

    if (agent.getEndpointManager().secGrpSetEmpty(secGrps)) {
        switchManager.clearFlows(secGrpsIdStr, SEC_GROUP_IN_TABLE_ID);
        switchManager.clearFlows(secGrpsIdStr, SEC_GROUP_OUT_TABLE_ID);
        return;
    }

    uint32_t secGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET, secGrpsIdStr);

    FlowEntryList secGrpIn;
    FlowEntryList secGrpOut;

    for (const opflex::modb::URI& secGrp : secGrps) {
        PolicyManager::rule_list_t rules;
        agent.getPolicyManager().getSecGroupRules(secGrp, rules);

        for (shared_ptr<PolicyRule>& pc : rules) {
            uint8_t dir = pc->getDirection();
            const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
            const URI& ruleURI = cls.get()->getURI();
            uint64_t secGrpCookie =
                idGen.getId("l24classifierRule", ruleURI.toString());
            boost::optional<const network::subnets_t&> remoteSubs;
            if (!pc->getRemoteSubnets().empty())
                remoteSubs = pc->getRemoteSubnets();

            flowutils::ClassAction act = flowutils::CA_DENY;
            if (pc->getAllow()) {
                if (cls->getConnectionTracking(ConnTrackEnumT::CONST_NORMAL) ==
                    ConnTrackEnumT::CONST_REFLEXIVE) {
                    act = CA_REFLEX_FWD;
                } else {
                    act = CA_ALLOW;
                }
            }

            if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                dir == DirectionEnumT::CONST_IN) {
                flowutils::add_classifier_entries(*cls, act,
                                                  remoteSubs,
                                                  boost::none,
                                                  OUT_TABLE_ID,
                                                  pc->getPriority(),
                                                  OFPUTIL_FF_SEND_FLOW_REM,
                                                  secGrpCookie,
                                                  secGrpSetId, 0,
                                                  secGrpIn);
                if (act == CA_REFLEX_FWD) {
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_FWD_TRACK,
                                                      remoteSubs,
                                                      boost::none,
                                                      GROUP_MAP_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpIn);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_FWD_EST,
                                                      remoteSubs,
                                                      boost::none,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpIn);
                    // add reverse entries for reflexive classifier
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_TRACK,
                                                      boost::none,
                                                      remoteSubs,
                                                      GROUP_MAP_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      0,
                                                      secGrpSetId, 0,
                                                      secGrpOut);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_ALLOW,
                                                      boost::none,
                                                      remoteSubs,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpOut);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_RELATED,
                                                      boost::none,
                                                      remoteSubs,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpOut);
                }
            }
            if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                dir == DirectionEnumT::CONST_OUT) {
                flowutils::add_classifier_entries(*cls, act,
                                                  boost::none,
                                                  remoteSubs,
                                                  OUT_TABLE_ID,
                                                  pc->getPriority(),
                                                  OFPUTIL_FF_SEND_FLOW_REM,
                                                  secGrpCookie,
                                                  secGrpSetId, 0,
                                                  secGrpOut);
                if (act == CA_REFLEX_FWD) {
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_FWD_TRACK,
                                                      boost::none,
                                                      remoteSubs,
                                                      GROUP_MAP_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpOut);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_FWD_EST,
                                                      boost::none,
                                                      remoteSubs,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpOut);
                    // add reverse entries for reflexive classifier
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_TRACK,
                                                      remoteSubs,
                                                      boost::none,
                                                      GROUP_MAP_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      0,
                                                      secGrpSetId, 0,
                                                      secGrpIn);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_ALLOW,
                                                      remoteSubs,
                                                      boost::none,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpIn);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_RELATED,
                                                      remoteSubs,
                                                      boost::none,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
                                                      OFPUTIL_FF_SEND_FLOW_REM,
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpIn);
                }
            }
        }
    }

    switchManager.writeFlow(secGrpsIdStr, SEC_GROUP_IN_TABLE_ID, secGrpIn);
    switchManager.writeFlow(secGrpsIdStr, SEC_GROUP_OUT_TABLE_ID, secGrpOut);
}

void AccessFlowManager::lbIfaceUpdated(const std::string& uuid) {
    LOG(DEBUG) << "Updating learning bridge interface " << uuid;

    LearningBridgeManager& lbMgr = agent.getLearningBridgeManager();
    shared_ptr<const LearningBridgeIface> iface = lbMgr.getLBIface(uuid);

    if (!iface)
        return;

    if (iface->getInterfaceName()) {
        EndpointManager& epMgr = agent.getEndpointManager();
        std::unordered_set<std::string> epUuids;
        epMgr.getEndpointsByIface(iface->getInterfaceName().get(), epUuids);

        for (auto& uuid : epUuids) {
            endpointUpdated(uuid);
        }
    }
}

void AccessFlowManager::rdConfigUpdated(const opflex::modb::URI& rdURI) {
//Interface not used
}

void AccessFlowManager::packetDropLogConfigUpdated(const opflex::modb::URI& dropLogCfgURI) {
    using modelgbp::observer::DropLogConfig;
    using modelgbp::observer::DropLogModeEnumT;
    FlowEntryList dropLogFlows;
    optional<shared_ptr<DropLogConfig>> dropLogCfg =
            DropLogConfig::resolve(agent.getFramework(), dropLogCfgURI);
    if(!dropLogCfg) {
        FlowBuilder().priority(2)
                .action().go(GROUP_MAP_TABLE_ID)
                .parent().build(dropLogFlows);
        switchManager.writeFlow("DropLogConfig", DROP_LOG_TABLE_ID, dropLogFlows);
        LOG(INFO) << "Defaulting to droplog disabled";
        return;
    }
    if(dropLogCfg.get()->getDropLogEnable(0) != 0) {
        if(dropLogCfg.get()->getDropLogMode(
                    DropLogModeEnumT::CONST_UNFILTERED_DROP_LOG) ==
           DropLogModeEnumT::CONST_UNFILTERED_DROP_LOG) {
            FlowBuilder().priority(2)
                    .action()
                    .metadata(flow::meta::DROP_LOG,
                              flow::meta::DROP_LOG)
                    .go(GROUP_MAP_TABLE_ID)
                    .parent().build(dropLogFlows);
            LOG(INFO) << "Droplog mode set to unfiltered";
        } else {
            switchManager.clearFlows("DropLogConfig", DROP_LOG_TABLE_ID);
            LOG(INFO) << "Droplog mode set to filtered";
            return;
        }
    } else {
        FlowBuilder().priority(2)
                .action()
                .go(GROUP_MAP_TABLE_ID)
                .parent().build(dropLogFlows);
        LOG(INFO) << "Droplog disabled";
    }
    switchManager.writeFlow("DropLogConfig", DROP_LOG_TABLE_ID, dropLogFlows);
}

void AccessFlowManager::packetDropFlowConfigUpdated(const opflex::modb::URI& dropFlowCfgURI) {
    using modelgbp::observer::DropFlowConfig;
    optional<shared_ptr<DropFlowConfig>> dropFlowCfg =
            DropFlowConfig::resolve(agent.getFramework(), dropFlowCfgURI);
    if(!dropFlowCfg) {
        switchManager.clearFlows(dropFlowCfgURI.toString(), DROP_LOG_TABLE_ID);
        return;
    }
    FlowEntryList dropLogFlows;
    FlowBuilder fb;
    fb.priority(1);
    if(dropFlowCfg.get()->isEthTypeSet()) {
        fb.ethType(dropFlowCfg.get()->getEthType(0));
    }
    if(dropFlowCfg.get()->isInnerSrcAddressSet()) {
	address addr = address::from_string(
                dropFlowCfg.get()->getInnerSrcAddress(""));
        fb.ipSrc(addr);
    }
    if(dropFlowCfg.get()->isInnerDstAddressSet()) {
        address addr = address::from_string(
                dropFlowCfg.get()->getInnerDstAddress(""));
        fb.ipDst(addr);
    }
    if(dropFlowCfg.get()->isOuterSrcAddressSet()) {
        address addr = address::from_string(
                dropFlowCfg.get()->getOuterSrcAddress(""));
        fb.outerIpSrc(addr);
    }
    if(dropFlowCfg.get()->isOuterDstAddressSet()) {
        address addr = address::from_string(
                dropFlowCfg.get()->getOuterDstAddress(""));
        fb.outerIpDst(addr);
    }
    if(dropFlowCfg.get()->isTunnelIdSet()) {
        fb.tunId(dropFlowCfg.get()->getTunnelId(0));
    }
    if(dropFlowCfg.get()->isIpProtoSet()) {
        fb.proto(dropFlowCfg.get()->getIpProto(0));
    }
    if(dropFlowCfg.get()->isSrcPortSet()) {
        fb.tpSrc(dropFlowCfg.get()->getSrcPort(0));
    }
    if(dropFlowCfg.get()->isDstPortSet()) {
        fb.tpDst(dropFlowCfg.get()->getDstPort(0));
    }
    fb.action().metadata(flow::meta::DROP_LOG, flow::meta::DROP_LOG)
            .go(GROUP_MAP_TABLE_ID).parent().build(dropLogFlows);
    switchManager.writeFlow(dropFlowCfgURI.toString(), DROP_LOG_TABLE_ID,
            dropLogFlows);
}

static bool secGrpSetIdGarbageCb(EndpointManager& endpointManager,
                                 const string& str) {
    uri_set_t secGrps;

    typedef boost::algorithm::split_iterator<string::const_iterator> ssi;
    ssi it = make_split_iterator(str, token_finder(is_any_of(",")));

    for(; it != ssi(); ++it) {
        string uri(copy_range<string>(*it));
        if (!uri.empty())
            secGrps.insert(URI(uri));
    }

    if (secGrps.empty()) return true;
    return !endpointManager.secGrpSetEmpty(secGrps);
}

void AccessFlowManager::cleanup() {
    using namespace modelgbp::gbp;

    auto gcb1 = [=](const std::string& ns,
                    const std::string& str) -> bool {
        return IdGenerator::uriIdGarbageCb<SecGroup>(agent.getFramework(),
                                                     ns, str);
    };
    idGen.collectGarbage(ID_NMSPC_SECGROUP, gcb1);

    auto gcb2 = [=](const std::string&,
                    const std::string& str) -> bool {
        return secGrpSetIdGarbageCb(agent.getEndpointManager(), str);
    };
    idGen.collectGarbage(ID_NMSPC_SECGROUP_SET, gcb2);
}

} // namespace opflexagent
