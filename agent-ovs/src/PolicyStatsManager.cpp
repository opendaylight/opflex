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
#include <boost/lexical_cast.hpp>

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
    allocateAgentUUID();
    resetClsfrGenId();
    resedDropGenId();

    intConnection->RegisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    intConnection->RegisterMessageHandler(OFPTYPE_FLOW_REMOVED, this);

    timer.reset(new deadline_timer(agent_io, milliseconds(timer_interval)));
    timer->async_wait(bind(&PolicyStatsManager::on_timer, this, error));
}

void PolicyStatsManager::stop() {
    LOG(DEBUG) << "Stopping policy stats manager";
    stopping = true;

    if (intConnection) {
        intConnection->UnregisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
        intConnection->UnregisterMessageHandler(OFPTYPE_FLOW_REMOVED, this);
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

void PolicyStatsManager::updateNewFlowCounters(uint32_t cookie,
                         struct match* pflow_metadata,
                         uint64_t flow_packet_count,
                         uint64_t flow_byte_count,
                         PolicyCounterMap_t& newCountersMap,
                         bool flowRemoved) {

    uint32_t srcEpgId = (uint32_t)pflow_metadata->flow.regs[0];
    uint32_t dstEpgId = (uint32_t)pflow_metadata->flow.regs[2];
#if 0
    LOG(ERROR) << "Cookie: " <<  cookie
                     << "  " << cookie << " reg0=" << srcEpgId <<
                     " reg2=" << dstEpgId;
    LOG(ERROR) << "Policy flow stats match packets: "
                    << flow_packet_count << " match bytes:" <<
                    flow_byte_count;
#endif
    PolicyManager& polMgr = agent->getPolicyManager();
    optional<URI> srcEpgUri = polMgr.getGroupForVnid(srcEpgId);
    optional<URI> dstEpgUri = polMgr.getGroupForVnid(dstEpgId);
    optional<std::string> idStr = idGen.getStringForId(
                IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID),
                                               cookie);
    FlowMatchKey_t flowMatchKey {
        .cookie = cookie,
        .reg0 = srcEpgId,
        .reg2 = dstEpgId,
    };

    if (srcEpgUri && dstEpgUri && idStr) {

#if 0
        LOG(ERROR) << "Policy flow stat rule string: " <<
                    idStr << " srcEpg:" << srcEpgUri.get() <<
                    " dstEpg:" << dstEpgUri.get();
        LOG(ERROR) << "Policy flow stats match packets: "
                    << flow_packet_count << " match bytes:" <<
                    flow_byte_count;
#endif

        PolicyCounters_t&  newCounters =
                                       newCountersMap[flowMatchKey];
        uint64_t packet_count = 0;
        uint64_t byte_count = 0;

        // We get multiple flow stats entries for same
        // cookie, reg0 and reg2 ie for each classifier
        // multiple flow entries may get installed in OVS
        // The newCountersMap is used for the purpose of
        // additing counters of such flows that have same
        // classifier.

        if (newCounters.packet_count) {
            // get existing packet_count and byte_count
            packet_count = newCounters.packet_count.get();
            byte_count = newCounters.byte_count.get();
        }

        // Add counters for new flow entry to existing
        // packet_count and byte_count
        newCounters.packet_count =
                                 make_optional(true, flow_packet_count +
                                 packet_count);
        newCounters.byte_count =
                                 make_optional(true, flow_byte_count +
                                 byte_count);

    } else {

        LOG(DEBUG) << "srcEpgUri or dstEpgUri or idStr" <<
                    " mapping failed";
        if (flowRemoved) {

            // Policy manager may have removed the mappings before
            // Policy Stats manager gets a chance to process flow
            // remove message.
            // Check if we already have stats for this flow
            // classifier in policyCountersMap and if we do then delete it.

            PolicyCounterMap_t:: iterator itr
                    = policyCountersMap.find(flowMatchKey);
            if (itr != newCountersMap.end()) {
                policyCountersMap.erase(flowMatchKey);
            }
        }
    }
}

void PolicyStatsManager::handleFlowRemoved(ofpbuf *msg) {

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_flow_removed* fentry, flow_removed;
    PolicyCounterMap_t newCountersMap;
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

        PolicyStatsManager::updateNewFlowCounters(
                            (uint32_t)ovs_ntohll(fentry->cookie),
                            &(fentry->match),
                            fentry->packet_count,
                            fentry->byte_count,
                            newCountersMap, true);
    }

    // walk through newCountersMap to create new set of MOs

    updatePolicyStatsMap(newCountersMap);

    // erase the mapping from policyCountersMap for all
    // removed flows
    for (PolicyCounterMap_t:: iterator iter = newCountersMap.begin();
         iter != newCountersMap.end();
         iter++) {

        const FlowMatchKey_t& flowKey = iter->first;
        policyCountersMap.erase(flowKey);
    }
}

