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

#include <vector>
#include <utility>
#include <rapidjson/document.h>

#include "opflex/modb/ModelMetadata.h"
#include "opflex/gbp/Policy.h"

#pragma once
#ifndef OPFLEX_TEST_MOCKOPFLEXSERVER_H
#define OPFLEX_TEST_MOCKOPFLEXSERVER_H

namespace opflex {

namespace engine {
namespace internal {

class MockOpflexServerImpl;

} /* namespace internal */
} /* namespace engine */

namespace test {

/**
 * An opflex server we can use for mocking interactions with a real
 * Opflex server
 */
class MockOpflexServer {
public:
    /**
     * a pair of a role bitmask and connectivity string
     */
    typedef std::pair<uint8_t, std::string> peer_t;

    /**
     * A vector of peers
     */
    typedef std::vector<peer_t> peer_vec_t;

    /**
     * Construct a new mock opflex server
     *
     * @param port listen port for the server
     * @param roles the opflex roles for this server
     * @param peers a list of peers to return in the opflex handshake
     * @param proxies a list of proxies to return in the opflex handshake
     * @param md the model metadata for the server
     * @param prr_interval_secs how often to wakeup prr timer thread
     */
    MockOpflexServer(int port, uint8_t roles, peer_vec_t peers,
                     std::vector<std::string> proxies,
                     const modb::ModelMetadata& md,
                     int prr_interval_secs);

    /**
     * Destroy the opflex server
     */
    ~MockOpflexServer();

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
                   bool verifyPeers = true);

    /**
     * Get the peers that this server was configured with
     *
     * @return a vector of peer pairs
     */
    const peer_vec_t& getPeers() const;

    /**
     * Get the proxies that this server was configured with
     *
     * @return a vector of proxies
     */
    const std::vector<std::string>& getProxies() const;

    /**
     * Get the port number that this server was configured with
     *
     * @return the port number
     */
    int getPort() const;

    /**
     * Get the roles that this server was configured with
     * 
     * @return a bitmask containing the server roles
     */
    uint8_t getRoles() const;

private:
    engine::internal::MockOpflexServerImpl* pimpl;
};

} /* namespace test */
} /* namespace opflex */

#endif /* OPFLEX_TEST_MOCKOPFLEXSERVER_H */

