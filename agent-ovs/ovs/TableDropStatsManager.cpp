/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for TableDropStatsManager class.
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/logging.h>
#include "FlowConstants.h"
#include "IntFlowManager.h"
#include "AccessFlowManager.h"
#include <opflexagent/IdGenerator.h>
#include <opflexagent/Agent.h>
#include "TableState.h"
#include "TableDropStatsManager.h"

#include "ovs-ofputil.h"
#ifdef HAVE_PROMETHEUS_SUPPORT
#include <opflexagent/PrometheusManager.h>
#endif
extern "C" {
#include <openvswitch/ofp-msgs.h>
}

#include <opflex/modb/URI.h>
#include <opflex/modb/Mutator.h>
#include <modelgbp/observer/PolicyStatUniverse.hpp>

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


void BaseTableDropStatsManager::updateDropFlowStatsCounters(
        flowCounterState_t& counterState, uint64_t cookie, uint16_t priority,
        const struct match& match) {
    FlowEntryMatchKey_t flowEntryKey(cookie, priority, match);

    if((ovs_htonll(cookie) & flow::cookie::TABLE_DROP_FLOW) !=
            flow::cookie::TABLE_DROP_FLOW) {
        return;
    }

    /* check if Stats Manager has it in its oldFlowCounterMap */
    if (counterState.oldFlowCounterMap.find(flowEntryKey) !=
        counterState.oldFlowCounterMap.end())
        return;

    /* check if Stats Manager has it in its newFlowCounterMap */
    auto it = counterState.newFlowCounterMap.find(flowEntryKey);
    if (it != counterState.newFlowCounterMap.end()) {
        it->second.age += 1;
        return;
    }

    /* Add the flow entry to newmap */
    FlowCounters_t& flowCounters =
        counterState.newFlowCounterMap[flowEntryKey];
    flowCounters.visited = false;
    flowCounters.age = 0;
    flowCounters.last_packet_count = make_optional(false, 0);
    flowCounters.last_byte_count = make_optional(false, 0);
    flowCounters.diff_packet_count = make_optional(false, 0);
    flowCounters.diff_byte_count = make_optional(false, 0);

}

void BaseTableDropStatsManager::start(bool register_listener) {
    LOG(DEBUG) << "Starting "
               << connection->getSwitchName()
               << " Table Drop stats manager ("
               << timer_interval << " ms)";
    for (const auto& tbl_it: tableDescMap) {
        Mutator mutator(agent->getFramework(), "policyelement");
        optional<shared_ptr<PolicyStatUniverse> > su =
            PolicyStatUniverse::resolve(agent->getFramework());
        if (su) {
            su.get()->addGbpeTableDropCounter(getAgentUUID(),
                                              connection->getSwitchName(),
                                              tbl_it.first)
                ->setTableName(tbl_it.second.first)
                .setProbableDropReason(tbl_it.second.second)
                .setPackets(0)
                .setBytes(0);
       }
       mutator.commit();
#ifdef HAVE_PROMETHEUS_SUPPORT
       PrometheusManager &prometheusManager = agent->getPrometheusManager();
       prometheusManager.addTableDropGauge(connection->getSwitchName(),
                                            tbl_it.second.first);
#endif
       CurrentDropCounterState[tbl_it.first];
       auto &counter = TableDropCounterState[tbl_it.first];
       counter.packet_count = boost::make_optional(false, 0);
       counter.byte_count = boost::make_optional(false, 0);
    }
    PolicyStatsManager::start(register_listener);
    {
        std::lock_guard<std::mutex> lock(timer_mutex);
        timer->async_wait(bind(&BaseTableDropStatsManager::on_timer, this, error));
    }
}

void BaseTableDropStatsManager::stop(bool unregister_listener) {
    LOG(DEBUG) << "Stopping "
               << connection->getSwitchName()
               << " Table Drop stats manager";
#ifdef HAVE_PROMETHEUS_SUPPORT
    PrometheusManager &prometheusManager = agent->getPrometheusManager();
    for (const auto& tbl_it: tableDescMap) {
        prometheusManager.removeTableDropGauge(
                connection->getSwitchName(),
                tbl_it.second.first);
    }
#endif
    stopping = true;

    PolicyStatsManager::stop(unregister_listener);
}

