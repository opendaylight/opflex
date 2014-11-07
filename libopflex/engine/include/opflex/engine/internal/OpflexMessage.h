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
     */
    OpflexMessage(const std::string& method, MessageType type);

    /**
     * Destroy the message
     */
    virtual ~OpflexMessage() {}

    typedef rapidjson::Writer<rapidjson::StringBuffer> StrWriter;

    /**
     * Serialize the payload of the message.  The payload will be
     * included in the json-rpc message in the appropriate location
     * depending on the type of the message
     */
    virtual void serializePayload(StrWriter& writer) = 0;

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
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXMESSAGE_H */
