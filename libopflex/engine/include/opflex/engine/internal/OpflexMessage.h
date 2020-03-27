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

#include "opflex/yajr/rpc/message_factory.hpp"
#include "opflex/rpc/JsonRpcMessage.h"

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
class OpflexMessage : public jsonrpc::JsonRpcMessage {
public:

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

    virtual void serializePayload(yajr::rpc::SendHandler& writer) = 0;

    /**
     * Get a transaction ID for a request.  If nonzero, allocate a
     * transaction ID using a counter
     *
     * @return the transaction ID for the request
     */
    virtual uint64_t getReqXid() { return 0; }
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

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXMESSAGE_H */
