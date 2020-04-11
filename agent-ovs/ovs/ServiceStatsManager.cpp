/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ServiceStatsManager class.
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
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
#include "ServiceStatsManager.h"

#include "ovs-ofputil.h"

extern "C" {
#include <openvswitch/ofp-msgs.h>
}

namespace opflexagent {

using boost::make_optional;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using std::bind;
using boost::system::error_code;


ServiceStatsManager::ServiceStatsManager(Agent* agent_, IdGenerator& idGen_,
                                           SwitchManager& switchManager_,
                                           IntFlowManager& intFlowManager_,
                                           long timer_interval_)
    : PolicyStatsManager(agent_,idGen_,switchManager_,
                         timer_interval_),
      intFlowManager(intFlowManager_) {
}

ServiceStatsManager::~ServiceStatsManager() {
}

void ServiceStatsManager::start() {
    LOG(DEBUG) << "Starting service stats manager ("
               << timer_interval << " ms)";
    PolicyStatsManager::start();
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        timer->async_wait(bind(&ServiceStatsManager::on_timer, this, error));
    }
}

void ServiceStatsManager::stop() {
    LOG(DEBUG) << "Stopping service stats manager";
    stopping = true;
    PolicyStatsManager::stop();
}

void ServiceStatsManager::on_timer(const error_code& ec) {
    if (ec) {
        std::lock_guard<std::mutex> lock(timer_mutex);
        // shut down the timer when we get a cancellation
        LOG(DEBUG) << "Resetting timer, error: " << ec.message(); 
        timer.reset();
        return;
    }

    // Request Switch Manager to provide flow entries
    {
        TableState::cookie_callback_t cb_func;
        cb_func = [this](uint64_t cookie, uint16_t priority,
                         const struct match& match) {
            updateFlowEntryMap(statsState, cookie, priority, match);
        };

        // Pod <--> Svc and * <--> svc-tgt stats handling based
        // on flows in STATS table
        std::lock_guard<std::mutex> lock(pstatMtx);

        // create flowcountermap entries for new flows
        switchManager.forEachCookieMatch(IntFlowManager::STATS_TABLE_ID,
                                         cb_func);

        // aggregate statsCounterMap based on FlowCounterState
        ServiceCounterMap_t statsCountersMap;
        on_timer_base(ec, statsState, statsCountersMap);

        // Update service stats objects. IntFlowManager/ServiceManager would
        // have already created the objects. If its not resolved,
        // then new objects will get created 
        updateServiceStatsObjects(&statsCountersMap);
    }

    {
        TableState::cookie_callback_t cb_func;
        cb_func = [this](uint64_t cookie, uint16_t priority,
                         const struct match& match) {
            updateFlowEntryMap(svhState, cookie, priority, match);
        };

        // svc-tgt rx stats handling based on flows in SERVICE_NEXTHOP table
        std::lock_guard<std::mutex> lock(pstatMtx);

        // create flowcountermap entries for new flows
        switchManager.forEachCookieMatch(IntFlowManager::SERVICE_NEXTHOP_TABLE_ID,
                                         cb_func);

        // aggregate statsCounterMap based on FlowCounterState
        ServiceCounterMap_t svhCountersMap;
        on_timer_base(ec, svhState, svhCountersMap);

        // Update service stats objects. ServiceManager would
        // have already created the objects.
        updateServiceStatsObjects(&svhCountersMap);
    }

    {
        TableState::cookie_callback_t cb_func;
        cb_func = [this](uint64_t cookie, uint16_t priority,
                         const struct match& match) {
            updateFlowEntryMap(svrState, cookie, priority, match);
        };

        // svc-tgt tx stats handling based on flows in SERVICE_REV table
        std::lock_guard<std::mutex> lock(pstatMtx);

        // create flowcountermap entries for new flows
        switchManager.forEachCookieMatch(IntFlowManager::SERVICE_REV_TABLE_ID,
                                         cb_func);

        // aggregate svrCounterMap based on FlowCounterState
        ServiceCounterMap_t svrCountersMap;
        on_timer_base(ec, svrState, svrCountersMap);

        // Update service stats objects. ServiceManager would
        // have already created the objects.
        updateServiceStatsObjects(&svrCountersMap);
    }

    sendRequest(IntFlowManager::STATS_TABLE_ID);
    sendRequest(IntFlowManager::SERVICE_NEXTHOP_TABLE_ID);
    sendRequest(IntFlowManager::SERVICE_REV_TABLE_ID);
    if (!stopping) {
        std::lock_guard<std::mutex> lock(timer_mutex);
        if (timer) {
            timer->expires_from_now(milliseconds(timer_interval));
            timer->async_wait(bind(&ServiceStatsManager::on_timer, this, error));
        }
    }
}

