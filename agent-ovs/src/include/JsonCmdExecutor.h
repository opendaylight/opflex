/*
 * Definition of JsonCmdExecutor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_JSONCMDEXECUTOR_H_
#define OVSAGENT_JSONCMDEXECUTOR_H_

#include <string>
#include <vector>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include "ovs.h"
#include "SwitchConnection.h"

namespace ovsagent {

/**
 * Class to compute the hash-value of a JSON object.
 * Needed by unordered_map where key type is 'json *'.
 */
struct JsonPtrHash {
    /**
     * Return the hash-value of JSON object
     * @param obj Pointer to the JSON object
     * @return hash-value of the JSON object
     */
    size_t operator()(json * const obj);
};

/**
 * Class to check the equality of JSON objects.
 * Needed by unordered_map where key type is 'json *'.
 */
struct JsonPtrEquals {
    /**
     * Returns true if specified JSON objects are equal.
     * @param lhs the left side of the comparison
     * @param rhs the right side of the comparison
     * @return true if the objects are equal, false otherwise
     */
    bool operator()(json * const lhs, json * const rhs);
};

/**
 * @brief Class that allows you to execute control commands, encoded
 * as JSON, against Openvswitch daemon.
 */
class JsonCmdExecutor : public JsonMessageHandler,
                        public OnConnectListener {
public:
    /*
     * Default constructor
     */
    JsonCmdExecutor();
    virtual ~JsonCmdExecutor();

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
     * Send the specified control command and return the reply received.
     * @param command Control command to send
     * @param params Command parameters
     * @param result Result of the command or the error message
     * @return false if any error occurs while sending messages or
     * an error reply was received, true otherwise
     */
    virtual bool execute(const std::string& command,
                         const std::vector<std::string>& params,
                         std::string& result);

    /**
     * Send the specified control command.
     * @param command Control command to send
     * @param params Command parameters
     * @return false if any error occurs while sending messages, true otherwise
     */
    virtual bool executeNoBlock(const std::string& command,
                                const std::vector<std::string>& params);

    /* Interface: JsonMessageHandler */
    void Handle(SwitchConnection *c, jsonrpc_msg *msg);

    /* Interface: OnConnectListener */
    void Connected(SwitchConnection *swConn);

private:
    /**
     * Convert the specified command and command parametes to a JSON request.
     * @param command Command to convert
     * @param params Command parameters
     * @return The JSON-RPC message representing the command.
     */
    jsonrpc_msg* createRequest(const std::string& command,
                               const std::vector<std::string>& params);

    SwitchConnection *swConn;

    boost::mutex reqMtx;
    boost::condition_variable reqCondVar;

    /* Status of the reply */
    enum ReplyStatus { REPLY_NONE, REPLY_RESULT, REPLY_ERROR };
    /**
     * Reply received from openvswitch daemon in response to a request.
     */
    struct Reply {
        Reply() : status(REPLY_NONE) {}
        ReplyStatus status;
        std::string data;
    };
    /* Map of request-IDs of in-flight requests to their replies */
    typedef boost::unordered_map<json *, Reply,
                                 JsonPtrHash, JsonPtrEquals>    RequestMap;
    RequestMap jsonRequests;
};

}   // namespace ovsagent

#endif /* OVSAGENT_JSONCMDEXECUTOR_H_ */
