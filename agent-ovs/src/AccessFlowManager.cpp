/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AccessFlowManager.h"
#include "FlowBuilder.h"
#include "FlowUtils.h"
#include "logging.h"

#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/sub_range.hpp>

#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>

#include <string>
#include <vector>
#include <sstream>

namespace ovsagent {

using std::string;
using std::vector;
using opflex::modb::URI;
typedef EndpointListener::uri_set_t uri_set_t;
using boost::algorithm::make_split_iterator;
using boost::algorithm::token_finder;
using boost::algorithm::is_any_of;
using boost::copy_range;
using boost::unordered_set;
using boost::shared_ptr;
using boost::optional;

static const char* ID_NAMESPACES[] =
    {"secGroup", "secGroupSet"};

static const char* ID_NMSPC_SECGROUP     = ID_NAMESPACES[0];
static const char* ID_NMSPC_SECGROUP_SET = ID_NAMESPACES[1];

static const std::string ID_NMSPC_ENDPOINT("endpoint");

AccessFlowManager::AccessFlowManager(Agent& agent_,
                                     SwitchManager& switchManager_,
                                     IdGenerator& idGen_)
    : agent(agent_), switchManager(switchManager_), idGen(idGen_),
      ctZoneManager(idGen_), taskQueue(agent.getAgentIOService()),
      conntrackEnabled(false), stopping(false) {
    // set up flow tables
    switchManager.setMaxFlowTables(NUM_FLOW_TABLES);
}

static string getSecGrpSetId(const uri_set_t& secGrps) {
   std::stringstream ss;
   bool notfirst = false;
   BOOST_FOREACH(const URI& uri, secGrps) {
       if (notfirst) ss << ",";
       notfirst = true;
       ss << uri.toString();
   }
   return ss.str();
}

void AccessFlowManager::enableConnTrack(uint16_t minId, uint16_t maxId,
                                        bool useNetLink) {
    conntrackEnabled = true;
    ctZoneManager.setCtZoneRange(minId, maxId);
    ctZoneManager.enableNetLink(useNetLink);
}

void AccessFlowManager::start() {
    if (conntrackEnabled)
        ctZoneManager.init(ID_NMSPC_ENDPOINT);

    switchManager.getPortMapper().registerPortStatusListener(this);
    agent.getEndpointManager().registerListener(this);
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
    agent.getPolicyManager().unregisterListener(this);
}

void AccessFlowManager::endpointUpdated(const string& uuid) {
    if (stopping) return;
    taskQueue.dispatch(uuid,
                       boost::bind(&AccessFlowManager::handleEndpointUpdate,
                                   this, uuid));
}

void AccessFlowManager::secGroupSetUpdated(const uri_set_t& secGrps) {
    if (stopping) return;
    const string id = getSecGrpSetId(secGrps);
    taskQueue.dispatch(id,
                       boost::bind(&AccessFlowManager::handleSecGrpSetUpdate,
                                   this, secGrps, id));
}

void AccessFlowManager::configUpdated(const opflex::modb::URI& configURI) {
    if (stopping) return;
    switchManager.enableSync();
}

void AccessFlowManager::secGroupUpdated(const opflex::modb::URI& uri) {
    if (stopping) return;
    taskQueue.dispatch(uri.toString(),
                       boost::bind(&AccessFlowManager::handleSecGrpUpdate,
                                   this, uri));
}

void AccessFlowManager::portStatusUpdate(const string& portName,
                                         uint32_t portNo, bool) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(boost::bind(&AccessFlowManager::handlePortStatusUpdate,
                              this, portName, portNo));
}

static FlowEntryPtr flowEmptySecGroup(uint32_t emptySecGrpSetId) {
    FlowBuilder noSecGrp;
    flowutils::match_group(noSecGrp,
                           flowutils::MAX_POLICY_RULE_PRIORITY,
                           emptySecGrpSetId, 0);
    noSecGrp.action().go(AccessFlowManager::OUT_TABLE_ID);
    return noSecGrp.build();
}

void AccessFlowManager::createStaticFlows() {
    LOG(DEBUG) << "Writing static flows";
    switchManager.writeFlow("static", OUT_TABLE_ID,
                            flowutils::default_out_flow());

    // everything is allowed for endpoints with no security group set
    uint32_t emptySecGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET, "");
    switchManager.writeFlow("static", SEC_GROUP_OUT_TABLE_ID,
                            flowEmptySecGroup(emptySecGrpSetId));
    switchManager.writeFlow("static", SEC_GROUP_IN_TABLE_ID,
                            flowEmptySecGroup(emptySecGrpSetId));

    // Restore flow state
    //switchManager.writeFlow("static", CT_RESTORE_TABLE_ID,
    //                        FlowBuilder()
    //                        .conntrackState(FlowBuilder::CT_STATE_TRACKED,
    //                                        FlowBuilder::CT_STATE_TRACKED)
    //                        .action()
    //                        .regMove(MFF_CT_LABEL, MFF_REG0, 0, 0, 32)
    //                        .regMove(MFF_CT_LABEL, MFF_REG6, 32, 0, 32)
    //                        .regMove(MFF_CT_LABEL, MFF_REG7, 64, 0, 32)
    //                        .go(SEC_GROUP_IN_TABLE_ID)
    //                        .parent().build());
    switchManager.writeFlow("static", GROUP_MAP_TABLE_ID,
                            FlowBuilder().priority(1).build());
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

    FlowEntryList el;
    if (accessPort != OFPP_NONE && uplinkPort != OFPP_NONE) {
        {
            FlowBuilder in;
            in.priority(100).inPort(accessPort);
            if (ep->getAccessIfaceVlan()) {
                in.vlan(ep->getAccessIfaceVlan().get());
                in.action().popVlan();
            }
            if (zoneId != static_cast<uint16_t>(-1))
                in.action()
                    .reg(MFF_REG6, zoneId);

            in.action()
                .reg(MFF_REG0, secGrpSetId)
                .reg(MFF_REG7, uplinkPort)
                .go(SEC_GROUP_OUT_TABLE_ID);
            in.build(el);
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
                    .pushVlan()
                    .setVlanVid(ep->getAccessIfaceVlan().get());
            }
            out.action().go(SEC_GROUP_IN_TABLE_ID);
            out.build(el);
        }
    }
    switchManager.writeFlow(uuid, GROUP_MAP_TABLE_ID, el);
}

