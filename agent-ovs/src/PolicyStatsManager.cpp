/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PolicyStatsManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "logging.h"
#include "PolicyStatsManager.h"
#include "IntFlowManager.h"
#include "IdGenerator.h"
#include "Agent.h"

#include "ovs-ofputil.h"
#include <lib/util.h>

extern "C" {
#include <openvswitch/ofp-msgs.h>
}

#include <opflex/modb/URI.h>
#include <opflex/modb/Mutator.h>
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <modelgbp/gbpe/L24ClassifierCounter.hpp>
#include <modelgbp/observer/PolicyStatUniverse.hpp>

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

namespace ovsagent {

using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using std::bind;
using boost::system::error_code;

PolicyStatsManager::PolicyStatsManager(Agent* agent_, IdGenerator& idGen_,
                                       long timer_interval_)
    : agent(agent_), intConnection(NULL), idGen(idGen_),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false) {

}

PolicyStatsManager::~PolicyStatsManager() {

}

void PolicyStatsManager::registerConnection(SwitchConnection* intConnection) {
    this->intConnection = intConnection;
}

void PolicyStatsManager::start() {
    LOG(DEBUG) << "Starting policy stats manager";
    stopping = false;

    intConnection->RegisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);

    timer.reset(new deadline_timer(agent_io, milliseconds(timer_interval)));
    timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
}

void PolicyStatsManager::stop() {
    LOG(DEBUG) << "Stopping policy stats manager";
    stopping = true;

    if (intConnection) {
        intConnection->UnregisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    }

    if (timer) {
        timer->cancel();
    }
}

void PolicyStatsManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    // send port stats request
    ofp_version ofVer = (ofp_version)intConnection->GetProtocolVersion();
    ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVer);

    ofputil_flow_stats_request fsr;
    fsr.aggregate = false;
    match_init_catchall(&fsr.match);
    fsr.table_id = IntFlowManager::POL_TABLE_ID;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;

    ofpbuf *req = ofputil_encode_flow_stats_request(&fsr, proto);

    int err = intConnection->SendMessage(req);
    if (err != 0) {
        LOG(ERROR) << "Failed to send policy statistics request: "
                   << ovs_strerror(err);
    }

    if (!stopping) {
        timer->expires_at(timer->expires_at() + milliseconds(timer_interval));
        timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
    }
}

