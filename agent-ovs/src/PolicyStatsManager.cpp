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
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;
using std::bind;
using boost::system::error_code;

PolicyStatsManager::FlowEntryCounterMap_t PolicyStatsManager::oldFlowCounterMap;
PolicyStatsManager::FlowEntryCounterMap_t PolicyStatsManager::newFlowCounterMap;
PolicyStatsManager::FlowEntryCounterMap_t PolicyStatsManager::removedFlowCounterMap;


PolicyStatsManager::PolicyStatsManager(Agent* agent_, IdGenerator& idGen_,
                                       SwitchManager& switchManager_,
                                       long timer_interval_)
    : agent(agent_), intConnection(NULL), idGen(idGen_),
      switchManager(switchManager_),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false) {
    std::random_device rng;
    std::mt19937 urng(rng());
    agentUUID = to_string(basic_random_generator<std::mt19937>(urng)());
}

PolicyStatsManager::~PolicyStatsManager() {

}

void PolicyStatsManager::registerConnection(SwitchConnection* intConnection) {
    this->intConnection = intConnection;
}

void PolicyStatsManager::start() {
    LOG(DEBUG) << "Starting policy stats manager";
    stopping = false;
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

void PolicyStatsManager::updateFlowEntryMap(uint64_t cookie, uint16_t priority,
                                            const struct match& match) {
    FlowEntryMatchKey_t flowEntryKey {
        .cookie = (uint32_t)ovs_ntohll(cookie),
        .priority = priority,
        .match = match,
    };
    /* check if Policy Stats Manager has it in its oldFlowCounterMap */
    FlowEntryCounterMap_t::iterator it =
                               oldFlowCounterMap.find(flowEntryKey);
    if (it != oldFlowCounterMap.end()) {
        //LOG(ERROR) << "flow found in oldFlowCounterMap";
        return;
    }

    /* check if Policy Stats Manager has it in its newFlowCounterMap */
    it = newFlowCounterMap.find(flowEntryKey);
    if (it != newFlowCounterMap.end()) {
        FlowCounters_t&  flowCounters = it->second;
        
        flowCounters.age += 30000;
        //LOG(ERROR) << "flow found in newFlowCounterMap";
        return;
    }

    /* Add the flow entry to newmap */
    FlowCounters_t&  flowCounters = newFlowCounterMap[flowEntryKey];
    flowCounters.visited = false;
    flowCounters.deleted = false;
    flowCounters.age = 0;
    flowCounters.last_packet_count = make_optional(false, 0);
    flowCounters.last_byte_count = make_optional(false, 0);
    flowCounters.diff_packet_count = make_optional(false, 0);
    flowCounters.diff_byte_count = make_optional(false, 0);
#if 0
    LOG(ERROR) << "Cookie= " << (uint32_t)ovs_ntohll(cookie) << " reg0 = " <<
                 match.flow.regs[0] << " reg2 = " <<
                 match.flow.regs[2] << " reg6 = " <<
                 match.flow.regs[6];
    LOG(ERROR) << "flow entry added to newFlowCounterMap";
#endif

}

void PolicyStatsManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        timer.reset();
        return;
    }

    TableState::cookie_callback_t cb_func;
    cb_func = updateFlowEntryMap;

    // Request Switch Manager to provide flow entries

    std::lock_guard<std::mutex> lock(pstatMtx);
    switchManager.forEachCookieMatch(IntFlowManager::POL_TABLE_ID,
                                     cb_func);

    PolicyCounterMap_t newClassCountersMap;

    /**
     * walk through all the old map entries that have
     * been visited.
     */
    for (FlowEntryCounterMap_t:: iterator itr = oldFlowCounterMap.begin();
         itr != oldFlowCounterMap.end();
         itr++) {
        const FlowEntryMatchKey_t& flowEntryKey = itr->first;
        FlowCounters_t&  newFlowCounters = itr->second;
        // Have we visited this flow entry yet
        if (!newFlowCounters.visited) {
            // increase age by polling interval
            newFlowCounters.age += 30000;
            if (newFlowCounters.age >= (30000 * 120)) {
                LOG(DEBUG) << "Unvisited entry for last 120 polling interval";
            }
            continue;
        }

        // Have we collected non-zero diffs for this flow entry
        if (newFlowCounters.diff_packet_count.get() != 0) {

            FlowMatchKey_t flowMatchKey {
                 .cookie = flowEntryKey.cookie,
                 .reg0 = flowEntryKey.match.flow.regs[0],
                 .reg2 = flowEntryKey.match.flow.regs[2],
            };

            PolicyCounters_t&  newClassCounters =
                                       newClassCountersMap[flowMatchKey];

            // We get multiple flow stats entries for same
            // cookie, reg0 and reg2 ie for each classifier
            // multiple flow entries may get installed in OVS
            // The newCountersMap is used for the purpose of
            // additing counters of such flows that have same
            // classifier, reg0 and reg2.

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (newClassCounters.packet_count) {
                // get existing packet_count and byte_count
                packet_count = newClassCounters.packet_count.get();
                byte_count = newClassCounters.byte_count.get();
            }

            // Add counters for new flow entry to existing
            // packet_count and byte_count
            newClassCounters.packet_count = make_optional(true,
                                     newFlowCounters.diff_packet_count.get() +
                                     packet_count);
            newClassCounters.byte_count = make_optional(true,
                                     newFlowCounters.diff_byte_count.get() +
                                     byte_count);
            // reset the per flow entry diff counters to zero.
            newFlowCounters.diff_packet_count = make_optional(true, 0);
            newFlowCounters.diff_byte_count = make_optional(true, 0);
            // set the age of this entry as zero as we have seen
            // its counter increment last polling cycle.
            newFlowCounters.age = 0;
            //LOG(ERROR) << "Added a non-zero diff entry to class counters map";
        }
        // set entry visited as false as we have consumed its diff counters
        newFlowCounters.visited = false;
    }


    /**
     * walk through all the removed flow map entries
     */
    for (FlowEntryCounterMap_t:: iterator itt = removedFlowCounterMap.begin();
         itt != removedFlowCounterMap.end();
         itt++) {
        const FlowEntryMatchKey_t& remFlowEntryKey = itt->first;
        FlowCounters_t&  remFlowCounters = itt->second;

        // Have we collected non-zero diffs for this removed flow entry
        if (remFlowCounters.diff_packet_count.get() != 0) {
           
            FlowMatchKey_t flowMatchKey {
                 .cookie = remFlowEntryKey.cookie,
                 .reg0 = remFlowEntryKey.match.flow.regs[0],
                 .reg2 = remFlowEntryKey.match.flow.regs[2],
            };

            PolicyCounters_t&  newClassCounters =
                                       newClassCountersMap[flowMatchKey];

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (newClassCounters.packet_count) {
                // get existing packet_count and byte_count
                packet_count = newClassCounters.packet_count.get();
                byte_count = newClassCounters.byte_count.get();
            }

            // Add counters for flow entry to be removed
            newClassCounters.packet_count = make_optional(true,
                                     remFlowCounters.diff_packet_count.get() +
                                     packet_count);
            newClassCounters.byte_count = make_optional(true,
                                     remFlowCounters.diff_byte_count.get() +
                                     byte_count);
        }
    }

    /**
     * walk through all the old map entries and remove those entries that have
     * not been visited but age is equal to 120 times
     * polling interval.
     */
    for (FlowEntryCounterMap_t:: iterator itr = oldFlowCounterMap.begin();
         itr != oldFlowCounterMap.end();) {
        const FlowEntryMatchKey_t& flowEntryKey = itr->first;
        FlowCounters_t&  flowCounters = itr->second;
        // Have we visited this flow entry yet
        if (!flowCounters.visited && (flowCounters.age >= (30000 * 120))) {
            //LOG(ERROR) << " Erasing unused (1hr) entry from oldFlowCounterMap";
            LOG(DEBUG) << " Erasing unused (1hr) entry from oldFlowCounterMap";
            itr = oldFlowCounterMap.erase(itr);
        }else 
            itr++;
    }

    /**
     * walk through all the new map entries and remove those entries
     * that have age equal to 120 times polling interval. 
     */
    for (FlowEntryCounterMap_t:: iterator itr = newFlowCounterMap.begin();
         itr != oldFlowCounterMap.end();) {
        const FlowEntryMatchKey_t& flowEntryKey = itr->first;
        FlowCounters_t&  flowCounters = itr->second;
        // Have we visited this flow entry yet
        if (flowCounters.age >= (30000 * 120)) {
            LOG(DEBUG) << " Erasing unused (1hr) entry from newFlowCounterMap";
            //LOG(ERROR) << " Erasing unused (1hr) entry from newFlowCounterMap";
            itr = newFlowCounterMap.erase(itr);
        } else
            itr++;
    }

    // Delete all the entries from removedFlowCountersMap
    // Since we have taken them into account in our calculations for
    // per classifier/epg pair counters.

    removedFlowCounterMap.clear();

    generatePolicyStatsObjects(newClassCountersMap);

    // send port stats request again
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
                         uint16_t priority,
                         struct match& match,
                         uint64_t flow_packet_count,
                         uint64_t flow_byte_count,
                         bool flowRemoved) {

    FlowEntryMatchKey_t flowEntryKey {
        .cookie = cookie,
        .priority = priority,
        .match = match,
    };
    // look in existing oldFlowCounterMap
    FlowEntryCounterMap_t::iterator it =
                               oldFlowCounterMap.find(flowEntryKey);
    uint64_t packet_count = 0;
    uint64_t byte_count = 0;
    if (it != oldFlowCounterMap.end()) {
        FlowCounters_t&  oldFlowCounters = it->second;
        /* compute diffs and mark the entry as visityed */
        packet_count = oldFlowCounters.last_packet_count.get();
        byte_count = oldFlowCounters.last_byte_count.get();
#if 0
        if ((flow_packet_count - packet_count) > 0) {
            LOG(ERROR) << "FlowStat: entry found in oldFlowCounterMap";
            LOG(ERROR) << "Non zero diff packet count flow entry found";
        }
#endif
        if ((flow_packet_count - packet_count) > 0) {
            oldFlowCounters.diff_packet_count = flow_packet_count -
                                 packet_count;
            oldFlowCounters.diff_byte_count =  flow_byte_count - byte_count;
            oldFlowCounters.last_packet_count = flow_packet_count;
            oldFlowCounters.last_byte_count = flow_byte_count;
            oldFlowCounters.visited = true;
        }
        if (flowRemoved) {
            // Move the entry to removedFlowCounterMap
            FlowCounters_t & newFlowCounters = removedFlowCounterMap[flowEntryKey];
            newFlowCounters.diff_byte_count = oldFlowCounters.diff_byte_count;
            newFlowCounters.diff_packet_count = oldFlowCounters.diff_packet_count;
            // Delete the entry from oldFlowCounterMap
            oldFlowCounterMap.erase(flowEntryKey);
        }
    } else {

        // Check if we this entry exists in newFlowCounterMap;

        FlowEntryCounterMap_t::iterator it =
                               newFlowCounterMap.find(flowEntryKey);

        if (it != newFlowCounterMap.end()) {

            // LOG(ERROR) << "FlowStat: entry found in newFlowCounterMap";


            // the flow entry probably got removed even before Policy Stats
            // manager could process its FLOW_STATS_REPLY

            if (flowRemoved) {
                FlowCounters_t & remFlowCounters = removedFlowCounterMap[flowEntryKey];
                remFlowCounters.diff_byte_count = make_optional(true,
                                                      flow_packet_count);
                remFlowCounters.diff_packet_count = make_optional(true,
                                                        flow_byte_count);
                // remove the entry from newFlowCounterMap.
                newFlowCounterMap.erase(flowEntryKey);
            } else {
                // Store the current counters as last counter value
                // by adding the entry to oldFlowCounterMap and expect
                // next FLOW_STATS_REPLY for it to compute delta and 
                // mark the entry as visited. If PolicyStats Manager(OVS agent)
                // got restarted then we can't use the counters reported as is
                // until delta is computed as entry may have existed long 
                // before it.

                if (flow_packet_count != 0) {
                   /* store the counters in oldFlowCounterMap for it and
                    * remove from newFlowCounterMap as it is no more new
                    */
                    FlowCounters_t & newFlowCounters = oldFlowCounterMap[flowEntryKey];
                    newFlowCounters.last_packet_count = make_optional(true,
                                                        flow_packet_count);
                    newFlowCounters.last_byte_count = make_optional(true,
                                                    flow_byte_count);
                    newFlowCounters.visited = false;
                    newFlowCounters.deleted = false;
                    newFlowCounters.age = 0;
                    // remove the entry from newFlowCounterMap.
                    LOG(ERROR) << "Non-zero flow_packet_count"
                         << " moving entry from newFlowCounterMap to"
                         << " oldFlowCounterMap";
                    newFlowCounterMap.erase(flowEntryKey);
                }
            }
        } else {
            // This is the case when FLOW_REMOVE is received even
            // before Policy Stats Manager knew about it.
            // This can potentially happen as we learn about
            // flow entries periodically. The side affect is 
            // we loose a maximum of polling interval worth of counters for this
            // classifier entry. We choose to ingore this case for now.
            LOG(ERROR) << "Received flow stats for an uknown flow";
        }
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

        PolicyStatsManager::updateNewFlowCounters(
                            (uint32_t)ovs_ntohll(fentry->cookie),
                            fentry->priority,
                            (fentry->match),
                            fentry->packet_count,
                            fentry->byte_count,
                            true);
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
                idRdStr = idGen.getStringForId(
                    IntFlowManager::getIdNamespace(RoutingDomain::CLASS_ID),
                                                   rdId);
                if (idRdStr == boost::none) {
                    LOG(DEBUG) << "rdId: " << rdId <<
                        " to URI traslation does not exist";
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
                                    false);
            }
        }
    } while (true);

}


