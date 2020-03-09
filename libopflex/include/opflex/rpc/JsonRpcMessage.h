/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file JsonRpcMessage.h
 * @brief Interface definition for various JSON/RPC messages used by the
 * engine
 */
/*
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef RPC_JSONRPCMESSAGE_H
#define RPC_JSONRPCMESSAGE_H

#include<set>
#include<map>
#include<list>

#include <condition_variable>
#include <mutex>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <opflex/yajr/rpc/message_factory.hpp>

namespace opflex {
namespace jsonrpc {

/**
 * Represent an Opflex message and allow serializing to the socket
 */
class JsonRpcMessage  {
public:
    /**
     * The type of the message
     */
    enum MessageType {
        /** A request */
        REQUEST,
        /** a response message */
        RESPONSE,
        /** an error response */
        ERROR_RESPONSE
    };

    /**
     * Construct a new JSON-RPC message
     *
     * @param method the method for the message
     * @param type the type of message
     * @param id if specified, use as the ID of the message.  The
     * memory is owned by the caller, and must be set for responses.
     */
    JsonRpcMessage(const std::string& method, MessageType type,
                   const rapidjson::Value* id = NULL);

    /**
     * Destroy the message
     */
    virtual ~JsonRpcMessage() {}

    /**
     * Clone the JSON-RPC message
     */
    virtual JsonRpcMessage* clone() = 0;

    /**
     * Get the request method for this message
     * @return the method for the message
     */
    const std::string& getMethod() const { return method; }

    /**
     * Get the message type for this message
     *
     * @return the type for the message
     */
    MessageType getType() const { return type; }

    /**
     * Get a transaction ID for a request.  If nonzero, allocate a
     * transaction ID using a counter
     *
     * @return the transaction ID for the request
     */
    virtual uint64_t getReqXid() { return 0; }

    /**
     * Get the ID for this message.  Must only be called on a response
     * or error.
     * @return the ID for the message
     */
    const rapidjson::Value& getId() const { return *id; }

    /**
     * Serialize payload
     * @param writer writer
     */
    virtual void serializePayload(yajr::rpc::SendHandler& writer) = 0;

    /**
     * Operator to serialize a generic empty payload to any writer
     * @param writer the writer to serialize to
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        switch (type) {
        case REQUEST:
            writer.StartArray();
            writer.EndArray();
            break;
        default:
            writer.StartObject();
            writer.EndObject();
        }
        return true;
    }

protected:
    /**
     * The request method associated with the message
     */
    std::string method;
    /**
     * The message type of the message
     */
    MessageType type;

    /**
     * The ID associated with the message; may be NULL for a request
     * message, but a response must always set it
     */
    const rapidjson::Value* id;
};

/**
 * Payload wrapper
 */
class PayloadWrapper {
public:
    /**
     * Construct a payload wrapper
     * @param message_ RPC message
     */
    PayloadWrapper(JsonRpcMessage* message_)
        : message(message_) { }

    /**
     * Operator to serialize a generic a payload
     * @param handler handler
     */
    bool operator()(yajr::rpc::SendHandler& handler) {
        message->serializePayload(handler);
        return true;
    }

private:
    JsonRpcMessage* message;
};

} /* namespace rpc */
} /* namespace opflex */

#endif