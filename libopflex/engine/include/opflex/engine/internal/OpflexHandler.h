/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpFlexHandler.h
 * @brief Interface definition file for OpFlex message handlers
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>

#include <boost/noncopyable.hpp>
#include <rapidjson/document.h>

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXHANDLER_H
#define OPFLEX_ENGINE_OPFLEXHANDLER_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexConnection;

/**
 * Abstract base class for implementing the Opflex protocol
 */
class OpflexHandler : private boost::noncopyable  {
public:
    /**
     * Construct a new handler associated with the given Opflex
     * connection
     *
     * @param conn_ the opflex connection
     */
    OpflexHandler(OpflexConnection* conn_)
        : conn(conn_), remoteRoles(0), state(DISCONNECTED) {}

    /**
     * Destroy the handler
     */
    virtual ~OpflexHandler() {}

    /**
     * Get the Opflex connection associated with this handler
     *
     * @return the OpflexConnection pointer
     */
    OpflexConnection* getConnection() { return conn; }

    /**
     * The state of the connection
     */
    enum ConnectionState {
        DISCONNECTED,
        CONNECTED,
        READY
    };

    /**
     * Get the connection state for this connection
     *
     * @return the current state for the connection
     */
    ConnectionState getState() const { return state; }

    /**
     * Check whether the connection is ready to accept requests.  This
     * means that the server handshake is complete and the connection
     * is active.
     *
     * @return true if the connection is ready
     */
    bool isReady() { return state == READY; }

    /**
     * Set the connection state for the connection
     */
    void setState(ConnectionState state_) { state = state_; }

    /**
     * The set of possible OpFlex roles
     */
    enum OpflexRole {
        /**
         * This is a client connection being used to boostrap the list
         * of peers
         */
        BOOTSTRAP = 0,
        /**
         * The policy element role
         */
        POLICY_ELEMENT = 1,
        /**
         * The policy repository role
         */
        POLICY_REPOSITORY = 2,
        /**
         * The endpoint registry role
         */
        ENDPOINT_REGISTRY = 4,
        /**
         * The observer role
         */
        OBSERVER = 8
    };

    /**
     * Get the bitmask representing the set of roles for the remote
     * end of this connection
     *
     * @return the role bitmask, consisting of bitfields set from
     * OpflexRole.
     */
    uint8_t getRemoteRoles() const { return remoteRoles; }

    /**
     * Check whether the current connection has the specified opflex
     * role.
     * 
     * @param role the OpFlex role to check for
     * @return true if the connection has the given role 
     */
    bool hasRemoteRole(OpflexRole role) const { return (remoteRoles & role); }

    // *************************
    // Connection state handlers
    // *************************

    /**
     * Called when the connection is connected.  Note that the same
     * connection may disconnect and reconnect multiple times.
     */
    virtual void connected() {}

    /**
     * Called when the connection is disconnected.  Note that the same
     * connection may disconnect and reconnect multiple times.
     */
    virtual void disconnected() {}

    /**
     * Called when the connection handshake is complete and the
     * connection is ready to handle requests.
     */
    virtual void ready() {}

    // *************************
    // Protocol Message Handlers
    // *************************