void PolicyStatsManager::generatePolicyStatsObjects(
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
        if (srcEpgUri == boost::none) {
            LOG(ERROR) << "Reg0: " << flowKey.reg0 <<
                          " to EPG URI translation does not exist";
            continue;
        }
        if (dstEpgUri == boost::none) {
            LOG(ERROR) << "Reg2: " << flowKey.reg2 <<
                          " to EPG URI translation does not exist";
            continue;
        }
        if (idStr == boost::none) {
            LOG(ERROR) << "Cookie: " << flowKey.cookie <<
                          " to Classifier URI translation does not exist";
            continue;
        }
        if (newCounters.packet_count.get() != 0) {
            LOG(ERROR) << "Non-zero class counter found..generating objects";
            updatePolicyStatsCounters(srcEpgUri.get().toString(),
                                      dstEpgUri.get().toString(),
                                      idStr.get(),
                                      newCounters);
        }
    }
}

#if 0
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
#endif

void PolicyStatsManager::updatePolicyStatsCounters(const std::string& srcEpg,
                          const std::string& dstEpg,
                          const std::string& l24Classifier,
                          PolicyCounters_t& newVals) {

    Mutator mutator(agent->getFramework(), "policyelement");
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent->getFramework());
    if (su) {
        su.get()->addGbpeL24ClassifierCounter(
                  getAgentUUID(),
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
             getAgentUUID(),
             getNextDropGenId(), rdStr)->setPackets(newVals.packet_count.get())
            .setBytes(newVals.byte_count.get());
    }


    mutator.commit();
}

size_t PolicyStatsManager::KeyHasher::
operator()(const PolicyStatsManager::FlowMatchKey_t& k) const noexcept {
      using boost::hash_value;
      using boost::hash_combine;

      std::size_t seed = 0;
      hash_combine(seed, hash_value(k.cookie));
      hash_combine(seed, hash_value(k.reg0));
      hash_combine(seed, hash_value(k.reg2));

      return (seed);
}

size_t PolicyStatsManager::FlowKeyHasher::
operator()(const PolicyStatsManager::FlowEntryMatchKey_t& k) const noexcept {
      using boost::hash_value;
      using boost::hash_combine;

      std::size_t hashv = match_hash(&k.match, 0);
      hash_combine(hashv, hash_value(k.cookie));
      hash_combine(hashv, hash_value(k.priority));

      return (hashv);
}

} /* namespace ovsagent */
