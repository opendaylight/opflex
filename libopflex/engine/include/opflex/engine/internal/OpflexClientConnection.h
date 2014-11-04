/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexClientConnection.h
 * @brief Interface definition file for OpflexClientConnection
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
#ifndef OPFLEX_ENGINE_OPFLEXCLIENTCONNECTION_H
#define OPFLEX_ENGINE_OPFLEXCLIENTCONNECTION_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;

/**
 * Maintain the clientConnection state information for a clientConnection to an
 * opflex peer
 */
class OpflexClientConnection : public OpflexConnection {
public:
    /**
     * Create a new opflex clientConnection for the given hostname and port
     *
     * @param handlerFactory a factory that can allocate a handler for
     * the connection
     * @param pool the connection pool that hosts this connection
     * @param hostname the hostname or IP address for the remote peer
     * @param port the TCP port number of the remote peer
     */
    OpflexClientConnection(HandlerFactory& handlerFactory,
                           OpflexPool* pool,
                           const std::string& hostname, 
                           int port);
    virtual ~OpflexClientConnection();

    /**
     * Disconnect this connection from the remote peer
     */
    virtual void disconnect();

    /**
     * Get the connection pool that hosts this connection
     *
     * @return a pointer to the OpflexPool
     */
    OpflexPool* getPool() { return pool; }

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
    OpflexPool* pool;

    std::string hostname;
    int port;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
