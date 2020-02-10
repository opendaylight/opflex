/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for TableDropStatsManager class.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
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
    }
    PolicyStatsManager::start(register_listener);
    timer->async_wait(bind(&BaseTableDropStatsManager::on_timer, this, error));
}

void BaseTableDropStatsManager::stop(bool unregister_listener) {
    LOG(DEBUG) << "Stopping "
               << connection->getSwitchName()
               << " Table Drop stats manager";
    stopping = true;
    PolicyStatsManager::stop(unregister_listener);
}

void BaseTableDropStatsManager::on_timer(const error_code& ec) {
    if (ec) {
        // shut down the timer when we get a cancellation
        LOG(DEBUG) << "Resetting timer, error: " << ec.message();
        timer.reset();
        return;
    }

    for(const auto& tbl_it: tableDescMap) {
        sendRequest(tbl_it.first, flow::cookie::TABLE_DROP_FLOW);
    }
    if (!stopping && timer) {
        timer->expires_from_now(milliseconds(timer_interval));
        timer->async_wait(bind(&BaseTableDropStatsManager::on_timer, this, error));
    }
}

void BaseTableDropStatsManager::handleTableDropStats(struct ofputil_flow_stats* fentry) {
    if(fentry->cookie != flow::cookie::TABLE_DROP_FLOW) {
        return;
    }
    TableDropCounters_t& oldCounters =
            TableDropCounterState[fentry->table_id];
    TableDropCounters_t diffCounters;

    if (oldCounters.packet_count) {
        diffCounters.packet_count = fentry->packet_count -
            oldCounters.packet_count.get();
        diffCounters.byte_count = fentry->byte_count -
            oldCounters.byte_count.get();
    } else {
        diffCounters.packet_count = fentry->packet_count;
        diffCounters.byte_count = fentry->byte_count;
    }

    oldCounters.packet_count = fentry->packet_count;
    oldCounters.byte_count = fentry->byte_count;

    if (diffCounters.packet_count) {
        Mutator mutator(agent->getFramework(), "policyelement");
        optional<shared_ptr<PolicyStatUniverse> > su =
            PolicyStatUniverse::resolve(agent->getFramework());
        if (su) {
            su.get()->addGbpeTableDropCounter(getAgentUUID(),
                                 connection->getSwitchName(),
                                 fentry->table_id)
                ->setPackets(diffCounters.packet_count.get())
                .setBytes(diffCounters.byte_count.get());
        }
        mutator.commit();
    }

}

void BaseTableDropStatsManager::objectUpdated(opflex::modb::class_id_t class_id,
                                         const opflex::modb::URI& uri) {
    /* Don't need to register for any object updates. Table drops are
     * not related to specific objects
     */
}

void BaseTableDropStatsManager::Handle(SwitchConnection* connection,
                                  int msgType, ofpbuf *msg) {
    handleMessage(msgType, msg,
                  [this](uint32_t table_id) -> flowCounterState_t* {
                      return &UnusedFlowCounterState[table_id];
                  });
}

} /* namespace opflexagent */
