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
      
}

PolicyStatsManager::~PolicyStatsManager() {

}
void PolicyStatsManager::start() {
    LOG(DEBUG) << "Starting policy stats manager";
    BaseStatsManager::start();
    agent->getPolicyManager().registerListener(this);
    timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
}

void PolicyStatsManager::stop() {
    LOG(DEBUG) << "Stopping policy stats manager";
    BaseStatsManager::stop();
    agent->getPolicyManager().unregisterListener(this);
}

void PolicyStatsManager::updatePolicyFlowEntryMap(uint64_t cookie,
                                             uint16_t priority,
                                            const struct match& match) {
    updateFlowEntryMap(cookie, priority,IntFlowManager::POL_TABLE_ID,match);
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
    on_timer_base(ec,IntFlowManager::POL_TABLE_ID,newClassCountersMap);

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
        uint64_t nextId = getNextClsfrGenId();
        string key = l24Classifier+srcEpg+dstEpg;
        clearOldCounters(key,nextId);
        classifierList_[key] = l24Classifier;
        srcEpgList_[key] = srcEpg;
        dstEpgList_[key] = dstEpg;
        su.get()->addGbpeL24ClassifierCounter(getAgentUUID(),
                                              nextId,
                                              srcEpg, dstEpg, l24Classifier)
            ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }

    mutator.commit();
}
void PolicyStatsManager::clearCounterObject(const std::string& key,
    uint8_t index) {
      modelgbp::gbpe::L24ClassifierCounter::remove(agent->getFramework(),getAgentUUID(),genIdList_[key][index],
            srcEpgList_[key],dstEpgList_[key],classifierList_[key]);
      
}

void PolicyStatsManager::
updatePolicyStatsDropCounters(const std::string& rdStr,
                              PolicyDropCounters_t& newVals) {

    uint64_t nextId = getNextDropGenId();
    if (!reachedLimitDropCounter_.count(rdStr)) {
        dropCounterList_[rdStr] = new uint64_t[MAX_DROP_COUNTER_LIMIT];
        idCountDropCounter_[rdStr] = 0;
        reachedLimitDropCounter_[rdStr] = false;
    }
   if (idCountDropCounter_[rdStr] == MAX_DROP_COUNTER_LIMIT) {
        reachedLimitDropCounter_[rdStr] = true;
        idCountDropCounter_[rdStr] = 0;
    }
    if (reachedLimitDropCounter_.find(rdStr)->second) {
        modelgbp::gbpe::RoutingDomainDropCounter::remove(getAgentUUID(),
                     dropCounterList_[rdStr][idCountDropCounter_[rdStr]],rdStr);
    }
    dropCounterList_[rdStr][idCountDropCounter_[rdStr]] = nextId;

    idCountDropCounter_[rdStr]++;
    Mutator mutator(agent->getFramework(), "policyelement");
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

void PolicyStatsManager::contractUpdated(const opflex::modb::URI& contractURI) {
    const string& contractId = contractURI.toString();
    PolicyManager& polMgr = agent->getPolicyManager();
    if (polMgr.contractExists(contractURI)) { 
        PolicyManager::uri_set_t provURIs;
        PolicyManager::uri_set_t consURIs;
        polMgr.getContractProviders(contractURI, provURIs);
        polMgr.getContractConsumers(contractURI, consURIs);
        PolicyManager::rule_list_t rules;
        polMgr.getContractRules(contractURI, rules);
        for (const opflex::modb::URI& srcEpg : provURIs) {
            for (const opflex::modb::URI& dstEpg : consURIs) {
                if (srcEpg.toString() == dstEpg.toString())
                    continue;
                 for (shared_ptr<PolicyRule>& pc : rules) {
                     const shared_ptr<L24Classifier>& cls = pc->getL24Classifier();
                     const URI& ruleURI = cls.get()->getURI();
                     std::string key = ruleURI.toString()+srcEpg.toString()+dstEpg.toString();
                     contractKeyMap.insert(std::pair<std::string,std::string>(contractId,key));

                 }
            }
        }
    } else {
        std::multimap<std::string,std::string>::iterator it = contractKeyMap.lower_bound(contractId);
        for (;it != contractKeyMap.upper_bound(contractId);++it) {
            std::string key = it->second;
            removeAllCounterObjects(key);
            if (srcEpgList_.count(key)) srcEpgList_.erase(srcEpgList_.find(key));
            if (dstEpgList_.count(key)) dstEpgList_.erase(dstEpgList_.find(key));
        }
    }   
}

void PolicyStatsManager::egDomainUpdated(const opflex::modb::URI& epgURI) {
    PolicyManager& polMgr = agent->getPolicyManager();
    if (!polMgr.groupExists(epgURI)) {
        const std::string epgName = epgURI.toString();
        std::unordered_map<std::string,std::string>::iterator it = srcEpgList_.begin();
        for(;it != srcEpgList_.end();++it) {
            if (it->first == epgName) {
                removeAllCounterObjects(it->first);
                if (srcEpgList_.count(it->first)) srcEpgList_.erase(srcEpgList_.find(it->first));
                if (dstEpgList_.count(it->first)) dstEpgList_.erase(dstEpgList_.find(it->first));
            }
        }
        it = dstEpgList_.begin();
        for(;it != dstEpgList_.end();++it) {
            if (it->first == epgName) {
                removeAllCounterObjects(it->first);
                if (srcEpgList_.count(it->first)) srcEpgList_.erase(srcEpgList_.find(it->first));
                if (dstEpgList_.count(it->first)) dstEpgList_.erase(dstEpgList_.find(it->first));
            }
        }
    }
}

void
PolicyStatsManager::handleDomainUpdate(opflex::modb::class_id_t cid, const opflex::modb::URI& domURI) {

    if (cid == RoutingDomain::CLASS_ID) {
        optional<shared_ptr<RoutingDomain > > rd =
            RoutingDomain::resolve(agent->getFramework(), domURI);

        if (!rd) {
            std::string rdName = domURI.toString();
            if (!reachedLimitDropCounter_.count(rdName)) return;
            int max = idCountDropCounter_[rdName];
            if (reachedLimitDropCounter_[rdName]) {
                max = MAX_DROP_COUNTER_LIMIT;
            }   
            for (int i = 0;i < max;i++) {
                modelgbp::gbpe::RoutingDomainDropCounter::remove(getAgentUUID(),
                     dropCounterList_[rdName][idCountDropCounter_[rdName]],rdName);
            }
            if (idCountDropCounter_.count(rdName)) idCountDropCounter_.erase(idCountDropCounter_.find(rdName));   
            if (reachedLimitDropCounter_.count(rdName)) reachedLimitDropCounter_.erase(reachedLimitDropCounter_.find(rdName));   
            delete [] dropCounterList_[rdName];
            if (dropCounterList_.count(rdName)) dropCounterList_.erase(dropCounterList_.find(rdName));   
        }
    }
}
} /* namespace ovsagent */
