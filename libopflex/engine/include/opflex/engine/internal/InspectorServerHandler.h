/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file InspectorServerHandler.h
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
#ifndef OPFLEX_ENGINE_INSPECTORSERVERHANDLER_H
#define OPFLEX_ENGINE_INSPECTORSERVERHANDLER_H

#include <string>

#include <boost/noncopyable.hpp>
#include <rapidjson/document.h>

#include "opflex/engine/internal/OpflexHandler.h"

namespace opflex {
namespace engine {

class Inspector;

namespace internal {

class InspectorConn;

/**
 * Handler for implementing the server side of the inspector protocol
 */
class InspectorServerHandler : public OpflexHandler  {
public:
    /**
     * Construct a new handler associated with the given Inspector
     * connection
     *
     * @param conn_ the inspector connection
     */
    InspectorServerHandler(OpflexConnection* conn_, 
                           Inspector* inspector_) 
        : OpflexHandler(conn_), inspector(inspector_) {}

    /**
     * Destroy the handler
     */
    virtual ~InspectorServerHandler() {}

    // *************
    // OpflexHandler
    // *************

    virtual void connected();
    virtual void disconnected();
    virtual void ready();

    virtual void handlePolicyResolveReq(const rapidjson::Value& id,
                                        const rapidjson::Value& payload);


protected:
    Inspector* inspector;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_INSPECTORSERVERHANDLER_H */
