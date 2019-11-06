/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MockOpflexServer.h
 * @brief Interface definition file for MockOpflexServer
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/test/MockOpflexServer.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/gbp/Policy.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/MockServerHandler.h"

#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>

#pragma once
#ifndef OPFLEX_ENGINE_MOCKOPFLEXSERVERIMPL_H
#define OPFLEX_ENGINE_MOCKOPFLEXSERVERIMPL_H

namespace opflex {
namespace engine {
namespace internal {

/**
 * An opflex server we can use for mocking interactions with a real
 * Opflex server
 */
class MockOpflexServerImpl : public HandlerFactory,
                             public MOSerializer::Listener {
public:
    /**
     * Construct a new mock opflex server
     *
     * @param port listen port for the server
     * @param roles the opflex roles for this server
     * @param peers a list of peers to return in the opflex handshake
     * @param md the model metadata for the server
     * @param prr_interval_secs how often to wakeup prr timer thread
     */
    MockOpflexServerImpl(int port, uint8_t roles,
                         test::MockOpflexServer::peer_vec_t peers,
                         std::vector<std::string> proxies,
                         const modb::ModelMetadata& md,
                         int prr_interval_secs);
    virtual ~MockOpflexServerImpl();

    /**
     * Enable SSL for connections to opflex peers.  Call before start()
     *
     * @param caStorePath the filesystem path to a directory
     * containing CA certificates, or to a file containing a specific
     * CA certificate.
     * @param serverKeyPath the path to the server private key
     * @param serverKeyPass the passphrase for the server private key
     * @param verifyPeers set to true to verify that peer certificates
     * properly chain to a trusted root
     */
    void enableSSL(const std::string& caStorePath,
                   const std::string& serverKeyPath,
                   const std::string& serverKeyPass,
                   bool verifyPeers);

    /**
     * Start the server
     */
    void start();

    /**
     * Stop the server
     */
    void stop();

    /**
     * Read policy into the server from the specified file.  Note that
     * this will not automatically cause updates to be sent to
     * connected clients.
     *
     * @param file the filename to read in
     */
    void readPolicy(const std::string& file);

    /**
     * Update policy from RapidJson document
     *
     * @param d the RapidJson document to be read in
     * @param op the Update opcode
     */
    void updatePolicy(rapidjson::Document& d, gbp::PolicyUpdateOp op);

    /**
     * Get the peers that this server was configured with
     *
     * @return a vector of peer pairs
     */
    const test::MockOpflexServer::peer_vec_t& getPeers() { return peers; }

    /**
     * Get the proxies that this server was configured with
     *
     * @return a vector of proxies
     */
    const std::vector<std::string>& getProxies() { return proxies; }

    /**
     * Get the port number that this server was configured with
     *
     * @return the port number
     */
    int getPort() { return port; }

    /**
     * Get the roles that this server was configured with
     *
     * @param a bitmask containing the server roles
     */
    uint8_t getRoles() { return roles; }

    // See HandlerFactory::newHandler
    virtual OpflexHandler* newHandler(OpflexConnection* conn);

    /**
     * Get the object store for this server
     */
    modb::ObjectStore& getStore() { return db; }
    /**
     * Get a system store client for this server
     */
    modb::mointernal::StoreClient* getSystemClient() { return client; }
    /**
     * Get the MOSerializer for the server
     */
    MOSerializer& getSerializer() { return serializer; }

    /**
     * Get the opflex listener
     */
    OpflexListener& getListener() { return listener; }

    /**
     * Dispatch a policy update to the attached clients
     */
    void policyUpdate(const std::vector<modb::reference_t>& replace,
                      const std::vector<modb::reference_t>& merge_children,
                      const std::vector<modb::reference_t>& del);
    /**
     * Dispatch a policy update to a single client
     */
    void policyUpdate(OpflexServerConnection* conn,
                      const std::vector<modb::reference_t>& replace,
                      const std::vector<modb::reference_t>& merge_children,
                      const std::vector<modb::reference_t>& del);


    /**
     * Dispatch an endpoint update to the attached clients
     */
    void endpointUpdate(const std::vector<modb::reference_t>& replace,
                        const std::vector<modb::reference_t>& del);

    // **********************
    // MOSerializer::Listener
    // **********************

    virtual void remoteObjectUpdated(modb::class_id_t class_id,
                                     const modb::URI& uri,
                                     gbp::PolicyUpdateOp op);

    /**
     * on timer callback
     */
    void on_timer(const boost::system::error_code& ec);

    /**
     * Get prr timer callback interval
     */
    int getPrrIntervalSecs() { return prr_interval_secs; }
private:
    int port;
    uint8_t roles;

    test::MockOpflexServer::peer_vec_t peers;

    std::vector<std::string> proxies;

    OpflexListener listener;

    util::ThreadManager threadManager;
    modb::ObjectStore db;
    MOSerializer serializer;
    modb::mointernal::StoreClient* client;

    std::unique_ptr<std::thread> io_service_thread;
    boost::asio::io_service io;
    std::unique_ptr<boost::asio::deadline_timer> prr_timer;
    std::atomic_bool stopping;
    int prr_interval_secs;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_MOCKOPFLEXSERVERIMPL_H */
