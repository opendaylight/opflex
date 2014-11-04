/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MockServerHandler.h
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

#include <rapidjson/document.h>

#include "opflex/engine/internal/OpflexHandler.h"

#pragma once
#ifndef OPFLEX_ENGINE_MOCKSERVERHANDLER_H
#define OPFLEX_ENGINE_MOCKSERVERHANDLER_H

namespace opflex {
namespace engine {
namespace internal {

class MockOpflexServer;

/**
 * An opflex handler for mocking a server implementation when writing
 * unit tests.
 */
class MockServerHandler : public OpflexHandler {
public:
    /**
     * Construct a new opflex PE handler associated with the given
     * connection
     */
    MockServerHandler(OpflexConnection* conn, MockOpflexServer* server_) 
        : OpflexHandler(conn), server(server_) {}

    /**
     * Destroy the handler
     */
    virtual ~MockServerHandler() {}

    // *************
    // OpflexHandler
    // *************

    virtual void connected();
    virtual void disconnected();
    virtual void ready();
    virtual void handleSendIdentityReq(const rapidjson::Value& payload);
    virtual void handlePolicyResolveReq(const rapidjson::Value& payload);
    virtual void handlePolicyUnresolveReq(const rapidjson::Value& payload);
    virtual void handleEPDeclareReq(const rapidjson::Value& payload);
    virtual void handleEPUndeclareReq(const rapidjson::Value& payload);
    virtual void handleEPResolveReq(const rapidjson::Value& payload);
    virtual void handleEPUnresolveReq(const rapidjson::Value& payload);
    virtual void handleEPUpdateRes(const rapidjson::Value& payload);

protected:
    MockOpflexServer* server;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXHANDLER_H */
