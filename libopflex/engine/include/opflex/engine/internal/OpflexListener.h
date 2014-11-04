/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexListener.h
 * @brief Interface definition file for OpflexListener
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexServerConnection.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXLISTENER_H
#define OPFLEX_ENGINE_OPFLEXLISTENER_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;

/**
 * Represents a listen socket that will spawn OpflexServerConnections
 */
class OpflexListener {
public:
    /**
     * Create a new opflex listener for the given IP address and port
     *
     * @param handlerFactory a factory that can allocate a handler for
     * the spawned OpflexServerConnection objects
     * @param pool the connection pool that hosts this connection
     * @param hostname the hostname or IP address to bind to
     * @param port the TCP port number to bind to
     */
    OpflexListener(HandlerFactory& handlerFactory,
                   const std::string& hostname, 
                   int port,
                   const std::string& name,
                   const std::string& domain);
    ~OpflexListener();

    /**
     * Stop listening on the local socket for new connections
     */
    void disconnect();

    /**
     * Get the bind hostname for this connection
     */
    const std::string& getHostname() const { return hostname; }

    /**
     * Get the bind port for this connection
     */
    int getPort() const { return port; }

    /**
     * Get the unique name for this component in the policy domain
     *
     * @returns the string name
     */
    const std::string& getName() { return name; }

    /**
     * Get the globally unique name for this policy domain
     *
     * @returns the string domain name
     */
    const std::string& getDomain() { return domain; }

private:
    HandlerFactory& handlerFactory;

    std::string hostname;
    int port;

    std::string name;
    std::string domain;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