// update statsCounterMap based on FlowCounterState
void BaseTableDropStatsManager::
on_timer_base(const boost::system::error_code& ec,
              flowCounterState_t& counterState,
              PolicyStatsManager::FlowStats_t& dropFlowCounterState) {

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

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (dropFlowCounterState.packet_count) {
                // get existing packet_count and byte_count
                packet_count = dropFlowCounterState.packet_count.get();
                byte_count = dropFlowCounterState.byte_count.get();
            }

            // Add counters for new flow entry to existing
            // packet_count and byte_count
            if (newFlowCounters.diff_packet_count) {
                dropFlowCounterState.packet_count =
                    make_optional(true,
                                  newFlowCounters.diff_packet_count.get() +
                                  packet_count);
                dropFlowCounterState.byte_count =
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
        FlowCounters_t&  remFlowCounters = i.second;

        // Have we collected non-zero diffs for this removed flow entry
        if (remFlowCounters.diff_packet_count) {

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (dropFlowCounterState.packet_count) {
                // get existing packet_count and byte_count
                packet_count = dropFlowCounterState.packet_count.get();
                byte_count = dropFlowCounterState.byte_count.get();
            }

            // Add counters for flow entry to be removed
            dropFlowCounterState.packet_count =
                make_optional(true,
                              (remFlowCounters.diff_packet_count)
                              ? remFlowCounters.diff_packet_count.get()
                              : 0 + packet_count);
            dropFlowCounterState.byte_count =
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


void BaseTableDropStatsManager::on_timer(const boost::system::error_code& ec) {
    if (ec) {
        std::lock_guard<std::mutex> lock(timer_mutex);
        // shut down the timer when we get a cancellation
        LOG(DEBUG) << "Resetting timer, error: " << ec.message();
        timer.reset();
        return;
    }

    for(const auto& tbl_it: tableDescMap) {
        uint64_t packet_count=0, byte_count=0;
        TableState::cookie_callback_t cb_func;
        cb_func = [this, tbl_it](uint64_t cookie, uint16_t priority,
                         const struct match& match) {
            updateDropFlowStatsCounters(
                    CurrentDropCounterState[tbl_it.first], cookie, priority,
                   match);
        };

        // Request Switch Manager to provide flow entries
        {
            std::lock_guard<std::mutex> lock(pstatMtx);
            switchManager.forEachCookieMatch(tbl_it.first,
                                             cb_func);
            on_timer_base(ec, CurrentDropCounterState[tbl_it.first],
                            TableDropCounterState[tbl_it.first]);

            if(TableDropCounterState[tbl_it.first].packet_count) {
                packet_count = TableDropCounterState[tbl_it.first].packet_count.get();
                byte_count = TableDropCounterState[tbl_it.first].byte_count.get();
            }
        }

        Mutator mutator(agent->getFramework(), "policyelement");
        optional<shared_ptr<PolicyStatUniverse> > su =
            PolicyStatUniverse::resolve(agent->getFramework());
        if (su) {
            su.get()->addGbpeTableDropCounter(getAgentUUID(),
                                 connection->getSwitchName(),
                                 tbl_it.first)
                ->setPackets(packet_count)
                .setBytes(byte_count);
        }
        mutator.commit();
#ifdef HAVE_PROMETHEUS_SUPPORT
        PrometheusManager &prometheusManager = agent->getPrometheusManager();
        prometheusManager.updateTableDropGauge(connection->getSwitchName(),
                                                tbl_it.second.first,
                                                byte_count,
                                                packet_count);
#endif

    }

    for(const auto& tbl_it: tableDescMap) {
        sendRequest(tbl_it.first, flow::cookie::TABLE_DROP_FLOW,
                flow::cookie::TABLE_DROP_FLOW);

    }
    if (!stopping) {
        std::lock_guard<std::mutex> lock(timer_mutex);
        if(timer) {
            timer->expires_from_now(milliseconds(timer_interval));
            timer->async_wait(bind(&BaseTableDropStatsManager::on_timer, this, error));
        }
    }
}

void BaseTableDropStatsManager::handleTableDropStats(struct ofputil_flow_stats* fentry) {
    if((fentry->cookie & flow::cookie::TABLE_DROP_FLOW) !=
            flow::cookie::TABLE_DROP_FLOW) {
        return;
    }
    PolicyStatsManager::flowCounterState_t &counterState =
            CurrentDropCounterState[fentry->table_id];
    updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                                          fentry->priority,
                                          (fentry->match),
                                          fentry->packet_count,
                                          fentry->byte_count,
                                          counterState, false);
}

void BaseTableDropStatsManager::objectUpdated(opflex::modb::class_id_t class_id,
                                         const opflex::modb::URI& uri) {
    /* Don't need to register for any object updates. Table drops are
     * not related to specific objects
     */
}

void BaseTableDropStatsManager::Handle(SwitchConnection* connection,
                                  int msgType,
                                  ofpbuf *msg,
                                  struct ofputil_flow_removed* fentry) {
    handleMessage(msgType, msg,
        [this](uint32_t table_id) -> flowCounterState_t* {
            if(tableDescMap.find(table_id)!= tableDescMap.end())
                return &CurrentDropCounterState[table_id];
            else
                return NULL;
        }, fentry);
}

} /* namespace opflexagent */
