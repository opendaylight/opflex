/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file InspectorClientConn.h
 * @brief Interface definition file for InspectorClientConn
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexConnection.h"

#pragma once
#ifndef OPFLEX_ENGINE_INSPECTORCLIENTCONN_H
#define OPFLEX_ENGINE_INSPECTORCLIENTCONN_H

namespace opflex {
namespace engine {

class InspectorClientImpl;

namespace internal {

/**
 * A client connection for connecting to an inspector server
 */
class InspectorClientConn : public OpflexConnection {
public:
    /**
     * Create a new inspector client for the given socket
     *
     * @param handlerFactory a factory that can allocate a handler for
     * the connection
     * @param name A path name for the unix socket
     */
    InspectorClientConn(HandlerFactory& handlerFactory,
                        const std::string& name);
    virtual ~InspectorClientConn();

    // ****************
    // OpflexConnection
    // ****************

    virtual void connect();
    virtual void disconnect();
    virtual void close();
    virtual const std::string& getName();
    virtual const std::string& getDomain();
    virtual const std::string& getRemotePeer() { return name; }
    virtual yajr::Peer* getPeer() { return peer; }
    virtual void messagesReady();

private:
    const std::string& name;

    yajr::Peer* peer;

    uv_loop_t client_loop;
    uv_timer_t timer;
    static uv_loop_t* loop_selector(void* data);
    static void on_state_change(yajr::Peer* p, void* data,
                                yajr::StateChange::To stateChange,
                                int error);
    static void on_timer(uv_timer_t* timer);
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