// update statsCounterMap based on FlowCounterState
void ServiceStatsManager::
on_timer_base(const error_code& ec,
              flowCounterState_t& counterState,
              ServiceCounterMap_t& statsCountersMap) {

    // Walk through all the old map entries that have
    // been visited.
    for (auto& i : counterState.oldFlowCounterMap) {
        const FlowEntryMatchKey_t& flowEntryKey = i.first;
        FlowCounters_t& newFlowCounters = i.second;
        // Have we visited this flow entry yet
        if (!newFlowCounters.visited) {
            // increase age by polling interval
            newFlowCounters.age += 1;
            if (newFlowCounters.age >= MAX_AGE) {
                LOG(DEBUG) << "Unvisited entry for last " << MAX_AGE
                           << " polling intervals: "
                           << flowEntryKey.cookie << ", "
                           << *flowEntryKey.match;
            }
            continue;
        }

        // Have we collected non-zero diffs for this flow entry
        if (newFlowCounters.diff_packet_count &&
            newFlowCounters.diff_packet_count.get() != 0) {

            ServiceFlowMatchKey_t flowMatchKey(flowEntryKey.cookie);

            FlowStats_t&  newStatsCounters =
                                statsCountersMap[flowMatchKey];

            // We get multiple flow stats entries for same
            // cookie, ie for each pod<-->svc, multiple flow
            // entries may get installed in OVS. This is because
            // each pod/svc can contain multiple IPs. statsCountersMap
            // is used for the purpose of adding counters of such flows
            // that have same cookie.

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (newStatsCounters.packet_count) {
                // get existing packet_count and byte_count
                packet_count = newStatsCounters.packet_count.get();
                byte_count = newStatsCounters.byte_count.get();
            }

            // Add counters for new flow entry to existing
            // packet_count and byte_count
            if (newFlowCounters.diff_packet_count) {
                newStatsCounters.packet_count =
                    make_optional(true,
                                  newFlowCounters.diff_packet_count.get() +
                                  packet_count);
                newStatsCounters.byte_count =
                    make_optional(true,
                                  newFlowCounters.diff_byte_count.get() +
                                  byte_count);
            }

            // reset the per flow entry diff counters to zero.
            newFlowCounters.diff_packet_count = make_optional(true, 0);
            newFlowCounters.diff_byte_count = make_optional(true, 0);
            // set the age of this entry as zero as we have seen
            // its counter increment last polling cycle.
            newFlowCounters.age = 0;
        }
        // Set entry visited as false as we have consumed its diff
        // counters.  When we visit this entry when handling a
        // FLOW_STATS_REPLY corresponding to this entry, we mark this
        // entry as visited again.
        newFlowCounters.visited = false;
    }

    // Walk through all the removed flow map entries
    for (auto& i : counterState.removedFlowCounterMap) {
        const FlowEntryMatchKey_t& remFlowEntryKey = i.first;
        FlowCounters_t&  remFlowCounters = i.second;

        // Have we collected non-zero diffs for this removed flow entry
        if (remFlowCounters.diff_packet_count) {

            ServiceFlowMatchKey_t flowMatchKey(remFlowEntryKey.cookie);

            FlowStats_t& newStatsCounters =
                statsCountersMap[flowMatchKey];

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (newStatsCounters.packet_count) {
                // get existing packet_count and byte_count
                packet_count = newStatsCounters.packet_count.get();
                byte_count = newStatsCounters.byte_count.get();
            }

            // Add counters for flow entry to be removed
            newStatsCounters.packet_count =
                make_optional(true,
                              (remFlowCounters.diff_packet_count)
                              ? remFlowCounters.diff_packet_count.get()
                              : 0 + packet_count);
            newStatsCounters.byte_count =
                make_optional(true,
                              (remFlowCounters.diff_byte_count)
                              ? remFlowCounters.diff_byte_count.get()
                              : 0 + byte_count);
        }
    }

    // Delete all the entries from removedFlowCountersMap
    // Since we have taken them into account in our calculations for
    // per classifier/epg pair counters.

    counterState.removedFlowCounterMap.clear();

    // Walk through all the old map entries and remove those entries
    // that have not been visited but age is equal to MAX_AGE times
    // polling interval.

    for (auto itr = counterState.oldFlowCounterMap.begin();
         itr != counterState.oldFlowCounterMap.end();) {
        FlowCounters_t& flowCounters = itr->second;
        // Have we visited this flow entry yet
        if (!flowCounters.visited && (flowCounters.age >= MAX_AGE)) {
            itr = counterState.oldFlowCounterMap.erase(itr);
        } else
            itr++;
    }

    // Walk through all the new map entries and remove those entries
    // that have age equal to MAX_AGE times polling interval.
    for (auto itr = counterState.newFlowCounterMap.begin();
         itr != counterState.newFlowCounterMap.end();) {
        FlowCounters_t& flowCounters = itr->second;
        // Have we visited this flow entry yet
        if (flowCounters.age >= MAX_AGE) {
            itr = counterState.newFlowCounterMap.erase(itr);
        } else
            itr++;
    }
}

