/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AccessFlowManager.h"
#include "ActionBuilder.h"
#include "logging.h"

#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/sub_range.hpp>

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

static const char ID_NMSPC_SECGROUP_SET[] = "secGroupSet";

AccessFlowManager::AccessFlowManager(Agent& agent_,
                                     SwitchManager& switchManager_,
                                     IdGenerator& idGen_)
    : agent(agent_), switchManager(switchManager_), idGen(idGen_),
      taskQueue(agent.getAgentIOService()), stopping(false) {
    // set up flow tables
    switchManager.setMaxFlowTables(NUM_FLOW_TABLES);
}

static string getSecGrpId(const uri_set_t& secGrps) {
   std::stringstream ss;
   bool notfirst = false;;
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
    idGen.initNamespace(ID_NMSPC_SECGROUP_SET);
}

void AccessFlowManager::stop() {
    stopping = true;
    switchManager.getPortMapper().unregisterPortStatusListener(this);
    agent.getEndpointManager().unregisterListener(this);
}

void AccessFlowManager::endpointUpdated(const string& uuid) {
    if (stopping) return;
    taskQueue.dispatch(uuid,
                       boost::bind(&AccessFlowManager::handleEndpointUpdate,
                                   this, uuid));
}

void AccessFlowManager::secGroupSetUpdated(const uri_set_t& secGrps) {
    if (stopping) return;
    const string id = getSecGrpId(secGrps);
    taskQueue.dispatch(id,
                       boost::bind(&AccessFlowManager::handleSecGrpUpdate,
                                   this, secGrps, id));
}

void AccessFlowManager::portStatusUpdate(const string& portName,
                                         uint32_t portNo, bool) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(boost::bind(&AccessFlowManager::handlePortStatusUpdate,
                              this, portName, portNo));
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
                                       getSecGrpId(ep->getSecurityGroups()));

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

void AccessFlowManager::handleSecGrpUpdate(const uri_set_t& secGrps,
                                           const string& secGrpsId) {
    LOG(DEBUG) << "Updating security group set " << secGrpsId;
    unordered_set<std::string> eps;
    agent.getEndpointManager().getEndpointsForSecGrps(secGrps, eps);

    if (eps.empty()) {
        // TODO
        return;
    }

    // TODO
}

// TODO: handle updates to rules in security groups

static bool idGarbageCb(EndpointManager& endpointManager,
                        const string&, const string& str) {
    uri_set_t secGrps;

    typedef boost::algorithm::split_iterator<string::const_iterator> ssi;
    ssi it = make_split_iterator(str, token_finder(is_any_of(",")));

    for(; it != ssi(); ++it) {
        secGrps.insert(URI(copy_range<string>(*it)));
    }
    return !endpointManager.secGrpSetEmpty(secGrps);
}

void AccessFlowManager::cleanup() {
    IdGenerator::garbage_cb_t gcb =
        bind(idGarbageCb, boost::ref(agent.getEndpointManager()), _1, _2);
    agent.getAgentIOService()
        .dispatch(bind(&IdGenerator::collectGarbage, boost::ref(idGen),
                       ID_NMSPC_SECGROUP_SET, gcb));
}

} // namespace ovsagent
