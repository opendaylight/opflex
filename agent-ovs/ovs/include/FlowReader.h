/*
 * Definition of FlowReader class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_FLOWREADER_H_
#define OPFLEXAGENT_FLOWREADER_H_

#include "TableState.h"
#include "SwitchConnection.h"

#include <unordered_map>
#include <mutex>
#include <functional>

struct match;

namespace opflexagent {

/**
 * @brief Class that allows you to read flow and group tables from a switch.
 */
class FlowReader : public MessageHandler {
public:
    /**
     * Default constructor
     */
    FlowReader();
    virtual ~FlowReader();

    /**
     * Register all the necessary event listeners on connection.
     * @param conn Connection to register
     */
    void installListenersForConnection(SwitchConnection *conn);

    /**
     * Unregister all event listeners from connection.
     * @param conn Connection to unregister from
     */
    void uninstallListenersForConnection(SwitchConnection *conn);

    /**
     * Callback function to process a list of flow-table entries.
     */
    typedef std::function<void (const FlowEntryList&, bool)> FlowCb;

    /**
     * Get the flow-table entries for specified table.
     *
     * @param tableId ID of flow-table to read
     * @param cb Callback function to invoke when flow-entries are
     * received
     * @return true if request for getting flows was sent successfully
     */
    virtual bool getFlows(uint8_t tableId, const FlowCb& cb);

    /**
     * Get the flow-table entries for specified table.
     *
     * @param tableId ID of flow-table to read
     * @param m a match to request for the flows
     * @param cb Callback function to invoke when flow-entries are
     * received
     * @return true if request for getting flows was sent successfully
     */
    virtual bool getFlows(uint8_t tableId, struct match *m,
                          const FlowCb& cb);

    /**
     * Callback function to process a list of group-table entries.
     */
    typedef std::function<void (const GroupEdit::EntryList&, bool)> GroupCb;

    /**
     * Get the group-table entries from switch.
     *
     * @param cb Callback function to invoke when group-entries are
     * received
     * @return true if request for getting groups was sent successfully
     */
    virtual bool getGroups(const GroupCb& cb);

    /**
     * Callback function to process a list of tlv-table entries.
     */
    typedef std::function<void (const TlvEntryList&, bool)> TlvCb;

    /**
     * Get the tlv-table entries from switch.
     *
     * @param cb Callback function to invoke when tlv-entries are
     * received
     * @return true if request for getting tlvs was sent successfully
     */
    virtual bool getTlvs(const TlvCb& cb);

    /* Interface: MessageHandler */
    void Handle(SwitchConnection *c,
                int msgType,
                ofpbuf *msg,
                struct ofputil_flow_removed *fentry=NULL);

    /**
     * Clear the state of the flow reader
     */
    void clear();

private:
    /**
     * Create a request for reading all entries of specified table.
     *
     * @param tableId ID of flow-table to read
     * @param m A match to request, or NULL to match all
     * all
     * @return flow-table read request
     */
    OfpBuf createFlowRequest(uint8_t tableId, struct match* m = NULL);

    /**
     * Create a request for reading all entries of group-table.
     *
     * @return group-table read request
     */
    OfpBuf createGroupRequest();

    /**
     * Create a request for reading all entries of tlv-table.
     *
     * @return tlv-table read request
     */
    OfpBuf createTlvRequest();

    /**
     * Send specified read request on the connection and update
     * internal structures to track replies.
     *
     * @param msg Read request to send
     * @param cb Callback to invoke when replies are received
     * @param reqMap Map to track requests and callbacks
     * @return true if request was sent successfully
     */
    template <typename U, typename V>
    bool sendRequest(OfpBuf& msg, const U& cb, V& reqMap);

    /**
     * Process the reply message received for a read request
     * and invoke the callback registered for the request.
     *
     * @param msg The reply message
     * @param reqMap Map to track requests and callbacks
     */
    template<typename T, typename U, typename V>
    void handleReply(ofpbuf *msg, V& reqMap);

    /**
     * Extract flow/group entries from the received message.
     *
     * @param msg The received reply message
     * @param recv Container to put the extracted entries into
     * @param replyDone Set to true if no more replies are
     * expected for this request
     */
    template<typename T>
    void decodeReply(ofpbuf *msg, T& recv, bool& replyDone);

    SwitchConnection *swConn;

    std::mutex reqMtx;

    typedef std::unordered_map<uint32_t, FlowCb> FlowCbMap;
    FlowCbMap flowRequests;
    typedef std::unordered_map<uint32_t, GroupCb> GroupCbMap;
    GroupCbMap groupRequests;
    typedef std::unordered_map<uint32_t, TlvCb> TlvCbMap;
    TlvCbMap tlvRequests;
};

}   // namespace opflexagent

#endif /* OPFLEXAGENT_FLOWREADER_H_ */
