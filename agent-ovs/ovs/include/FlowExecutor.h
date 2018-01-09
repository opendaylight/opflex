/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_FLOWEXECUTOR_H_
#define OPFLEXAGENT_FLOWEXECUTOR_H_

#include "SwitchConnection.h"
#include "TableState.h"

#include <boost/optional.hpp>

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace opflexagent {

/**
 * @brief Class that can execute a set of OpenFlow
 * table modifications.
 */
class FlowExecutor : public MessageHandler,
                     public OnConnectListener {
public:
    /**
     * Default constructor
     */
    FlowExecutor();
    virtual ~FlowExecutor();

    /**
     * Construct and send flow-modification messages corresponding
     * to the flow-edits specified. Waits till all the messages
     * have been acted upon (through a barrier message).
     * @param fe The flow modifications
     * @return false if any error occurs while sending messages or
     * an error reply was received, true otherwise
     */
    virtual bool Execute(const FlowEdit& fe);

    /**
     * Construct and send group-modification messages corresponding
     * to the group-edits specified. Waits till all the messages
     * have been acted upon (through a barrier message).
     * @param ge The group modifications
     * @return false if any error occurs while sending messages or
     * an error reply was received, true otherwise
     */
    virtual bool Execute(const GroupEdit& ge);

    /**
     * Construct and send flow-modification messages corresponding
     * to the flow-edits specified, but does not wait the messages
     * to be acted upon.
     * @param fe The flow modifications
     * @return false if any error occurs while sending messages,
     * true otherwise
     */
    virtual bool ExecuteNoBlock(const FlowEdit& fe);

    /**
     * Construct and send group-modification messages corresponding
     * to the group-edits specified, but does not wait the messages
     * to be acted upon.
     * @param ge The group modifications
     * @return false if any error occurs while sending messages,
     * true otherwise
     */
    virtual bool ExecuteNoBlock(const GroupEdit& ge);

    /**
     * Register all the necessary event listeners on connection.
     * @param conn Connection to register
     */
    void InstallListenersForConnection(SwitchConnection *conn);

    /**
     * Unregister all event listeners from connection.
     * @param conn Connection to unregister from
     */
    void UninstallListenersForConnection(SwitchConnection *conn);

    /** Interface: MessageHandler */
    void Handle(SwitchConnection *conn, int msgType, ofpbuf *msg);

    /** Interface: OnConnectListener */
    void Connected(SwitchConnection *swConn);

    /**
     * Construct a flow-modification message for the specified flow-edit.
     * @param edit The flow modification
     * @param ofVersion OpenFlow version to use for encoding
     * @return flow-modification message
     */
    static ofpbuf *EncodeFlowMod(const FlowEdit::Entry& edit,
                                 int ofVersion);

    /**
     * Construct a group-modification message for the specified group-edit.
     * @param edit The group modification
     * @param ofVersion OpenFlow version to use for encoding
     * @return group-modification message
     */
    static ofpbuf *EncodeGroupMod(const GroupEdit::Entry& edit,
                                  int ofVersion);
private:
    /**
     * Internal helper function to execute blocking flow/group-edits.
     *
     * @param fe The flow/group modification
     * @return true on success, false otherwise
     */
    template<typename T>
    bool ExecuteInt(const T& fe);

    /**
     * Internal helper function to execute non-blocking flow/group-edits.
     *
     * @param fe The flow/group modification
     * @return true on success, false otherwise
     */
    template<typename T>
    bool ExecuteIntNoBlock(const T& fe);

    /**
     * Construct and send flow-modification messages corresponding
     * to the edits specified and optionally associate them with
     * a barrier request.
     * @param fe The flow/group modifications
     * @param barrXid ID of barrier request to associate with
     * @return 0 on success, error code if any error occurs while
     * sending messages
     */
    template<typename T>
    int DoExecuteNoBlock(const T& fe,
            const boost::optional<uint32_t>& barrXid);

    /**
     * Internal helper function to construct an OpenFlow message from
     * a flow/group edit.
     *
     * @param edit The flow/group modification
     * @param ofVersion OpenFlow version to use for encoding
     * @return flow/group-modification message
     */
    template<typename T>
    static
    ofpbuf *EncodeMod(const T& edit, int ofVersion);

    /**
     * Send the barrier request specified and wait for a reply.
     * @param barrReq Barrier request to send
     * @return 0 on success, error code if any error occurs while
     * sending barrier message or an error reply was received for any of
     * the associated messages
     */
    int WaitOnBarrier(ofpbuf *barrReq);

    SwitchConnection *swConn;

    /**
     * @brief Maintains information about outstanding requests that
     * need to be tracked.
     */
    struct RequestState {
        RequestState() : status(0), done(false) {}

        std::unordered_set<uint32_t> reqXids;
        int status;
        bool done;
    };
    /* Map of barrier request IDs to RequestState */
    typedef std::unordered_map<uint32_t, RequestState> RequestMap;
    RequestMap requests;

    std::mutex reqMtx;
    std::condition_variable reqCondVar;
};

} // namespace opflexagent


#endif // OPFLEXAGENT_FLOWEXECUTOR_H_