    /**
     * Handle an Opflex send identity request.
     *
     * @param payload the payload of the message
     */
    virtual void handleSendIdentityReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Send Identity Request");
    }

    /**
     * Handle an Opflex send identity response.
     *
     * @param payload the payload of the message
     */
    virtual void handleSendIdentityRes(const rapidjson::Value& payload) {
        handleUnexpected("Send Identity Response");
    }

    /**
     * Handle an Opflex send identity error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleSendIdentityErr(const rapidjson::Value& payload) {
        handleError(payload, "Send Identity");
    }

    /**
     * Handle an Opflex policy resolve request
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyResolveReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Policy Resolve Request");
    }

    /**
     * Handle an Opflex policy resolve response.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyResolveRes(const rapidjson::Value& payload) {
        handleUnexpected("Policy Resolve Response");
    }

    /**
     * Handle an Opflex policy resolve error response.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyResolveErr(const rapidjson::Value& payload) {
        handleError(payload, "Policy Resolve");
    }

    /**
     * Handle an Opflex policy unresolve request.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyUnresolveReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Policy Unresolve Request");
    }

    /**
     * Handle an Opflex policy unresolve response.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyUnresolveRes(const rapidjson::Value& payload) {
        handleUnexpected("Policy Unresolve Response");
    }

    /**
     * Handle an Opflex policy unresolve error response.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyUnresolveErr(const rapidjson::Value& payload) {
        handleError(payload, "Policy Unresolve");
    }

    /**
     * Handle an Opflex policy update  request.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyUpdateReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Policy Update Request");
    }

    /**
     * Handle an Opflex policy update response.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyUpdateRes(const rapidjson::Value& payload) {
        handleUnexpected("Policy Update Response");
    }

    /**
     * Handle an Opflex policy update error response.
     *
     * @param payload the payload of the message
     */
    virtual void handlePolicyUpdateErr(const rapidjson::Value& payload) {
        handleError(payload, "Policy Update");
    }

    /**
     * Handle an Opflex endpoint declare request.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPDeclareReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Endpoint Declare Request");
    }

    /**
     * Handle an Opflex endpoint declare response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPDeclareRes(const rapidjson::Value& payload) {
        handleUnexpected("Endpoint Declare Response");
    }

    /**
     * Handle an Opflex endpoint declare error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPDeclareErr(const rapidjson::Value& payload) {
        handleError(payload, "Endpoint Declare");
    }

    /**
     * Handle an Opflex endpoint undeclare request.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUndeclareReq(  const rapidjson::Value& payload) {
        handleUnsupportedReq("Endpoint Undeclare Request");
    }

    /**
     * Handle an Opflex endpoint undeclare response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUndeclareRes(  const rapidjson::Value& payload) {
        handleUnexpected("Endpoint Undeclare Response");
    }

    /**
     * Handle an Opflex endpoint undeclare error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUndeclareErr(  const rapidjson::Value& payload) {
        handleError(payload, "Endpoint Undeclare");
    }

    /**
     * Handle an Opflex endpoint resolve request.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPResolveReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Endpoint Resolve Request");
    }

    /**
     * Handle an Opflex endpoint resolve response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPResolveRes(const rapidjson::Value& payload) {
        handleUnexpected("Endpoint Resolve Response");
    }

    /**
     * Handle an Opflex endpoint resolve error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPResolveErr(const rapidjson::Value& payload) {
        handleError(payload, "Endpoint Resolve");
    }

    /**
     * Handle an Opflex endpoint unresolve request.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUnresolveReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Endpoint Unresolve Request");
    }

    /**
     * Handle an Opflex endpoint unresolve response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUnresolveRes(const rapidjson::Value& payload) {
        handleUnexpected("Endpoint Unresolve Response");
    }

    /**
     * Handle an Opflex endpoint unresolve error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUnresolveErr(const rapidjson::Value& payload) {
        handleError(payload, "Endpoint Unresolve");
    }

    /**
     * Handle an Opflex endpoint update request.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUpdateReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("Endpoint Update Request");
    }

    /**
     * Handle an Opflex endpoint update response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUpdateRes(const rapidjson::Value& payload) {
        handleUnexpected("Endpoint Update Response");
    }

    /**
     * Handle an Opflex endpoint update error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleEPUpdateErr(const rapidjson::Value& payload) {
        handleError(payload, "Endpoint Update");
    }

    /**
     * Handle an Opflex state report request.
     *
     * @param payload the payload of the message
     */
    virtual void handleStateReportReq(const rapidjson::Value& payload) {
        handleUnsupportedReq("State Report Request");
    }

    /**
     * Handle an Opflex state report response.
     *
     * @param payload the payload of the message
     */
    virtual void handleStateReportRes(const rapidjson::Value& payload) {
        handleUnexpected("State Report Response");
    }

    /**
     * Handle an Opflex state report error response.
     *
     * @param payload the payload of the message
     */
    virtual void handleStateReportErr(const rapidjson::Value& payload) {
        handleError(payload, "State Report");
    }

    /**
     * Generically handle an error by logging the message
     *
     * @param payload the error payload
     * @param type the type of message
     */
    virtual void handleError(const rapidjson::Value& payload,
                             const std::string& type);

    /**
     * Handle an unexpected message by logging an error, and then
     * disconnecting the connection.
     *
     * @param type the type of message
     */
    virtual void handleUnexpected(const std::string& type);

    /**
     * Handle an unsupported request by responding with an error with
     * a response code of EUNSUPPORTED
     * 
     * @param type the type of message
     */
    virtual void handleUnsupportedReq(const std::string& type);

protected:
    /**
     * The OpflexConnection associated with the handler
     */
    OpflexConnection* conn;

    /**
     * The local roles associated with this connection
     */
    uint8_t remoteRoles;

    /**
     * The current connection state
     */
    ConnectionState state;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXHANDLER_H */
