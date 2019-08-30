/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexMessage.h
 * @brief Interface definition file for OpFlex messages
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
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/document.h>

#include "yajr/rpc/message_factory.hpp"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXMESSAGE_H
#define OPFLEX_ENGINE_OPFLEXMESSAGE_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexConnection;

/**
 * Represent an Opflex message and allow serializing to the socket
 */
class OpflexMessage  {
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
     * Construct a new opflex message
     *
     * @param method the method for the message
     * @param type the type of message
     * @param id if specified, use as the ID of the message.  The
     * memory is owned by the caller, and must be set for responses.
     */
    OpflexMessage(const std::string& method, MessageType type,
                  const rapidjson::Value* id = NULL);

    /**
     * Destroy the message
     */
    virtual ~OpflexMessage() {}

    /**
     * Clone the opflex message
     */
    virtual OpflexMessage* clone() = 0;

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
     * A rapidjson writer that should be used to serialize messages
     */
    typedef rapidjson::Writer<rapidjson::StringBuffer> MessageWriter;

    /**
     * Serialize the payload of the message.  The payload will be
     * included in the json-rpc message in the appropriate location
     * depending on the type of the message.  By default, the payload
     * will be an empty object of the appropriate type
     *
     * @param writer the message writer to write the payload to
     */
    virtual void serializePayload(MessageWriter& writer) = 0;

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
 * A generic message that can be used to send messages with no payload
 */
class GenericOpflexMessage : public OpflexMessage {
public:
    GenericOpflexMessage(const std::string& method, MessageType type,
                         const rapidjson::Value* id = NULL)
        : OpflexMessage(method, type, id) {}

    /**
     * Destroy the message
     */
    virtual ~GenericOpflexMessage() {}

    /**
     * Clone the opflex message
     */
    virtual GenericOpflexMessage* clone() {
        return new GenericOpflexMessage(*this);
    }

    virtual void serializePayload(MessageWriter& writer);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXMESSAGE_H */
