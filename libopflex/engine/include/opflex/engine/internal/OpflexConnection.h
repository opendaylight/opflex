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
#include <cstdint>
#include <sstream>
#include <list>
#include <utility>

#include <boost/noncopyable.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <uv.h>

#include "yajr/yajr.hpp"
#include "yajr/rpc/message_factory.hpp"
#include "opflex/engine/internal/OpflexMessage.h"

#pragma once
#ifndef OPFLEX_ENGINE_OPFLEXCONNECTION_H
#define OPFLEX_ENGINE_OPFLEXCONNECTION_H

namespace opflex {
namespace engine {
namespace internal {

class OpflexPool;
class OpflexHandler;
class HandlerFactory;
class OpflexMessage;

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
     * Initialize SSL global context
     */
    static void initSSL();

    /**
     * Connect to the remote host.  Must be called from the libuv
     * processing thread.
     */
    virtual void connect();

    /**
     * Disconnect this connection from the remote peer.  Must be
     * called from the libuv processing thread.  Will retry if the
     * connection type supports it.
     */
    virtual void disconnect();

    /**
     * Disconnect this connection from the remote peer and close the
     * connection.  Must be called from the libuv processing thread.
     * Will not retry.
     */
    virtual void close();

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
     * calling the opflex handler.  This means the handshake has
     * succeeded.
     *
     * @return true if the connection is ready
     */
    virtual bool isReady();

    /**
     * Get a human-readable view of the name of the remote peer
     *
     * @return the string name
     */
    virtual const std::string& getRemotePeer() = 0;

    /**
     * Send the opflex message to the remote peer.  This can be called
     * from any thread.
     *
     * @param message the message to send.  Ownership of the object
     * passes to the connection.
     * @param sync if true, send the message synchronously.  This can
     * only be called if it's called from the uv loop thread.
     */
    virtual void sendMessage(OpflexMessage* message, bool sync = false);

    /**
     * Get the handler associated with this connection
     *
     * @return the OpflexHandler for the connection.
     */
    virtual OpflexHandler* getHandler() { return handler; }

    /**
     * Process the write queue for the connection from within the
     * libuv loop thread
     */
    void processWriteQueue();

protected:
    /**
     * The handler for the connection
     */
    OpflexHandler* handler;

    /**
     * New messages are ready to be written to the socket.
     * processWriteQueue() must be called.
     */
    virtual void messagesReady() = 0;

    /**
     * Get the peer for this connection
     */
    virtual yajr::Peer* getPeer() = 0;

protected:
    /**
     * Clean up write queue
     */
    virtual void cleanup();

private:
    uint64_t requestId;

    uint64_t connGeneration;
    typedef std::pair<OpflexMessage*, uint64_t> write_queue_item_t;
    typedef std::list<write_queue_item_t> write_queue_t;
    write_queue_t write_queue;
    uv_mutex_t queue_mutex;

    void doWrite(OpflexMessage* message);

    virtual void notifyReady();
    virtual void notifyFailed() {}

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
     * @param conn the connection associated with the handler
     * @return a newly-allocated handler that will be owned by the
     * connection
     */
    virtual OpflexHandler* newHandler(OpflexConnection* conn) = 0;
};


class PayloadWrapper {
public:
    PayloadWrapper(OpflexMessage* message_)
        : message(message_) { }

    bool operator()(yajr::rpc::SendHandler& handler) {
        message->serializePayload(handler);
        return true;
    }

    OpflexMessage* message;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
