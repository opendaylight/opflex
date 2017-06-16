/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for AccStatsManager class.
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
#include "AccStatsManager.h"

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

static const int MAX_AGE = 3;

AccStatsManager::AccStatsManager(Agent* agent_, IdGenerator& idGen_,
                                       SwitchManager& switchManager_,
                                       long timer_interval_)
    : BaseStatsManager(agent_,idGen_,switchManager_,timer_interval_) {}


AccStatsManager::~AccStatsManager() {

}
void AccStatsManager::start() {
    LOG(DEBUG) << "Starting acc stats manager";
    BaseStatsManager::start();
    timer->async_wait(bind(&AccStatsManager::on_timer, this, error));
}

void AccStatsManager::stop() {
    LOG(DEBUG) << "Stopping acc stats manager";
    BaseStatsManager::stop();
}

void AccStatsManager::updateAccInFlowEntryMap(uint64_t cookie, uint16_t priority,
    const struct match& match) {
  updateFlowEntryMap(cookie, priority,0,match);
}

void AccStatsManager::updateAccOutFlowEntryMap(uint64_t cookie, uint16_t priority,
    const struct match& match) {
  updateFlowEntryMap(cookie, priority,1,match);
}

void AccStatsManager::on_timer(const error_code& ec) {

    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    TableState::cookie_callback_t cb_func;
    cb_func = std::bind(&AccStatsManager::updateAccInFlowEntryMap, this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3);

    // Request Switch Manager to provide flow entries

    std::lock_guard<std::mutex> lock(pstatMtx);
    switchManager.forEachCookieMatch(AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                     cb_func);
    cb_func = std::bind(&AccStatsManager::updateAccOutFlowEntryMap, this,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3);
    switchManager.forEachCookieMatch(AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                     cb_func);

    PolicyCounterMap_t newClassCountersMap1;
    PolicyCounterMap_t newClassCountersMap2;
    on_timer_base(ec,0,newClassCountersMap1);
    on_timer_base(ec,1,newClassCountersMap2);
    generatePolicyStatsObjects(newClassCountersMap1,newClassCountersMap2);

    // send port stats request again
    ofp_version ofVer = (ofp_version)connection->GetProtocolVersion();
    ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVer);

    ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(ofputil_flow_stats_request));
    fsr.aggregate = false;
    match_init_catchall(&fsr.match);
    fsr.table_id = AccessFlowManager::SEC_GROUP_IN_TABLE_ID;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;

    ofpbuf *req = ofputil_encode_flow_stats_request(&fsr, proto);
    ofpmsg_update_length(req);

    int err = connection->SendMessage(req);
    if (err != 0) {
        LOG(ERROR) << "Failed to send policy statistics request: "
                   << ovs_strerror(err);
    }
    fsr.table_id = AccessFlowManager::SEC_GROUP_OUT_TABLE_ID;
    req = ofputil_encode_flow_stats_request(&fsr, proto);
    ofpmsg_update_length(req);

    err = connection->SendMessage(req);
    if (err != 0) {
        LOG(ERROR) << "Failed to send policy statistics request: "
                   << ovs_strerror(err);
    }
    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&AccStatsManager::on_timer, this, error));
    }
}



void AccStatsManager::handleFlowRemoved(ofpbuf *msg) {

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_flow_removed* fentry, flow_removed;
    PolicyManager& polMgr = agent->getPolicyManager();

    fentry = &flow_removed;
    std::lock_guard<std::mutex> lock(pstatMtx);
    int ret;
    bzero(fentry, sizeof(struct ofputil_flow_removed));

    ret = ofputil_decode_flow_removed(fentry, oh);
    if (ret != 0) {
        LOG(ERROR) << "Failed to decode flow removed message: "
                   << ovs_strerror(ret);
        return;
    } else {
      if (fentry->table_id != AccessFlowManager::SEC_GROUP_IN_TABLE_ID && 
          fentry->table_id != AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
            LOG(ERROR) << "Unexpected table_id = " << fentry->table_id <<
                " in flow remove msg: ";
            return;
      }
	    uint8_t index = 0;
      if (fentry->table_id == AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
          index = 1;
	    }

      updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                              fentry->priority,
                              (fentry->match),
                              fentry->packet_count,
                              fentry->byte_count,
                              index,true);
    }

}

