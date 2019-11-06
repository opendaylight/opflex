/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for Processor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <utility>
#include <iostream>

#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>

#include "opflex/engine/internal/MockOpflexServerImpl.h"

#include "opflex/c/offramework_c.h"
#include "opflex/c/ofpeerstatuslistener_c.h"
#include "opflex/c/ofloghandler_c.h"

#include "MDFixture.h"
#include "TestListener.h"

using opflex::modb::MDFixture;
using opflex::engine::internal::MockOpflexServerImpl;
using opflex::ofcore::OFConstants;
using std::make_pair;
using boost::assign::list_of;
using std::string;
using std::vector;

BOOST_AUTO_TEST_SUITE(cwrapper_test)

#define SERVER_ROLES \
        (OFConstants::POLICY_REPOSITORY |     \
         OFConstants::ENDPOINT_REGISTRY |     \
         OFConstants::OBSERVER)
#define LOCALHOST "127.0.0.1"

class ServerFixture : public MDFixture {
public:
    ServerFixture()
        : MDFixture(),
          mockServer(8009, SERVER_ROLES,
                     list_of(make_pair(SERVER_ROLES, LOCALHOST":8009")),
                     vector<string>(),
                     md, 60),
          peerStatus(-1), poolHealth(1) {
        mockServer.start();
        WAIT_FOR(mockServer.getListener().isListening(), 1000);
    }

    ~ServerFixture() {
        mockServer.stop();
    }

    MockOpflexServerImpl mockServer;
    int peerStatus;
    int poolHealth;
};

void handler(const char* file, int line, 
             const char* function, int level, 
             const char* message) {
    std::cout << "<" << level << "> " 
              << file << ":" << line << " " 
              << message << std::endl;
}

void peerstatus_peer(void* user_data, 
                     const char *peerhostname, 
                     int port, 
                     int status) {
    LOG(INFO) << peerhostname << ":" << port << status;
    ((ServerFixture*)user_data)->peerStatus = status;
}

void peerstatus_health(void* user_data, 
                       int health) {
    LOG(INFO) << health;
    ((ServerFixture*)user_data)->poolHealth = health;
}

BOOST_FIXTURE_TEST_CASE( init, ServerFixture ) {
    ofloghandler_register(LOG_DEBUG1, handler);

    offramework_p framework = NULL;
    ofpeerstatuslistener_p peer_listener = NULL;
    BOOST_CHECK(OF_IS_SUCCESS(ofpeerstatuslistener_create((void*)this, 
                                                          peerstatus_peer,
                                                          peerstatus_health,
                                                          &peer_listener)));

    BOOST_CHECK(OF_IS_SUCCESS(offramework_create(&framework)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_register_peerstatuslistener(framework,
                                                                      peer_listener)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_set_model(framework, &md)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_set_opflex_identity(framework, "dummy", "test")));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_start(framework)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_add_peer(framework, LOCALHOST, 8009)));
    WAIT_FOR(peerStatus == OF_PEERSTATUS_READY, 1000)
    WAIT_FOR(poolHealth == OF_POOLHEALTH_HEALTHY, 1000)

    BOOST_CHECK(OF_IS_SUCCESS(offramework_stop(framework)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_destroy(&framework)));
    BOOST_CHECK(OF_IS_SUCCESS(ofpeerstatuslistener_destroy(&peer_listener)));
}

BOOST_AUTO_TEST_SUITE_END()
