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
     * @param handlerFactory a factory that can allocate a handler for
     * the connection
     * @param listener the listener associated with the connection
     * @param hostname the hostname or IP address for the remote peer
     * @param port the TCP port number of the remote peer
     */
    OpflexServerConnection(HandlerFactory& handlerFactory,
                           OpflexListener* listener,
                           const std::string& hostname, 
                           int port);
    virtual ~OpflexServerConnection();

    /**
     * Disconnect this connection from the remote peer
     */
    virtual void disconnect();

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

    /**
     * Get the hostname for this connection
     */
    const std::string& getHostname() const { return hostname; }

    /**
     * Get the port for this connection
     */
    int getPort() const { return port; }

private:
    OpflexListener* listener;

    std::string hostname;
    int port;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
