/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file InspectorClientHandler.h
 * @brief Interface definition file for Inspector message handlers
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_ENGINE_INSPECTORCLIENTHANDLER_H
#define OPFLEX_ENGINE_INSPECTORCLIENTHANDLER_H

#include <string>

#include <boost/noncopyable.hpp>
#include <rapidjson/document.h>

#include "opflex/engine/internal/OpflexHandler.h"

namespace opflex {
namespace engine {

class InspectorClientImpl;

namespace internal {

/**
 * Handler for implementing the client side of the inspector protocol
 */
class InspectorClientHandler : public OpflexHandler  {
public:
    /**
     * Construct a new handler associated with the given Inspector
     * connection
     *
     * @param conn the connection
     * @param client The inspector client implementation
     */
    InspectorClientHandler(OpflexConnection* conn, 
                           InspectorClientImpl* client);

    /**
     * Destroy the handler
     */
    virtual ~InspectorClientHandler() {}

    // *************
    // OpflexHandler
    // *************

    virtual void connected();
    virtual void disconnected();
    virtual void ready();

    virtual void handleCustomRes(const rapidjson::Value& payload);
    virtual void handleError(const rapidjson::Value& payload,
                             const std::string& type);

private:
    InspectorClientImpl* client;

    void checkDone();

    virtual void handlePolicyQueryRes(const rapidjson::Value& payload);
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_INSPECTORCLIENTHANDLER_H */
