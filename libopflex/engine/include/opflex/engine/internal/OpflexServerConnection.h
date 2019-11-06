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
#include <uv.h>
#include <mutex>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/modb/URI.h"
#include "opflex/modb/PropertyInfo.h"
#include "opflex/modb/mo-internal/ObjectInstance.h"
#include "opflex/gbp/Policy.h"

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

    /**
     * Add URI to resolve uri map
     *
     * @param uri uri to be added to map
     * @lifetime lifetime of the uri
     */
    void addUri(const opflex::modb::URI& uri, int64_t lifetime);

    /**
     * Check if URI exists in resolve uri map
     *
     * @returns true if uri exists
     */
    bool getUri(const opflex::modb::URI& uri);

    /**
     * Clear URI from resolve uri map
     *
     * @param uri uri to be removed
     * @returns true if remove successful
     */
    bool clearUri(const opflex::modb::URI& uri);

    /**
     * Add reference to pending update
     *
     * @param class_id_t class_id for modb::reference_t
     * @param uri uri for modb::reference_t
     * @param op the update operation type
     */
    void addPendingUpdate(opflex::modb::class_id_t class_id,
                          const opflex::modb::URI& uri,
                          opflex::gbp::PolicyUpdateOp op);

    /**
     * Send pending updates to each agent
     */
    void sendUpdates();

    /**
     * Send timeout triggers to each agent
     */
    void sendTimeouts();

private:
    OpflexListener* listener;

    std::string remote_peer;
    void setRemotePeer(int rc, struct sockaddr_storage& name);
    /**
     * uri_map is a map of URIs agent is interested in
     */
    OF_UNORDERED_MAP<opflex::modb::URI, int64_t> uri_map;
    std::mutex uri_map_mutex;

    /**
     * replace and del vectors are used to construct policy update
     */
    std::vector<opflex::modb::reference_t> replace;
    std::vector<opflex::modb::reference_t> merge;
    std::vector<opflex::modb::reference_t> deleted;
    std::mutex uri_update_mutex;

    /**
     * Start a thread for sending policy updates to agent
     */
    uv_loop_t server_loop;
    uv_thread_t server_thread;
    uv_async_t policy_update_async;
    uv_async_t cleanup_async;
    uv_async_t prr_timer_async;
    static void server_thread_entry(void *data);
    static void on_policy_update_async(uv_async_t *handle);
    static void on_cleanup_async(uv_async_t *handle);
    static void on_prr_timer_async(uv_async_t* handle);

    yajr::Peer* peer;
};


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_OPFLEXCONNECTION_H */