void AccessFlowManager::handlePortStatusUpdate(const string& portName,
                                               uint32_t) {
    unordered_set<std::string> eps;
    agent.getEndpointManager().getEndpointsByAccessIface(portName, eps);
    agent.getEndpointManager().getEndpointsByAccessUplink(portName, eps);
    BOOST_FOREACH(const std::string& ep, eps)
        endpointUpdated(ep);
}

void AccessFlowManager::handleSecGrpUpdate(const opflex::modb::URI& uri) {
    unordered_set<uri_set_t> secGrpSets;
    agent.getEndpointManager().getSecGrpSetsForSecGrp(uri, secGrpSets);
    BOOST_FOREACH(const uri_set_t& secGrpSet, secGrpSets)
        secGroupSetUpdated(secGrpSet);
}

void AccessFlowManager::handleSecGrpSetUpdate(const uri_set_t& secGrps,
                                              const string& secGrpsIdStr) {
    using modelgbp::gbpe::L24Classifier;
    using modelgbp::gbp::DirectionEnumT;
    using modelgbp::gbp::ConnTrackEnumT;
    using flowutils::CA_REFLEX_REV_ALLOW;
    using flowutils::CA_REFLEX_REV;
    using flowutils::CA_REFLEX_FWD;
    using flowutils::CA_ALLOW;

    LOG(DEBUG) << "Updating security group set \"" << secGrpsIdStr << "\"";

    if (agent.getEndpointManager().secGrpSetEmpty(secGrps)) {
        switchManager.clearFlows(secGrpsIdStr, SEC_GROUP_IN_TABLE_ID);
        switchManager.clearFlows(secGrpsIdStr, SEC_GROUP_OUT_TABLE_ID);
        return;
    }

    uint32_t secGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET, secGrpsIdStr);

    FlowEntryList secGrpIn;
    FlowEntryList secGrpOut;

    //ActionBuilder ctSave;
    //ctSave.regMove(MFF_REG0, MFF_CT_LABEL, 0, 0, 32)
    //    .regMove(MFF_REG6, MFF_CT_LABEL, 0, 32, 32)
    //    .regMove(MFF_REG7, MFF_CT_LABEL, 0, 64, 32);

    BOOST_FOREACH(const opflex::modb::URI& secGrp, secGrps) {
        PolicyManager::rule_list_t rules;
        uint64_t secGrpCookie =
            idGen.getId(ID_NMSPC_SECGROUP, secGrp.toString());
        agent.getPolicyManager().getSecGroupRules(secGrp, rules);

        BOOST_FOREACH(shared_ptr<PolicyRule>& pc, rules) {
            uint8_t dir = pc->getDirection();
            const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();

            boost::optional<const flowutils::subnets_t&> remoteSubs;
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
                                                  secGrpCookie,
                                                  secGrpSetId, 0,
                                                  secGrpIn);
                if (act == CA_REFLEX_FWD) {
                    // add reverse entries for reflexive classifier
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV,
                                                      boost::none,
                                                      remoteSubs,
                                                      GROUP_MAP_TABLE_ID,
                                                      pc->getPriority(),
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpOut);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_ALLOW,
                                                      boost::none,
                                                      remoteSubs,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
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
                                                  secGrpCookie,
                                                  secGrpSetId, 0,
                                                  secGrpOut);
                if (act == CA_REFLEX_FWD) {
                    // add reverse entries for reflexive classifier
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV,
                                                      remoteSubs,
                                                      boost::none,
                                                      GROUP_MAP_TABLE_ID,
                                                      pc->getPriority(),
                                                      secGrpCookie,
                                                      secGrpSetId, 0,
                                                      secGrpIn);
                    flowutils::add_classifier_entries(*cls, CA_REFLEX_REV_ALLOW,
                                                      remoteSubs,
                                                      boost::none,
                                                      OUT_TABLE_ID,
                                                      pc->getPriority(),
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

static bool secGrpSetIdGarbageCb(EndpointManager& endpointManager,
                                 const string&, const string& str) {
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
    IdGenerator::garbage_cb_t gcb1 =
        boost::bind(IdGenerator::uriIdGarbageCb<modelgbp::gbp::SecGroup>,
                    boost::ref(agent.getFramework()), _1, _2);
    idGen.collectGarbage(ID_NMSPC_SECGROUP, gcb1);

    IdGenerator::garbage_cb_t gcb2 =
        boost::bind(secGrpSetIdGarbageCb,
                    boost::ref(agent.getEndpointManager()), _1, _2);
    idGen.collectGarbage(ID_NMSPC_SECGROUP_SET, gcb2);
}

} // namespace ovsagent
