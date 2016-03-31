/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AccessFlowManager.h"
#include "ActionBuilder.h"
#include "FlowUtils.h"
#include "logging.h"

#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/sub_range.hpp>

#include <modelgbp/gbp/DirectionEnumT.hpp>

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

AccessFlowManager::AccessFlowManager(Agent& agent_,
                                     SwitchManager& switchManager_,
                                     IdGenerator& idGen_)
    : agent(agent_), switchManager(switchManager_), idGen(idGen_),
      taskQueue(agent.getAgentIOService()), stopping(false) {
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

void AccessFlowManager::start() {
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

static FlowEntryPtr emptySecGroupFlow(uint32_t emptySecGrpSetId) {
    FlowEntryPtr noSecGrp(new FlowEntry());
    flowutils::set_match_group(*noSecGrp,
                               flowutils::MAX_POLICY_RULE_PRIORITY,
                               emptySecGrpSetId, 0);
    ActionBuilder ab;
    ab.SetGotoTable(AccessFlowManager::OUT_TABLE_ID);
    ab.Build(noSecGrp->entry);
    return noSecGrp;
}

void AccessFlowManager::createStaticFlows() {
    LOG(DEBUG) << "Writing static flows";
    switchManager.writeFlow("static", OUT_TABLE_ID,
                            flowutils::default_out_flow());

    // everything is allowed for endpoints with no security group set
    uint32_t emptySecGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET, "");
    switchManager.writeFlow("static", SEC_GROUP_OUT_TABLE_ID,
                            emptySecGroupFlow(emptySecGrpSetId));
    switchManager.writeFlow("static", SEC_GROUP_IN_TABLE_ID,
                            emptySecGroupFlow(emptySecGrpSetId));
}

static FlowEntryPtr epFlow(uint32_t inPort, uint32_t nextHop,
                           uint32_t secGrpSetId, uint8_t nextTable) {
    FlowEntry* flow = new FlowEntry();
    flow->entry->table_id = AccessFlowManager::GROUP_MAP_TABLE_ID;
    flow->entry->priority = 100;
    match_set_in_port(&flow->entry->match, inPort);

    ActionBuilder ab;
    ab.SetRegLoad(MFF_REG0, secGrpSetId);
    ab.SetRegLoad(MFF_REG7, nextHop);
    ab.SetGotoTable(nextTable);
    ab.Build(flow->entry);
    return FlowEntryPtr(flow);
}

void AccessFlowManager::handleEndpointUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating endpoint " << uuid;
    shared_ptr<const Endpoint> ep =
        agent.getEndpointManager().getEndpoint(uuid);
    if (!ep) {
        switchManager.writeFlow(uuid, GROUP_MAP_TABLE_ID, NULL);
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

    FlowEntryList el;
    if (accessPort != OFPP_NONE && uplinkPort != OFPP_NONE) {
        el.push_back(epFlow(accessPort, uplinkPort,
                            secGrpSetId, SEC_GROUP_OUT_TABLE_ID));
        el.push_back(epFlow(uplinkPort, accessPort,
                            secGrpSetId, SEC_GROUP_IN_TABLE_ID));
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

    LOG(DEBUG) << "Updating security group set \"" << secGrpsIdStr << "\"";

    if (agent.getEndpointManager().secGrpSetEmpty(secGrps)) {
        switchManager.writeFlow(secGrpsIdStr, SEC_GROUP_IN_TABLE_ID, NULL);
        switchManager.writeFlow(secGrpsIdStr, SEC_GROUP_OUT_TABLE_ID, NULL);
        return;
    }

    uint32_t secGrpSetId = idGen.getId(ID_NMSPC_SECGROUP_SET, secGrpsIdStr);

    FlowEntryList secGrpIn;
    FlowEntryList secGrpOut;

    uint16_t prio = flowutils::MAX_POLICY_RULE_PRIORITY;
    BOOST_FOREACH(const opflex::modb::URI& secGrp, secGrps) {
        PolicyManager::rule_list_t rules;
        uint64_t secGrpCookie =
            idGen.getId(ID_NMSPC_SECGROUP, secGrp.toString());
        agent.getPolicyManager().getSecGroupRules(secGrp, rules);

        BOOST_FOREACH(shared_ptr<PolicyRule>& pc, rules) {
            uint8_t dir = pc->getDirection();
            const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
            if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                dir == DirectionEnumT::CONST_IN) {
                flowutils::add_classifier_entries(cls.get(),
                                                  pc->getAllow(),
                                                  OUT_TABLE_ID,
                                                  prio, secGrpCookie,
                                                  secGrpSetId, 0,
                                                  secGrpIn);
            }
            if (dir == DirectionEnumT::CONST_BIDIRECTIONAL ||
                dir == DirectionEnumT::CONST_OUT) {
                flowutils::add_classifier_entries(cls.get(),
                                                  pc->getAllow(),
                                                  OUT_TABLE_ID,
                                                  prio, secGrpCookie,
                                                  secGrpSetId, 0,
                                                  secGrpOut);
            }
            --prio;
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
        secGrps.insert(URI(copy_range<string>(*it)));
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
