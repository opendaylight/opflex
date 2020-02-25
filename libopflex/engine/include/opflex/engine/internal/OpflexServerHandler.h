/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexServerHandler.h
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

#include <boost/atomic.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <rapidjson/document.h>

#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/MOSerializer.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXSERVERHANDLER_H
#define OPFLEX_ENGINE_OPFLEXSERVERHANDLER_H

namespace opflex {
namespace engine {
namespace internal {

class GbpOpflexServerImpl;

/**
 * An opflex handler for mocking a server implementation when writing
 * unit tests.
 */
class OpflexServerHandler : public OpflexHandler {
public:
    /**
     * Construct a new opflex PE handler associated with the given
     * connection
     */
    OpflexServerHandler(OpflexConnection* conn, GbpOpflexServerImpl* server_)
        : OpflexHandler(conn), server(server_), flakyMode(false) {}

    /**
     * Destroy the handler
     */
    virtual ~OpflexServerHandler() {}

    /**
     * Check whether the server has recieved a specific resolution
     */
    bool hasResolution(modb::class_id_t class_id, const modb::URI& uri);

    /**
     * Check whether there are any active resolutions
     */
    bool hasResolutions() {
        boost::lock_guard<boost::mutex> guard(resolutionMutex);
        return !resolutions.empty();
    }

    /**
     * Enable or disable flaky mode.  When enabled, drop the first
     * attempt to resolve anything.
     *
     * @param flakyMode true to enable flaky mode
     */
    void setFlaky(bool flakyMode) { this->flakyMode = flakyMode; }

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
    virtual void handleEPUpdateRes(uint64_t reqId,
                                   const rapidjson::Value& payload);
    virtual void handleStateReportReq(const rapidjson::Value& id,
                                      const rapidjson::Value& payload);

protected:
    GbpOpflexServerImpl* server;
    boost::mutex resolutionMutex;
    OF_UNORDERED_SET<modb::reference_t> resolutions;
    OF_UNORDERED_SET<modb::reference_t> declarations;
    boost::atomic<bool> flakyMode;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXHANDLER_H */