void AccStatsManager::handleFlowStats(int msgType, ofpbuf *msg) {

    struct ofputil_flow_stats* fentry, fstat;
    PolicyCounterMap_t newCountersMap;
    PolicyManager& polMgr = agent->getPolicyManager();

    fentry = &fstat;
    std::lock_guard<std::mutex> lock(pstatMtx);

    do {
        ofpbuf actsBuf;
        ofpbuf_init(&actsBuf, 64);
        bzero(fentry, sizeof(struct ofputil_flow_stats));

        int ret = ofputil_decode_flow_stats_reply(fentry, msg, false, &actsBuf);
        ofpbuf_uninit(&actsBuf);

        if (ret != 0) {
            if (ret != EOF) {
                LOG(ERROR) << "Failed to decode flow stats reply: "
                           << ovs_strerror(ret);
            } else {
                LOG(DEBUG) << "No more flow stats entries to decode "
                           << ovs_strerror(ret);
            }
            break;
        } else {
	          if (fentry->table_id != AccessFlowManager::SEC_GROUP_IN_TABLE_ID && 
		          fentry->table_id != AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
                LOG(ERROR) << "Unexpected table_id: " <<
                    fentry->table_id;
                continue;
            }
	          uint8_t index = 0;
            if (fentry->table_id == AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
              index = 1;
	          }
            if ((fentry->flags & OFPUTIL_FF_SEND_FLOW_REM) == 0) {
                // skip those flow entries that don't have flag set
                continue;
            }

            // Packet forward entries are per classifier

            // Handle flow stats entries for packets that are matched
            // and are forwarded

            updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                                      fentry->priority,
                                      (fentry->match),
                                      fentry->packet_count,
                                      fentry->byte_count,
                                      index,false);
        }
    } while (true);

}

void AccStatsManager::
generatePolicyStatsObjects(PolicyCounterMap_t& newCountersMap1, PolicyCounterMap_t& newCountersMap2) {

    // walk through newCountersMap to create new set of MOs

    PolicyManager& polMgr = agent->getPolicyManager();

    for (PolicyCounterMap_t:: iterator itr = newCountersMap1.begin();
         itr != newCountersMap1.end();
         itr++) {
        const FlowMatchKey_t& flowKey = itr->first;
        PolicyCounters_t&  newCounters1 = itr->second;
        PolicyCounters_t&  newCounters2 = newCountersMap2.find(flowKey)->second;
	      boost::optional<std::string> secGrpsIdStr = idGen.getStringForId(
                "secGroupSet",flowKey.reg0);
        boost::optional<std::string> idStr =
            idGen.getStringForId(IntFlowManager::
                                 getIdNamespace(L24Classifier::CLASS_ID),
                                 flowKey.cookie);
	      if (secGrpsIdStr == boost::none) {
            LOG(ERROR) << "Reg0: " << flowKey.reg0 <<
                          " to SecGrpSetId translation does not exist";
            continue;
        }
        if (idStr == boost::none) {
            LOG(ERROR) << "Cookie: " << flowKey.cookie
                       << " to Classifier URI translation does not exist";
            continue;
        }
        updatePolicyStatsCounters(idStr.get(),
                                      newCounters1,newCounters2);
    }
}

void AccStatsManager::
updatePolicyStatsCounters(const std::string& l24Classifier,
                          PolicyCounters_t& newVals1,PolicyCounters_t& newVals2) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
            su.get()->addGbpeHPPClassifierCounter(getAgentUUID(),
                                              getNextClsfrGenId(),
                                              l24Classifier)
            ->setTxpackets(newVals2.packet_count.get())
            .setTxbytes(newVals2.byte_count.get())
            .setRxpackets(newVals1.packet_count.get())
            .setRxbytes(newVals1.byte_count.get());
	  }

    mutator.commit();
}



} /* namespace ovsagent */