void PolicyStatsManager::handleDropStats(uint32_t rdId,
                         boost::optional<std::string> idStr,
                         struct ofputil_flow_stats* fentry) {

    PolicyDropCounters_t    newCounters;
    newCounters.packet_count = make_optional(true, fentry->packet_count);
    newCounters.byte_count = make_optional(true, fentry->byte_count);
    PolicyDropCounters_t&  oldCounters = policyDropCountersMap[rdId];
    PolicyDropCounters_t  diffCounters;

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

    if (diffCounters.packet_count.get() != 0)
        updatePolicyStatsDropCounters(idStr.get(),
                                      diffCounters);
}

void PolicyStatsManager::Handle(SwitchConnection* connection,
                          int msgType, ofpbuf *msg) {

    if (msg == (ofpbuf *)NULL) {
        LOG(ERROR) << "Unexpected null message";
        return;
    }
    if (msgType == OFPTYPE_FLOW_STATS_REPLY) {
        PolicyStatsManager::handleFlowStats(msgType, msg);
    } else if (msgType == OFPTYPE_FLOW_REMOVED) {
        PolicyStatsManager::handleFlowRemoved(msg);
    } else {
        LOG(ERROR) << "Unexpected message type: " << msgType;
        return;
    }

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

            // Handle forward entries and drop entries separately
            // Packet forward entries are per classifier
            // Packet drop entries are per routingDomain

            struct match *pflow_metadata;
            pflow_metadata = &(fentry->match);
            uint32_t rdId = (uint32_t)pflow_metadata->flow.regs[6];
            boost::optional<std::string> idRdStr;

            if (rdId)
                idRdStr = idGen.getStringForId(
                    IntFlowManager::getIdNamespace(RoutingDomain::CLASS_ID),
                                                   rdId);
            // Does flow stats entry qualify to be a drop entry?
            // if yes, then process it and continue with next flow
            // stats entry.
            if (rdId && idRdStr && (fentry->priority == 1)) {
                handleDropStats(rdId, idRdStr, fentry);
            } else {

                // Handle flow stats entries for packets that are matched
                // and are forwarded

                updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                                    &(fentry->match),
                                    fentry->packet_count,
                                    fentry->byte_count,
                                    newCountersMap, false);
            }
        }
    } while (true);

    updatePolicyStatsMap(newCountersMap);

}


void PolicyStatsManager::updatePolicyStatsMap(
                           PolicyCounterMap_t& newCountersMap) {

    // walk through newCountersMap to create new set of MOs

    PolicyManager& polMgr = agent->getPolicyManager();

    for (PolicyCounterMap_t:: iterator itr = newCountersMap.begin();
         itr != newCountersMap.end();
         itr++) {
        const FlowMatchKey_t& flowKey = itr->first;
        PolicyCounters_t&  newCounters = itr->second;
        optional<URI> srcEpgUri = polMgr.getGroupForVnid(flowKey.reg0);
        optional<URI> dstEpgUri = polMgr.getGroupForVnid(flowKey.reg2);
        boost::optional<std::string> idStr = idGen.getStringForId(
                IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID),
                                               flowKey.cookie);
        PolicyCounters_t&  oldCounters = policyCountersMap[flowKey];
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
        su.get()->addGbpeL24ClassifierCounter(
                  boost::lexical_cast<std::string>(getAgentUUID()),
                  getNextClsfrGenId(),
                  srcEpg, dstEpg, l24Classifier)
                ->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }

    mutator.commit();
}

void PolicyStatsManager::updatePolicyStatsDropCounters(
                             const std::string& rdStr,
                             PolicyDropCounters_t& newVals) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        su.get()->addGbpeRoutingDomainDropCounter(
             boost::lexical_cast<std::string>(getAgentUUID()),
             getNextDropGenId(), rdStr)->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }


    mutator.commit();
}

} /* namespace ovsagent */
