/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OpflexConnection.h
 * @brief Interface definition file for OpflexConnection
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <stdint.h>

#include <boost/noncopyable.hpp>

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXCONNECTION_H
#define OPFLEX_ENGINE_OPFLEXCONNECTION_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;
class OpflexHandler;
class HandlerFactory;

/**
 * Maintain the connection state information for a connection to an
 * opflex peer
 */
class OpflexConnection : private boost::noncopyable {
public:

    /**
     * Create a new opflex connection for the given hostname and port
     *
     * @param handlerFactory a factory that can allocate a handler for
     * the connection
     */
    OpflexConnection(HandlerFactory& handlerFactory);
    virtual ~OpflexConnection();

    /**
     * Connect to the remote host.  This is called by the constructor
     * but exists so it can be overridden in derived classes.
     */
    virtual void connect();

    /**
     * Disconnect this connection from the remote peer.  This is
     * called by the destructor.
     */
    virtual void disconnect();

    /**
     * Get the unique name for this component in the policy domain
     *
     * @returns the string name
     */
    virtual const std::string& getName() = 0;

    /**
     * Get the globally unique name for this policy domain
     *
     * @returns the string domain name
     */
    virtual const std::string& getDomain() = 0;

    /**
     * Check whether the connection is ready to accept requests by
     * calling the opflex handler
     *
     * @return true if the connection is ready
     */
    virtual bool isReady();

private:
    OpflexHandler* handler;

    friend class OpflexHandler;
};

/**
 * A factory that will manufacture new handlers for connections
 */
class HandlerFactory {
public:
    /**
     * Allocate a new OpflexHandler for the connection
     *
     * @param the connection associated with the handler
     * @return a newly-allocated handler that will be owned by the
     * connection
     */
    virtual OpflexHandler* newHandler(OpflexConnection* conn) = 0;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
