/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SwitchManager
 *
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_SWITCHMANAGER_H_
#define OPFLEXAGENT_SWITCHMANAGER_H_

#include "SwitchConnection.h"
#include "FlowExecutor.h"
#include "FlowReader.h"
#include "PortMapper.h"
#include <opflexagent/Agent.h>
#include <opflexagent/IdGenerator.h>
#include "SwitchStateHandler.h"

#include <boost/noncopyable.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <string>
#include <memory>

namespace opflexagent {

class FlowBuilder;

/**
 * Manage the state for a given OVS bridge.  Handles connection state
 * and flow table state.
 */
class SwitchManager : public OnConnectListener,
                      private boost::noncopyable {
public:
    /**
     * Construct a new switch manager
     *
     * @param agent the agent
     * @param flowExecutor the flow executor to use
     * @param flowReader the flow reader to use
     * @param portMapper the port mapper to use
     */
    SwitchManager(Agent& agent,
                  FlowExecutor& flowExecutor,
                  FlowReader& flowReader,
                  PortMapper& portMapper);

    /**
     * Start the switch manager and initiate the connection to the
     * switch.
     *
     * @param swName the name of the OVS bridge to manager
     */
    virtual void start(const std::string& swName);

    /**
     * Connect to the switch
     */
    virtual void connect();

    /**
     * Stop the switch manager
     */
    virtual void stop();

    /**
     * Set the number of flow tables that will be managed by this
     * switch manager.
     *
     * @param max the number of flow tables to set
     */
    void setMaxFlowTables(int max);

    /**
     * Enable state synchronization with the switch.  The
     * synchronization will occur once the switch is connected after
     * the synchronization delay has elapsed.
     */
    void enableSync();

    /**
     * Register a handler for switch state events
     * @param handler the handler
     */
    void registerStateHandler(SwitchStateHandler* handler);

    /**
     * Set the delay after connection before syncing starts
     * @param delay the delay in milliseconds
     */
    void setSyncDelayOnConnect(long delay);

    /* Interface: OnConnectListener */
    virtual void Connected(SwitchConnection *swConn);

    /**
     * Check whether the switch is current syncing its flow table
     * state
     */
    bool isSyncing() { return syncing; }

    /**
     * Get the current connection.  Will be NULL if the switch manager
     * is not started
     *
     * @return the connection
     */
    SwitchConnection* getConnection() { return connection.get(); }

    /**
     * Get the port mapper for this switch
     */
    PortMapper& getPortMapper() { return portMapper; }

    /**
     * Get the flow reader for this switch
     */
    FlowReader& getFlowReader() { return flowReader; }

    /**
     * Write the given flow list to the flow table
     *
     * @param objId the ID for the object associated with the flow
     * @param tableId the tableId for the flow table
     * @param el the list of flows to write
     */
    bool writeFlow(const std::string& objId, int tableId, FlowEntryList& el);

    /**
     * Write the given flow entry to the flow table
     *
     * @param objId the ID for the object associated with the flow
     * @param tableId the tableId for the flow table
     * @param fb a flow builder to use to build and write the flow
     */
    bool writeFlow(const std::string& objId, int tableId, FlowBuilder& fb);

    /**
     * Clear the flow entries for the given object ID.
     *
     * @param objId the ID for the object associated with the flow
     * @param tableId the tableId for the flow table
     */
    bool clearFlows(const std::string& objId, int tableId);

    /**
     * Write the given flow entry to the flow table
     *
     * @param objId the ID for the object associated with the flow
     * @param tableId the tableId for the flow table
     * @param e the list of flows to write
     */
    bool writeFlow(const std::string& objId, int tableId, FlowEntryPtr e);

    /**
     * Write a group-table change to the switch
     *
     * @param entry Change to the group-table entry
     * @return true is successful, false otherwise
     */
    bool writeGroupMod(const GroupEdit::Entry& entry);

    /**
     * Compare the state of a give table against the provided
     * snapshot.
     *
     * @param tableId the table to compare against
     * @param el the list of flows to compare against
     * @param diffs returns the differences between the entries
     */
    void diffTableState(int tableId, const FlowEntryList& el,
                        /* out */ FlowEdit& diffs);

    /**
     * Call the callback synchronously for each unique cookie and flow
     * table match in the flow table.
     *
     * @param tableId the table to check
     * @param cb the callback to call
     */
    void forEachCookieMatch(int tableId, TableState::cookie_callback_t& cb);

protected:
    /**
     * Connection for this switch
     */
    std::unique_ptr<SwitchConnection> connection;

private:
    /**
     * Begin reconciliation by reading all the flows and groups from the
     * switch.
     */
    void initiateSync();

    /**
     * Compare flows/groups read from switch to determine the differences
     * and make modification to eliminate those differences.
     */
    void completeSync();

    /**
     * Callback function provided to FlowReader to process received
     * flow table entries.
     */
    void gotFlows(int tableNum, const FlowEntryList& flows,
                  bool done);

    /**
     * Callback function provided to FlowReader to process received
     * group table entries.
     */
    void gotGroups(const GroupEdit::EntryList& groups,
                   bool done);

    /**
     * Determine if all flows/groups were received; starts reconciliation
     * if so.
     */
    void checkRecvDone();

    /**
     * Clear the sync state
     */
    void clearSyncState();

    Agent& agent;
    FlowExecutor& flowExecutor;
    FlowReader& flowReader;
    PortMapper& portMapper;
    SwitchStateHandler* stateHandler;

    // table state
    std::vector<TableState> flowTables;

    // connection state
    void handleConnection(SwitchConnection *sw);
    void onConnectTimer(const boost::system::error_code& ec);
    std::unique_ptr<boost::asio::deadline_timer> connectTimer;
    long connectDelayMs;

    // sync state
    bool stopping;
    bool syncEnabled;
    bool syncing;
    bool syncInProgress;
    bool syncPending;

    std::vector<FlowEntryList> recvFlows;
    std::vector<bool> tableDone;

    SwitchStateHandler::GroupMap recvGroups;
    bool groupsDone;
};

} // namespace opflexagent

#endif // OPFLEXAGENT_SWITCHMANAGER_H_
