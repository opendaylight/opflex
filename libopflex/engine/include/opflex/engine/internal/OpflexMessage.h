/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpFlexMessage.h
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
    enum MessageType {
        REQUEST,
        RESPONSE,
        ERROR_RESPONSE
    };

    /**
     * Construct a new opflex message
     *
     * @param method the method for the message
     * @param type the type of message
     * @param id if specified, use as the ID of the message.  The
     * memory is owned by the caller
     */
    OpflexMessage(const std::string& method, MessageType type,
                  const rapidjson::Value* id = NULL);

    /**
     * Destroy the message
     */
    virtual ~OpflexMessage() {}

    /**
     * A rapidjson writer that should be used to serialize messages
     */
    typedef rapidjson::Writer<rapidjson::StringBuffer> MessageWriter;

    /**
     * Serialize the payload of the message.  The payload will be
     * included in the json-rpc message in the appropriate location
     * depending on the type of the message.  By default, the payload
     * will be an empty object of the appropriate type
     */
    virtual void serializePayload(MessageWriter& writer);

    /**
     * Serialize into a fully-formed opflex message
     *
     * @return a newly-allocated string buffer containing the message
     * data
     */
    rapidjson::StringBuffer* serialize();

protected:
    std::string method;
    MessageType type;

    const rapidjson::Value* id;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXMESSAGE_H */
