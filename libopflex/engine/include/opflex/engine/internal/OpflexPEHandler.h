/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexPEHandler.h
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
#ifndef OPFLEX_ENGINE_OPFLEXPEHANDLER_H
#define OPFLEX_ENGINE_OPFLEXPEHANDLER_H

namespace opflex {
namespace engine {

class Processor;

namespace internal {

/**
 * An opflex handler that supports acting as a policy element with the
 * managed object database.
 */
class OpflexPEHandler : public OpflexHandler {
public:
    /**
     * Construct a new opflex PE handler associated with the given
     * connection
     */
    OpflexPEHandler(OpflexConnection* conn, Processor* processor_);

    /**
     * Destroy the handler
     */
    virtual ~OpflexPEHandler() {}

    /**
     * Get the Processor associated with this opflex PE handler
     *
     * @return the Processor pointer
     */
    Processor* getProcessor() { return processor; }

    // *************
    // OpflexHandler
    // *************

    virtual void connected();
    virtual void disconnected();
    virtual void ready();
    virtual void handleSendIdentityRes(uint64_t reqId,
                                       const rapidjson::Value& payload);
    virtual void handleSendIdentityErr(uint64_t reqId,
                                       const rapidjson::Value& payload);
    virtual void handlePolicyResolveRes(uint64_t reqId,
                                        const rapidjson::Value& payload);
    virtual void handlePolicyResolveErr(uint64_t reqId,
                                        const rapidjson::Value& payload);
    virtual void handlePolicyUnresolveRes(uint64_t reqId,
                                          const rapidjson::Value& payload);
    virtual void handlePolicyUnresolveErr(uint64_t reqId,
                                          const rapidjson::Value& payload);
    virtual void handlePolicyUpdateReq(const rapidjson::Value& id,
                                       const rapidjson::Value& payload);
    virtual void handleEPDeclareRes(uint64_t reqId,
                                    const rapidjson::Value& payload);
    virtual void handleEPDeclareErr(uint64_t reqId,
                                    const rapidjson::Value& payload);
    virtual void handleEPUndeclareRes(uint64_t reqId,
                                      const rapidjson::Value& payload);
    virtual void handleEPUndeclareErr(uint64_t reqId,
                                      const rapidjson::Value& payload);
    virtual void handleEPResolveRes(uint64_t reqId,
                                    const rapidjson::Value& payload);
    virtual void handleEPUnresolveRes(uint64_t reqId,
                                      const rapidjson::Value& payload);
    virtual void handleEPUpdateReq(const rapidjson::Value& id,
                                   const rapidjson::Value& payload);
    virtual void handleStateReportRes(uint64_t reqId,
                                      const rapidjson::Value& payload);
    virtual void handleStateReportErr(uint64_t reqId,
                                      const rapidjson::Value& payload);

private:
    Processor* processor;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXHANDLER_H */
