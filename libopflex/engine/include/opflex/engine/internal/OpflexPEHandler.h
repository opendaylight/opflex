/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpFlexPEHandler.h
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
    OpflexPEHandler(OpflexConnection* conn, Processor* processor_) 
        : OpflexHandler(conn), processor(processor_) {}

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
    virtual void handleSendIdentityRes(const rapidjson::Value& payload);
    virtual void handlePolicyResolveRes(const rapidjson::Value& payload);
    virtual void handlePolicyUnresolveRes(const rapidjson::Value& payload);
    virtual void handleEPDeclareRes(const rapidjson::Value& payload);
    virtual void handleEPUndeclareRes(const rapidjson::Value& payload);
    virtual void handleEPResolveRes(const rapidjson::Value& payload);
    virtual void handleEPUnresolveRes(const rapidjson::Value& payload);
    virtual void handleEPUpdateReq(const rapidjson::Value& payload);

protected:
    Processor* processor;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXHANDLER_H */
