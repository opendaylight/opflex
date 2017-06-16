/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SecGrpStatsManager class.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "logging.h"
#include "AccessFlowManager.h"
#include "IntFlowManager.h"
#include "IdGenerator.h"
#include "Agent.h"
#include "TableState.h"

#include "ovs-ofputil.h"
#include <lib/util.h>
#include "SecGrpStatsManager.h"

extern "C" {
#include <openvswitch/ofp-msgs.h>
}

#include <opflex/modb/URI.h>
#include <opflex/modb/Mutator.h>
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <modelgbp/gbpe/HPPClassifierCounter.hpp>
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
typedef EndpointListener::uri_set_t uri_set_t;
static const int MAX_AGE = 3;

SecGrpStatsManager::SecGrpStatsManager(Agent* agent_, IdGenerator& idGen_,
                                       SwitchManager& switchManager_,
                                       long timer_interval_)
    : BaseStatsManager(agent_,idGen_,switchManager_,timer_interval_) {
    }


SecGrpStatsManager::~SecGrpStatsManager() {

}
void SecGrpStatsManager::start() {
    LOG(DEBUG) << "Starting acc stats manager";
    BaseStatsManager::start();
    timer->async_wait(bind(&SecGrpStatsManager::on_timer, this, error));
}

void SecGrpStatsManager::stop() {
    LOG(DEBUG) << "Stopping acc stats manager";
    BaseStatsManager::stop();
}

void SecGrpStatsManager::updateAccInFlowEntryMap(uint64_t cookie, uint16_t priority,
    const struct match& match) {
  updateFlowEntryMap(cookie, priority,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,match);
}

void SecGrpStatsManager::updateAccOutFlowEntryMap(uint64_t cookie, uint16_t priority,
    const struct match& match) {
  updateFlowEntryMap(cookie, priority,AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,match);
}

void SecGrpStatsManager::on_timer(const error_code& ec) {

    if (ec) {
        // shut down the timer when we get a cancellation
        return;
    }

    TableState::cookie_callback_t cb_func;
    cb_func = std::bind(&SecGrpStatsManager::updateAccInFlowEntryMap, this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3);

    // Request Switch Manager to provide flow entries

    std::lock_guard<std::mutex> lock(pstatMtx);
    switchManager.forEachCookieMatch(AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                     cb_func);
    cb_func = std::bind(&SecGrpStatsManager::updateAccOutFlowEntryMap, this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3);
    switchManager.forEachCookieMatch(AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                     cb_func);

    PolicyCounterMap_t newClassCountersMap1;
    PolicyCounterMap_t newClassCountersMap2;
    on_timer_base(ec,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,newClassCountersMap1);
    on_timer_base(ec,AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,newClassCountersMap2);
    generatePolicyStatsObjects(&newClassCountersMap1,&newClassCountersMap2);

    sendRequest(AccessFlowManager::SEC_GROUP_IN_TABLE_ID);
    sendRequest(AccessFlowManager::SEC_GROUP_OUT_TABLE_ID);
    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&SecGrpStatsManager::on_timer, this, error));
    }
}

void SecGrpStatsManager::clearCounterObject(const std::string& key,uint8_t index) {
      modelgbp::gbpe::HPPClassifierCounter::remove(agent->getFramework(),getAgentUUID(),genIdList_[key]->uidList[index],key);
    }
bool SecGrpStatsManager::isTableIdFound(uint8_t table_id) {
    if (table_id == AccessFlowManager::SEC_GROUP_IN_TABLE_ID
          || table_id == AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
        return true;
    }
    return false;
}

void SecGrpStatsManager::resolveCounterMaps(uint8_t table_id) {
    if (table_id == AccessFlowManager::SEC_GROUP_IN_TABLE_ID) {
        oldFlowCounterMap = &(secGrpInState.oldFlowCounterMap);
        newFlowCounterMap = &(secGrpInState.newFlowCounterMap);
        removedFlowCounterMap = &(secGrpInState.removedFlowCounterMap);
    } else if (table_id == AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
        oldFlowCounterMap = &(secGrpOutState.oldFlowCounterMap);
        newFlowCounterMap = &(secGrpOutState.newFlowCounterMap);
        removedFlowCounterMap = &(secGrpOutState.removedFlowCounterMap);
    }
}



void SecGrpStatsManager::
updatePolicyStatsCounters(const std::string& l24Classifier,
                          PolicyCounters_t& newVals1,PolicyCounters_t& newVals2) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        uint64_t nextId = getNextClsfrGenId();
        clearOldCounters(l24Classifier,nextId);
        if (newVals1.byte_count && newVals2.byte_count) {
            su.get()->addGbpeHPPClassifierCounter(getAgentUUID(),
                                              nextId,
                                              l24Classifier)
            ->setTxpackets(newVals2.packet_count.get())
            .setTxbytes(newVals2.byte_count.get())
            .setRxpackets(newVals1.packet_count.get())
            .setRxbytes(newVals1.byte_count.get());
        } else if (newVals1.byte_count) {
            su.get()->addGbpeHPPClassifierCounter(getAgentUUID(),
                                              nextId,
                                              l24Classifier)
            ->setRxpackets(newVals1.packet_count.get())
            .setRxbytes(newVals1.byte_count.get());
        } else if (newVals2.byte_count) {
            su.get()->addGbpeHPPClassifierCounter(getAgentUUID(),
                                              nextId,
                                              l24Classifier)
            ->setTxpackets(newVals2.packet_count.get())
            .setTxbytes(newVals2.byte_count.get());
        }
    }
    mutator.commit();
}

void SecGrpStatsManager::objectUpdated(opflex::modb::class_id_t class_id,
    const opflex::modb::URI& uri) {
    if (class_id == modelgbp::gbpe::L24Classifier::CLASS_ID) {
        if (!modelgbp::gbpe::L24Classifier::resolve(agent->getFramework(),uri)) {
            std::string key = uri.toString();
            removeAllCounterObjects(key);
        }
    }
}

} /* namespace ovsagent */
