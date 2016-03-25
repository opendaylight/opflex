/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AccessFlowManager.h"
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

static const char ID_NMSPC_SECGROUP_SET[] = "secGroupSet";

AccessFlowManager::AccessFlowManager(Agent& agent_,
                                     SwitchManager& switchManager_,
                                     IdGenerator& idGen_)
    : agent(agent_), switchManager(switchManager_), idGen(idGen_),
      taskQueue(agent.getAgentIOService()), stopping(false) {
    // set up flow tables
    switchManager.setMaxFlowTables(NUM_FLOW_TABLES);
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
    std::stringstream ss;
    bool notfirst = false;;
    BOOST_FOREACH(const URI& uri, secGrps) {
        if (notfirst) ss << ",";
        notfirst = true;
        ss << uri.toString();
    }
    const string& id = ss.str();
    taskQueue.dispatch(id,
                       boost::bind(&AccessFlowManager::handleSecGrpUpdate,
                                   this, secGrps, id));
}

void AccessFlowManager::handleEndpointUpdate(const string& uuid) {
    LOG(DEBUG) << "Updating endpoint " << uuid;
    // TODO
}

void AccessFlowManager::handleSecGrpUpdate(const uri_set_t& secGrps,
                                           const string& secGrpsId) {
    LOG(DEBUG) << "Updating security group set " << secGrpsId;
    // TODO

}

void AccessFlowManager::portStatusUpdate(const string& portName,
                                         uint32_t portNo, bool) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(boost::bind(&AccessFlowManager::handlePortStatusUpdate,
                              this, portName, portNo));
}

void AccessFlowManager::handlePortStatusUpdate(const string& portName,
                                               uint32_t portNo) {
    // TODO
}

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
