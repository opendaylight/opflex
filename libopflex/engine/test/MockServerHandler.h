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

#include <boost/unordered_set.hpp>
#include <rapidjson/document.h>

#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/MOSerializer.h"

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

    /**
     * Check whether the server has recieved a specific resolution
     */
    bool hasResolution(modb::class_id_t class_id, const modb::URI& uri);

    /**
     * Check whether there are any active resolutions
     */
    bool hasResolutions() { return resolutions.size() > 0; }

    // *************
    // OpflexHandler
    // *************

    virtual void connected();
    virtual void disconnected();
    virtual void ready();
    virtual void handleSendIdentityReq(const rapidjson::Value& id,
                                       const rapidjson::Value& payload);
    virtual void handlePolicyResolveReq(const rapidjson::Value& id,
                                        const rapidjson::Value& payload);
    virtual void handlePolicyUnresolveReq(const rapidjson::Value& id,
                                          const rapidjson::Value& payload);
    virtual void handleEPDeclareReq(const rapidjson::Value& id,
                                    const rapidjson::Value& payload);
    virtual void handleEPUndeclareReq(const rapidjson::Value& id,
                                      const rapidjson::Value& payload);
    virtual void handleEPResolveReq(const rapidjson::Value& id,
                                    const rapidjson::Value& payload);
    virtual void handleEPUnresolveReq(const rapidjson::Value& id,
                                      const rapidjson::Value& payload);
    virtual void handleEPUpdateRes(const rapidjson::Value& id,
                                   const rapidjson::Value& payload);
    virtual void handleStateReportReq(const rapidjson::Value& id,
                                      const rapidjson::Value& payload);

protected:
    MockOpflexServer* server;
    boost::unordered_set<modb::reference_t> resolutions;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXHANDLER_H */