// Generate/update Pod <--> Svc stats objects
void ServiceStatsManager::
updateServiceStatsObjects(ServiceCounterMap_t *newCountersMap) {
    
    // walk through newCountersMap to update new set of MOs
    for (ServiceCounterMap_t:: iterator itr = newCountersMap->begin();
         itr != newCountersMap->end();
         itr++) {
        
        const ServiceFlowMatchKey_t& flowKey = itr->first;
        FlowStats_t&  newCounters = itr->second;
        if (newCounters.packet_count.get() != 0) {
            intFlowManager.updateSvcStatsCounters(
                                     flowKey.cookie,
                                     newCounters.packet_count.get(),
                                     newCounters.byte_count.get());
        } 
    }
}

bool ServiceStatsManager::ServiceFlowMatchKey_t::
operator==(const ServiceFlowMatchKey_t &other) const {
    return (cookie == other.cookie);
}

size_t ServiceStatsManager::ServiceKeyHasher::
operator()(const ServiceStatsManager::ServiceFlowMatchKey_t& k) const noexcept {
    using boost::hash_value;
    using boost::hash_combine;

    std::size_t seed = 0;
    hash_combine(seed, hash_value(k.cookie));
    return (seed);
}

void ServiceStatsManager::Handle(SwitchConnection* connection,
                                int msgType, ofpbuf *msg,
                                struct ofputil_flow_removed *fentry)
{
    handleMessage(msgType, msg,
                  [this](uint32_t table_id) -> flowCounterState_t* {
                      if (table_id == IntFlowManager::STATS_TABLE_ID)
                          return &statsState;
                      else if (table_id == IntFlowManager::SERVICE_NEXTHOP_TABLE_ID)
                          return &svhState;
                      else if (table_id == IntFlowManager::SERVICE_REV_TABLE_ID)
                          return &svrState;
                      else
                          return NULL;
                  }, fentry);
}

} /* namespace opflexagent */
