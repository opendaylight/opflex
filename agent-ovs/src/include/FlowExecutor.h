/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_FLOWEXECUTOR_H_
#define _OPFLEX_ENFORCER_FLOWEXECUTOR_H_

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/optional.hpp>

#include "ovs.h"
#include "SwitchConnection.h"
#include "TableState.h"

namespace opflex {
namespace enforcer {

/**
 * @brief Class that can execute a set of OpenFlow
 * table modifications.
 */
class FlowExecutor : public MessageHandler {
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
    virtual bool Execute(const flow::FlowEdit& fe);

    /**
     * Construct and send flow-modification messages corresponding
     * to the flow-edits specified, but does not wait the messages
     * to be acted upon.
     * @param fe The flow modifications
     * @return false if any error occurs while sending messages,
     * true otherwise
     */
    virtual bool ExecuteNoBlock(const flow::FlowEdit& fe);

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
    void Handle(SwitchConnection *conn, ofptype msgType, ofpbuf *msg);

    /**
     * Construct a flow-modification message for the specified flow-edit.
     * @param edit The flow modification
     * @param ofVersion OpenFlow version to use for encoding
     * @return flow-modification message
     */
    static ofpbuf *EncodeFlowMod(const flow::FlowEdit::Entry& edit,
            ofp_version ofVersion);
private:
    /**
     * Construct and send flow-modification messages corresponding
     * to the flow-edits specified and optionally associate them with
     * a barrier request.
     * @param fe The flow modifications
     * @param barrXid ID of barrier request to associate with
     * @return 0 on success, error code if any error occurs while
     * sending messages
     */
    int DoExecuteNoBlock(const flow::FlowEdit& fe,
            const boost::optional<ovs_be32>& barrXid);

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

        boost::unordered_set<ovs_be32> reqXids;
        int status;
        bool done;
    };
    /* Map of barrier request IDs to RequestState */
    typedef boost::unordered_map<ovs_be32, RequestState> RequestMap;
    RequestMap requests;

    boost::mutex reqMtx;
    boost::condition_variable reqCondVar;
};

}   // namespace enforcer
}   // namespace opflex


#endif // _OPFLEX_ENFORCER_FLOWEXECUTOR_H_