void PolicyStatsManager::Handle(SwitchConnection* connection,
                          int msgType, ofpbuf *msg) {
    if (msgType != OFPTYPE_FLOW_STATS_REPLY) {
        LOG(ERROR) << "Wrong message type in policy stats mgr: " << msgType;
        return;
    }
    //assert(msgType == OFPTYPE_FLOW_STATS_REPLY);

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_flow_stats* fentry, fstat;
    PolicyCounterMap_t newCountersMap;
    PolicyManager& polMgr = agent->getPolicyManager();

    fentry = &fstat;
    std::lock_guard<std::mutex> lock(pstatMtx);
   
    do {
        ofpbuf actsBuf;
        ofpbuf_init(&actsBuf, 32);
        bzero(fentry, sizeof(struct ofputil_flow_stats));

        int ret = ofputil_decode_flow_stats_reply(fentry, msg, false, &actsBuf);

        ofpbuf_uninit(&actsBuf);

        if (ret != 0) {
            if (ret != EOF)
                LOG(ERROR) << "Failed to decode flow stats reply: "
                    << ovs_strerror(ret);
            else
                LOG(DEBUG) << "No more flow stats entries to decode ";
            break;
        } else {
            if (fentry->table_id != IntFlowManager::POL_TABLE_ID) {
                LOG(ERROR) << "Wrong table_id in policy stats mgr: " <<
                    fentry->table_id;
                return;
            }

            struct match *pflow_metadata;
            pflow_metadata = &(fentry->match);
            uint32_t srcEpgId = (uint32_t)pflow_metadata->flow.regs[0];
            uint32_t dstEpgId = (uint32_t)pflow_metadata->flow.regs[2];
            uint32_t cookie = (uint32_t)ovs_ntohll(fentry->cookie);
                LOG(DEBUG) << "Cookie: " <<  fentry->cookie 
                     << "  " << cookie << " reg0=" << srcEpgId <<
                     " reg2=" << dstEpgId;
            optional<URI> srcEpgUri = polMgr.getGroupForVnid(srcEpgId);
            optional<URI> dstEpgUri = polMgr.getGroupForVnid(dstEpgId);
            boost::optional<std::string> idStr = idGen.getId2String(
                IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID), cookie);

            if (srcEpgUri && dstEpgUri && idStr) {
                LOG(DEBUG) << "Policy flow stat rule string: " << 
                    idStr << " srcEpg:" << srcEpgUri.get() <<
                    " dstEpg:" << dstEpgUri.get();
                LOG(DEBUG) << "Policy flow stats match packets: "
                    << fentry->packet_count << " match bytes:" <<
                    fentry->byte_count;

                FlowMatchKey_t flowMatchKey;
                flowMatchKey.cookie = cookie;
                flowMatchKey.reg0 = srcEpgId;
                flowMatchKey.reg2 = dstEpgId;
                uint64_t packet_count = 0;
                uint64_t byte_count = 0;
           
                PolicyCounters_t&  newCounters = 
                                       newCountersMap[flowMatchKey];
                packet_count = 0;
                byte_count = 0;

                // We get multiple flow stats entries for same
                // cookie, reg0 and reg2 ie for each classifier
                // multiple flow entries may get installed in OVS
                // We add counters for flows that belong to one
                // classifier.

                if (newCounters.packet_count) {
                    // get existing packet_count and byte_count
                    packet_count = newCounters.packet_count.get();
                    byte_count = newCounters.byte_count.get();
                }
                // Add counters for new flow entry to existing
                // packet_count and byte_count
                newCounters.packet_count =
                                 make_optional(true, fentry->packet_count + 
                                 packet_count);
                newCounters.byte_count =
                                 make_optional(true, fentry->byte_count +
                                 byte_count);

#if 0
                updatePolicyStatsCounters(srcEpgUri.get().toString(),
                                          dstEpgUri.get().toString(),
                                          idStr.get(),
                                          diffCounters);
#endif
            } else {
                LOG(DEBUG) << "srcEpgUri or dstEpgUri or idStr" <<
                    " mapping failed";
            }
        }
    } while (true);

    // walk through newCountersMap to create new set of MOs

    for (PolicyCounterMap_t:: iterator itr = newCountersMap.begin();
         itr != newCountersMap.end();
         itr++) {
        const FlowMatchKey_t& flowKey = itr->first;
        PolicyCounters_t&  newCounters = itr->second;
        optional<URI> srcEpgUri = polMgr.getGroupForVnid(flowKey.reg0);
        optional<URI> dstEpgUri = polMgr.getGroupForVnid(flowKey.reg2);
        boost::optional<std::string> idStr = idGen.getId2String(
                IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID),
                                               flowKey.cookie);
        PolicyCounters_t&  oldCounters = 
                                       policyCountersMap[flowKey];
        PolicyCounters_t  diffCounters;
        if (oldCounters.packet_count) {
            diffCounters.packet_count = newCounters.packet_count.get() -
                                       oldCounters.packet_count.get();
            diffCounters.byte_count = newCounters.byte_count.get() -
                                       oldCounters.byte_count.get();
        } else {
            diffCounters = newCounters;
        }

        // Store flow stats for the flow match key 
        // in the policyCountersMap. 

        oldCounters.packet_count = newCounters.packet_count;
        oldCounters.byte_count = newCounters.byte_count;

        if (diffCounters.packet_count.get() != 0)
            updatePolicyStatsCounters(srcEpgUri.get().toString(),
                                      dstEpgUri.get().toString(),
                                      idStr.get(),
                                      diffCounters);
    }
}

void PolicyStatsManager::updatePolicyStatsCounters(const std::string& srcEpg,
                          const std::string& dstEpg,
                          const std::string& l24Classifier,
                          PolicyCounters_t& newVals) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        su.get()->addGbpeL24ClassifierCounter(srcEpg, dstEpg, l24Classifier)
            ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }

    mutator.commit();
}
} /* namespace ovsagent */
