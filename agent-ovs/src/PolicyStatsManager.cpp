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
    timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
}

void PolicyStatsManager::stop() {
    LOG(DEBUG) << "Stopping policy stats manager";
    BaseStatsManager::stop();
}

void PolicyStatsManager::updatePolicyFlowEntryMap(uint64_t cookie,
                                             uint16_t priority,
                                            const struct match& match) {
    BaseStatsManager::updateFlowEntryMap(cookie, priority,0,match);
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
    BaseStatsManager::on_timer(ec,0,newClassCountersMap);

    generatePolicyStatsObjects(newClassCountersMap);

    // send port stats request again
    ofp_version ofVer = (ofp_version)connection->GetProtocolVersion();
    ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVer);

    ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(ofputil_flow_stats_request));
    fsr.aggregate = false;
    match_init_catchall(&fsr.match);
    fsr.table_id = IntFlowManager::POL_TABLE_ID;
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

    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
    }
}

void PolicyStatsManager::handleFlowRemoved(ofpbuf *msg) {

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
        if (fentry->table_id != IntFlowManager::POL_TABLE_ID) {
            LOG(ERROR) << "Unexpected table_id = " << fentry->table_id <<
                " in flow remove msg: ";
            return;
        }

        updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                              fentry->priority,
                              (fentry->match),
                              fentry->packet_count,
                              fentry->byte_count,
                              0,true);
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


void PolicyStatsManager::handleFlowStats(int msgType, ofpbuf *msg) {

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
            if (ret != EOF)
                LOG(ERROR) << "Failed to decode flow stats reply: "
                           << ovs_strerror(ret);
            else
                LOG(ERROR) << "No more flow stats entries to decode "
                           << ovs_strerror(ret);
            break;
        } else {
            if (fentry->table_id != IntFlowManager::POL_TABLE_ID) {
                LOG(ERROR) << "Unexpected table_id: " <<
                    fentry->table_id;
                continue;
            }
            if ((fentry->flags & OFPUTIL_FF_SEND_FLOW_REM) == 0) {
                // skip those flow entries that don't have flag set
                continue;
            }

            // Handle forward entries and drop entries separately
            // Packet forward entries are per classifier
            // Packet drop entries are per routingDomain

            struct match *pflow_metadata;
            pflow_metadata = &(fentry->match);
            uint32_t rdId = (uint32_t)pflow_metadata->flow.regs[6];
            boost::optional<std::string> idRdStr;

            if (rdId) {
                idRdStr = idGen
                    .getStringForId(IntFlowManager::
                                    getIdNamespace(RoutingDomain::CLASS_ID),
                                    rdId);
                if (idRdStr == boost::none) {
                    LOG(DEBUG) << "rdId: " << rdId <<
                        " to URI translation does not exist";
                    continue;
                }
            }
            // Does flow stats entry qualify to be a drop entry?
            // if yes, then process it and continue with next flow
            // stats entry.
            if (rdId && idRdStr && (fentry->priority == 1)) {
                handleDropStats(rdId, idRdStr, fentry);
            } else {

                // Handle flow stats entries for packets that are matched
                // and are forwarded

                updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                                      fentry->priority,
                                      (fentry->match),
                                      fentry->packet_count,
                                      fentry->byte_count,
                                      0,false);
            }
        }
    } while (true);

}

void PolicyStatsManager::
generatePolicyStatsObjects(PolicyCounterMap_t& newCountersMap) {

    // walk through newCountersMap to create new set of MOs

    PolicyManager& polMgr = agent->getPolicyManager();

    for (PolicyCounterMap_t:: iterator itr = newCountersMap.begin();
         itr != newCountersMap.end();
         itr++) {
        const FlowMatchKey_t& flowKey = itr->first;
        PolicyCounters_t&  newCounters = itr->second;
        optional<URI> srcEpgUri = polMgr.getGroupForVnid(flowKey.reg0);
        optional<URI> dstEpgUri = polMgr.getGroupForVnid(flowKey.reg2);
        boost::optional<std::string> idStr =
            idGen.getStringForId(IntFlowManager::
                                 getIdNamespace(L24Classifier::CLASS_ID),
                                 flowKey.cookie);
        if (srcEpgUri == boost::none) {
            LOG(ERROR) << "Reg0: " << flowKey.reg0
                       << " to EPG URI translation does not exist";
            continue;
        }
        if (dstEpgUri == boost::none) {
            LOG(ERROR) << "Reg2: " << flowKey.reg2
                       << " to EPG URI translation does not exist";
            continue;
        }
        if (idStr == boost::none) {
            LOG(ERROR) << "Cookie: " << flowKey.cookie
                       << " to Classifier URI translation does not exist";
            continue;
        }
        if (newCounters.packet_count.get() != 0) {
            updatePolicyStatsCounters(srcEpgUri.get().toString(),
                                      dstEpgUri.get().toString(),
                                      idStr.get(),
                                      newCounters);
        }
    }
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
