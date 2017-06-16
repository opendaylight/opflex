/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PolicyStatsManager class.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "logging.h"
#include "IntFlowManager.h"
#include "IdGenerator.h"
#include "Agent.h"
#include "TableState.h"

#include "ovs-ofputil.h"
#include <lib/util.h>
#include "PolicyStatsManager.h"

extern "C" {
#include <openvswitch/ofp-msgs.h>
}

#include <opflex/modb/URI.h>
#include <opflex/modb/Mutator.h>
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <modelgbp/gbpe/L24ClassifierCounter.hpp>
#include <modelgbp/observer/PolicyStatUniverse.hpp>

namespace ovsagent {

using std::string;
using std::unordered_map;
using boost::optional;
using boost::make_optional;
using std::shared_ptr;
using opflex::modb::URI;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using namespace modelgbp::gbp;
using namespace modelgbp::observer;
using namespace modelgbp::gbpe;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;
using std::bind;
using boost::system::error_code;

static const int MAX_AGE = 3;

PolicyStatsManager::PolicyStatsManager(Agent* agent_, IdGenerator& idGen_,
                                       SwitchManager& switchManager_,
                                       long timer_interval_)
    : BaseStatsManager(agent_,idGen_,switchManager_,timer_interval_) {
      std::random_device rng;
    std::mt19937 urng(rng());
      agentUUID = to_string(basic_random_generator<std::mt19937>(urng)());
      table_ids[0] = IntFlowManager::POL_TABLE_ID;
      
}

PolicyStatsManager::~PolicyStatsManager() {

}
void PolicyStatsManager::start() {
    LOG(DEBUG) << "Starting policy stats manager";
    BaseStatsManager::start();
    timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
}

void PolicyStatsManager::stop() {
    LOG(DEBUG) << "Stopping policy stats manager";
    BaseStatsManager::stop();
}

void PolicyStatsManager::updatePolicyFlowEntryMap(uint64_t cookie,
                                             uint16_t priority,
                                            const struct match& match) {
    updateFlowEntryMap(cookie, priority,0,match);
}

void PolicyStatsManager::on_timer(const error_code& ec) {

    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    TableState::cookie_callback_t cb_func;
    cb_func = std::bind(&PolicyStatsManager::updatePolicyFlowEntryMap, this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3);

    // Request Switch Manager to provide flow entries

    std::lock_guard<std::mutex> lock(pstatMtx);
    switchManager.forEachCookieMatch(IntFlowManager::POL_TABLE_ID,
                                     cb_func);

    PolicyCounterMap_t newClassCountersMap;
    on_timer_base(ec,0,newClassCountersMap);

    generatePolicyStatsObjects(&newClassCountersMap);

    sendRequest(IntFlowManager::POL_TABLE_ID);
    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
    }
}

void PolicyStatsManager::handleDropStats(uint32_t rdId,
                                         boost::optional<std::string> idStr,
                                         struct ofputil_flow_stats* fentry) {

    PolicyDropCounters_t newCounters;
    newCounters.packet_count = make_optional(true, fentry->packet_count);
    newCounters.byte_count = make_optional(true, fentry->byte_count);
    PolicyDropCounters_t& oldCounters = policyDropCountersMap[rdId];
    PolicyDropCounters_t diffCounters;

    if (oldCounters.packet_count) {
        diffCounters.packet_count = newCounters.packet_count.get() -
            oldCounters.packet_count.get();
        diffCounters.byte_count = newCounters.byte_count.get() -
            oldCounters.byte_count.get();
    } else {
        diffCounters = newCounters;
    }

    // Store flow stats for the routing domain
    // in the policyDropCountersMap.
    oldCounters.packet_count = newCounters.packet_count;
    oldCounters.byte_count = newCounters.byte_count;

    if (diffCounters.packet_count)
        updatePolicyStatsDropCounters(idStr.get(),
                                      diffCounters);
}

void PolicyStatsManager::
updatePolicyStatsCounters(const std::string& srcEpg,
                          const std::string& dstEpg,
                          const std::string& l24Classifier,
                          PolicyCounters_t& newVals) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        su.get()->addGbpeL24ClassifierCounter(getAgentUUID(),
                                              getNextClsfrGenId(),
                                              srcEpg, dstEpg, l24Classifier)
            ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }

    mutator.commit();
}

void PolicyStatsManager::
updatePolicyStatsDropCounters(const std::string& rdStr,
                              PolicyDropCounters_t& newVals) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        su.get()->addGbpeRoutingDomainDropCounter(getAgentUUID(),
                                                  getNextDropGenId(), rdStr)
            ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }


    mutator.commit();
}

} /* namespace ovsagent */
