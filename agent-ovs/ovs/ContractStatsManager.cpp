/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ContractStatsManager class.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/logging.h>
#include "IntFlowManager.h"
#include <opflexagent/IdGenerator.h>
#include <opflexagent/Agent.h>
#include "TableState.h"
#include "ContractStatsManager.h"

#include "ovs-ofputil.h"

extern "C" {
#include <openvswitch/ofp-msgs.h>
}

#include <opflex/modb/URI.h>
#include <opflex/modb/Mutator.h>
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <modelgbp/gbpe/L24ClassifierCounter.hpp>
#include <modelgbp/observer/PolicyStatUniverse.hpp>
#include <modelgbp/gbp/EpGroup.hpp>
#include <modelgbp/gbp/RoutingDomain.hpp>

namespace opflexagent {

using std::string;
using boost::optional;
using boost::make_optional;
using std::shared_ptr;
using opflex::modb::URI;
using opflex::modb::Mutator;
using namespace modelgbp::gbp;
using namespace modelgbp::observer;
using namespace modelgbp::gbpe;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using std::bind;
using boost::system::error_code;


ContractStatsManager::ContractStatsManager(Agent* agent_, IdGenerator& idGen_,
                                           SwitchManager& switchManager_,
                                           long timer_interval_)
    : PolicyStatsManager(agent_,idGen_,switchManager_,timer_interval_),
      dropGenId(0) {}

ContractStatsManager::~ContractStatsManager() {

}

void ContractStatsManager::start() {
    LOG(DEBUG) << "Starting contract stats manager ("
               << timer_interval << " ms)";
    PolicyStatsManager::start();
    EpGroup::registerListener(agent->getFramework(),this);
    RoutingDomain::registerListener(agent->getFramework(),this);
    timer->async_wait(bind(&ContractStatsManager::on_timer, this, error));
}

void ContractStatsManager::stop() {
    LOG(DEBUG) << "Stopping contract stats manager";
    stopping = true;
    EpGroup::unregisterListener(agent->getFramework(),this);
    RoutingDomain::unregisterListener(agent->getFramework(),this);
    PolicyStatsManager::stop();
}

void ContractStatsManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    TableState::cookie_callback_t cb_func;
    cb_func = [this](uint64_t cookie, uint16_t priority,
                     const struct match& match) {
        updateFlowEntryMap(contractState, cookie, priority, match);
    };

    // Request Switch Manager to provide flow entries
    {
        std::lock_guard<std::mutex> lock(pstatMtx);
        switchManager.forEachCookieMatch(IntFlowManager::POL_TABLE_ID,
                                         cb_func);

        PolicyCounterMap_t newClassCountersMap;
        on_timer_base(ec, contractState, newClassCountersMap);

        generatePolicyStatsObjects(&newClassCountersMap);
    }

    sendRequest(IntFlowManager::POL_TABLE_ID);
    if (!stopping && timer) {
        timer->expires_from_now(milliseconds(timer_interval));
        timer->async_wait(bind(&ContractStatsManager::on_timer, this, error));
    }
}

void ContractStatsManager::handleDropStats(struct ofputil_flow_stats* fentry) {
    uint32_t rdId = (uint32_t)fentry->match.flow.regs[6];
    if (rdId == 0) return;

    boost::optional<std::string> idStr =
        idGen.getStringForId(IntFlowManager::
                             getIdNamespace(RoutingDomain::CLASS_ID),
                             rdId);
    if (idStr == boost::none) {
        LOG(DEBUG) << "rdId: " << rdId <<
            " to URI translation does not exist";
        return;
    }

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

void ContractStatsManager::
updatePolicyStatsCounters(const std::string& srcEpg,
                          const std::string& dstEpg,
                          const std::string& l24Classifier,
                          PolicyCounters_t& newVals) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        uint64_t nextId = getNextClsfrGenId();
        clearOldCounters(l24Classifier,nextId);
        su.get()->addGbpeL24ClassifierCounter(getAgentUUID(),
                                              nextId,
                                              srcEpg, dstEpg, l24Classifier)
            ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
        counterObjectKeys_[nextId] = counter_key_t(l24Classifier,srcEpg,dstEpg);
    }
    mutator.commit();
}

