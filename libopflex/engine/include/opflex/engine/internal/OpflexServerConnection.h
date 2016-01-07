/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexServerConnection.h
 * @brief Interface definition file for OpflexServerConnection
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <arpa/inet.h>

#include "opflex/engine/internal/OpflexConnection.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXSERVERCONNECTION_H
#define OPFLEX_ENGINE_OPFLEXSERVERCONNECTION_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;
class OpflexListener;

/**
 * Maintain connection state for an opflex server connection
 */
class OpflexServerConnection : public OpflexConnection {
public:
    /**
     * Create a new server connection associated with the given
     *
     * @param listener the listener associated with the connection
     */
    OpflexServerConnection(OpflexListener* listener);
    virtual ~OpflexServerConnection();

    /**
     * Disconnect this connection from the remote peer
     */
    virtual void disconnect();

    /**
     * Get the listener associated with this server connection.
     *
     * @return the opflex listener
     */
    OpflexListener* getListener() { return listener; }

    /**
     * Get the unique name for this component in the policy domain
     *
     * @returns the string name
     */
    virtual const std::string& getName();

    /**
     * Get the globally unique name for this policy domain
     *
     * @returns the string domain name
     */
    virtual const std::string& getDomain();

    virtual const std::string& getRemotePeer() { return remote_peer; }

    static void on_state_change(yajr::Peer* p, void* data,
                                yajr::StateChange::To stateChange,
                                int error);
    static uv_loop_t* loop_selector(void* data);
    virtual yajr::Peer* getPeer() { return peer; }
    virtual void messagesReady();

private:
    OpflexListener* listener;

    std::string remote_peer;
    void setRemotePeer(int rc, struct sockaddr_storage& name);

    yajr::Peer* peer;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
