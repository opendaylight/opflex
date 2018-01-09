/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "SwitchManager.h"
#include "FlowBuilder.h"
#include <opflexagent/logging.h>

#include <boost/asio/placeholders.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "ovs-ofputil.h"

namespace opflexagent {

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;
using boost::asio::deadline_timer;
using boost::posix_time::milliseconds;
using boost::asio::placeholders::error;

const long DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC = 5000;

SwitchManager::SwitchManager(Agent& agent_,
                             FlowExecutor& flowExecutor_,
                             FlowReader& flowReader_,
                             PortMapper& portMapper_)
    : agent(agent_),
      flowExecutor(flowExecutor_),
      flowReader(flowReader_),
      portMapper(portMapper_), stateHandler(NULL),
      connectDelayMs(DEFAULT_SYNC_DELAY_ON_CONNECT_MSEC),
      stopping(false), syncEnabled(false), syncing(false),
      syncInProgress(false), syncPending(false) {

}

void SwitchManager::start(const std::string& swName) {
    connection.reset(new SwitchConnection(swName));
    portMapper.InstallListenersForConnection(connection.get());
    flowExecutor.InstallListenersForConnection(connection.get());
    flowReader.installListenersForConnection(connection.get());

    // Start out in syncing mode to avoid writing to the flow tables;
    // we'll update cached state only.
    syncing = true;
}

void SwitchManager::connect() {
    connection->RegisterOnConnectListener(this);
    connection->Connect(OFP13_VERSION);
}

void SwitchManager::stop() {
    stopping = true;

    if (connection) {
        flowReader.uninstallListenersForConnection(connection.get());
        flowExecutor.UninstallListenersForConnection(connection.get());
        portMapper.UninstallListenersForConnection(connection.get());
        connection->UnregisterOnConnectListener(this);
    }

    if (connectTimer) {
        connectTimer->cancel();
    }
}

void SwitchManager::setMaxFlowTables(int max) {
    flowTables.resize(max);
    recvFlows.resize(max);
    tableDone.resize(max);
}

void SwitchManager::enableSync() {
    if (stopping) return;

    if (!syncEnabled) {
        syncEnabled = true;
        LOG(INFO)
            << "[" << (connection ? connection->getSwitchName() : "(none)")
            << "] Switch state synchronization enabled";

        // Set a deadline for syncing of switch state. If we get
        // connected to the switch before that, then we'll wait till
        // the deadline expires before attempting to sync.
        if (connectDelayMs > 0) {
            connectTimer
                .reset(new deadline_timer(agent.getAgentIOService(),
                                          milliseconds(connectDelayMs)));
        }
        // Pretend that we just got connected to the switch to schedule sync
        if (connection && connection->IsConnected()) {
            agent.getAgentIOService()
                .dispatch(bind(&SwitchManager::handleConnection, this,
                               connection.get()));
        }
    }
}

void SwitchManager::registerStateHandler(SwitchStateHandler* handler) {
    stateHandler = handler;
}

void SwitchManager::setSyncDelayOnConnect(long delay) {
    connectDelayMs = delay;
}

void SwitchManager::Connected(SwitchConnection *swConn) {
    if (stopping) return;
    agent.getAgentIOService()
        .dispatch(bind(&SwitchManager::handleConnection, this, swConn));
}

void SwitchManager::handleConnection(SwitchConnection *sw) {
    flowReader.clear();
    syncInProgress = false;
    syncPending = false;

    if (syncEnabled) {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Handling new connection to switch";
    } else {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Opflex sync not yet enabled, ignoring new "
            "connection to switch";
        return;
    }

    if (connectTimer) {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Sync state with switch will begin in "
                   << connectTimer->expires_from_now();
        connectTimer->async_wait(bind(&SwitchManager::onConnectTimer,
                                      this, error));
    } else {
        onConnectTimer(boost::system::error_code());
    }
}

void SwitchManager::onConnectTimer(const boost::system::error_code& ec) {
    connectTimer.reset();
    if (stopping) return;
    if (!ec)
        initiateSync();
}

bool SwitchManager::writeFlow(const std::string& objId, int tableId,
                              FlowEntryList& el) {
    bool success = true;

    assert(tableId >= 0 &&
           static_cast<size_t>(tableId) < flowTables.size());
    for (FlowEntryPtr& fe : el)
        fe->entry->table_id = tableId;
    TableState& tab = flowTables[tableId];

    FlowEdit diffs;
    tab.apply(objId, el, diffs);
    if (!syncing) {
        // If a sync is in progress, don't write to the flow tables
        // while we are reading and reconciling with the current
        // flows.
        if (!(success = flowExecutor.Execute(diffs))) {
            LOG(ERROR) << "[" << connection->getSwitchName() << "] "
                       << "Writing flows for " << objId << " failed";

        }
    }
    el.clear();

    return success;
}

bool SwitchManager::writeFlow(const std::string& objId,
                              int tableId, FlowEntryPtr el) {
    FlowEntryList tmpEl;
    if (el)
        tmpEl.push_back(el);
    return writeFlow(objId, tableId, tmpEl);
}

bool SwitchManager::writeFlow(const std::string& objId,
                              int tableId, FlowBuilder& fb) {
    return writeFlow(objId, tableId, fb.build());
}

bool SwitchManager::clearFlows(const std::string& objId, int tableId) {
    FlowEntryList empty;
    return writeFlow(objId, tableId, empty);
}

bool SwitchManager::writeGroupMod(const GroupEdit::Entry& e) {
    // If a sync is in progress, don't write to the group table while
    // we are reading and reconciling with the current groups.
    if (syncing) {
        return true;
    }

    GroupEdit ge;
    ge.edits.push_back(e);
    bool success = flowExecutor.Execute(ge);
    if (!success) {
        LOG(ERROR) << "[" << connection->getSwitchName() << "] "
                   << "Group mod failed for group-id=" << e->mod->group_id;
    }
    return success;
}

void SwitchManager::diffTableState(int tableId, const FlowEntryList& el,
                                   /* out */ FlowEdit& diffs) {
    const TableState& tab = flowTables[tableId];
    tab.diffSnapshot(el, diffs);
}

void SwitchManager::forEachCookieMatch(int tableId,
                                       TableState::cookie_callback_t& cb) {
    const TableState& tab = flowTables[tableId];
    tab.forEachCookieMatch(cb);
}

void SwitchManager::initiateSync() {
    if (syncInProgress) {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Sync is already in progress, marking it as pending";
        syncPending = true;
        return;
    }
    syncInProgress = true;
    syncPending = false;
    syncing = true;
    LOG(INFO) << "[" << connection->getSwitchName() << "] "
              << "Sync initiated";

    clearSyncState();

    flowReader.getGroups(bind(&SwitchManager::gotGroups, this, _1, _2));

    for (size_t i = 0; i < flowTables.size(); ++i)
        flowReader.getFlows(i, bind(&SwitchManager::gotFlows, this, i, _1, _2));
}

void SwitchManager::gotGroups(const GroupEdit::EntryList& groups,
    bool done) {
    for (const GroupEdit::Entry& e : groups) {
        recvGroups[e->mod->group_id] = e;
    }
    groupsDone = done;
    if (done) {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Got all groups, #groups=" << recvGroups.size();
        checkRecvDone();
    }
}

void SwitchManager::gotFlows(int tableId, const FlowEntryList& flows,
                             bool done) {
    assert(tableId >= 0 &&
           static_cast<size_t>(tableId) < flowTables.size());

    FlowEntryList& fl = recvFlows[tableId];
    fl.insert(fl.end(), flows.begin(), flows.end());
    tableDone[tableId] = done;
    if (done) {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Got all entries for table=" << tableId
                   << ", #flows=" << fl.size();
        checkRecvDone();
    }
}

void SwitchManager::checkRecvDone() {
    bool allDone = groupsDone;
    for (size_t i = 0; allDone && i < flowTables.size(); ++i) {
        allDone = allDone && tableDone[i];
    }

    if (allDone) {
        LOG(DEBUG) << "[" << connection->getSwitchName() << "] "
                   << "Got all group and flow tables, starting reconciliation";
        agent.getAgentIOService()
            .dispatch(bind(&SwitchManager::completeSync, this));
    }
}

void SwitchManager::completeSync() {
    assert(syncInProgress == true);
    if (stateHandler) {
        GroupEdit ge = stateHandler->reconcileGroups(recvGroups);
        bool success = flowExecutor.Execute(ge);
        if (!success) {
            LOG(ERROR) << "[" << connection->getSwitchName() << "] "
                       << "Failed to execute group table changes";
        }

        std::vector<FlowEdit> diffs =
            stateHandler->reconcileFlows(flowTables, recvFlows);
        for (size_t i = 0; i < flowTables.size(); ++i) {
            bool success = flowExecutor.Execute(diffs[i]);
            if (!success) {
                LOG(ERROR) << "[" << connection->getSwitchName() << "] "
                           << "Failed to execute diffs on table=" << i;
            }
        }
    }

    clearSyncState();

    if (stateHandler) {
        stateHandler->completeSync();
    }
    syncInProgress = false;
    syncing = false;

    LOG(INFO) << "[" << connection->getSwitchName() << "] "
              <<"Sync complete";

    if (syncPending) {
        agent.getAgentIOService()
            .dispatch(bind(&SwitchManager::initiateSync, this));
    }
}

void SwitchManager::clearSyncState() {
    for (size_t i = 0; i < flowTables.size(); ++i) {
        recvFlows[i].clear();
        tableDone[i] = false;
    }
    recvGroups.clear();
    groupsDone = false;
}

} // namespace opflexagent