void ContractStatsManager::clearCounterObject(const std::string& key,
                                              uint8_t index) {
    std::string l24Classifier,srcEpg,dstEpg;
    if (!genIdList_.count(key)) return;
    int uid = genIdList_[key]->uidList[index];
    std::tie(l24Classifier,srcEpg,dstEpg) = counterObjectKeys_[uid];
    L24ClassifierCounter::remove(agent->getFramework(),
                                 getAgentUUID(),genIdList_[key]
                                 ->uidList[index],srcEpg,dstEpg,l24Classifier);
    counterObjectKeys_.erase(counterObjectKeys_.find(uid));
}

void ContractStatsManager::
updatePolicyStatsDropCounters(const std::string& rdStr,
                              PolicyDropCounters_t& newVals) {
    uint64_t nextId = getNextDropGenId();
    Mutator mutator(agent->getFramework(), "policyelement");
    if (!dropCounterList_.count(rdStr)) {
        dropCounterList_[rdStr] =
            std::unique_ptr<CircularBuffer>(new CircularBuffer());
    }
    if (dropCounterList_[rdStr]->count == MAX_DROP_COUNTER_LIMIT) {
        dropCounterList_[rdStr]->count = 0;
    }
    if (dropCounterList_[rdStr]->uidList.size() == MAX_DROP_COUNTER_LIMIT) {
        RoutingDomainDropCounter::
            remove(agent->getFramework(),getAgentUUID(),
                   dropCounterList_[rdStr]->
                   uidList[dropCounterList_[rdStr]->count],
                   rdStr);
        dropCounterList_[rdStr]->
            uidList[dropCounterList_[rdStr]->count] = nextId;
    } else {
        dropCounterList_[rdStr]->uidList.push_back(nextId);
    }
    dropCounterList_[rdStr]->count++;
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        su.get()->addGbpeRoutingDomainDropCounter(getAgentUUID(),
                                                  nextId,rdStr)
            ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }
    mutator.commit();
}

void ContractStatsManager::objectUpdated(opflex::modb::class_id_t class_id,
                                         const opflex::modb::URI& uri) {
    if (class_id == L24Classifier::CLASS_ID) {
        if (!L24Classifier::resolve(agent->getFramework(),uri)) {
            removeAllCounterObjects(uri.toString());
        }
    } else if (class_id == EpGroup::CLASS_ID) {
        if (!EpGroup::resolve(agent->getFramework(),uri)) {
            const std::string& epgName = uri.toString();
            // iterate trough all keys and objects
            // and check which ones srcEpg or dstEpg is equal to epgName
            Mutator mutator(agent->getFramework(), "policyelement");
            auto it = genIdList_.begin();
            for (;it != genIdList_.end();++it) {
                for (size_t i = 0; i < it->second->uidList.size(); i++) {
                    int uid = it->second->uidList[i];
                    std::string l24Classifier,srcEpg,dstEpg;
                    std::tie(l24Classifier,srcEpg,dstEpg) =
                        counterObjectKeys_[uid];
                    if (srcEpg == epgName || dstEpg == epgName) {
                        clearCounterObject(it->first,i);
                    }
                }
            }
            mutator.commit();
        }
    } else if (class_id == RoutingDomain::CLASS_ID) {
        if (!RoutingDomain::resolve(agent->getFramework(),uri)) {
            Mutator mutator(agent->getFramework(), "policyelement");
            const std::string& rdName = uri.toString();
            if (!dropCounterList_.count(rdName)) return;
            for (size_t i = 0; i < dropCounterList_[rdName]->uidList.size();
                 i++) {
                RoutingDomainDropCounter::
                    remove(agent->getFramework(),getAgentUUID(),
                           dropCounterList_[rdName]->
                           uidList[dropCounterList_[rdName]->count],
                           rdName);
            }
            if (dropCounterList_.count(rdName))
                dropCounterList_.erase(dropCounterList_.find(rdName));
            mutator.commit();
        }

    }
}

void ContractStatsManager::Handle(SwitchConnection* connection,
                                  int msgType, ofpbuf *msg) {
    handleMessage(msgType, msg,
                  [this](uint32_t table_id) -> flowCounterState_t* {
                      if (table_id == IntFlowManager::POL_TABLE_ID)
                          return &contractState;
                      else
                          return NULL;
                  });
}

} /* namespace opflexagent */
