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


#include <boost/test/unit_test.hpp>

#include "opflex/ofcore/OFConstants.h"
#include "opflex/engine/internal/OpflexPool.h"

using namespace opflex::engine;
using namespace opflex::engine::internal;
using opflex::ofcore::OFConstants;

class EmptyHandlerFactory : public HandlerFactory {
public:
    virtual OpflexHandler* newHandler(OpflexConnection* conn) {
        return NULL;
    }
};

class MockClientConn : public OpflexClientConnection {
public:
    MockClientConn(HandlerFactory& handlerFactory,
                   OpflexPool* pool,
                   const std::string& hostname,
                   int port) :
        OpflexClientConnection(handlerFactory,
                               pool,
                               hostname,
                               port), ready(true),closed(false) {}

    virtual void connect() {}
    virtual void disconnect() {
        on_state_change(NULL, this, yajr::StateChange::DELETE, 0);
    }
    virtual bool isReady() { return ready; }
    virtual std::string& getRemotePeer() {
        static std::string dummy("DUMMY");
        return dummy;
    }
    virtual void close() {
        closed = true;
    }
    bool ready;
    bool closed;
};

class PoolFixture {
public:
    PoolFixture() : pool(handlerFactory, threadManager) {
        pool.start();
    }

    ~PoolFixture() {
        pool.stop();
    }

    opflex::util::ThreadManager threadManager;
    EmptyHandlerFactory handlerFactory;
    OpflexPool pool;
};

BOOST_AUTO_TEST_SUITE(OpflexPool_test)

BOOST_FIXTURE_TEST_CASE( manage_roles , PoolFixture ) {
    MockClientConn* c1 = new MockClientConn(handlerFactory, &pool,
                                            "1.2.3.4", 1234);
    MockClientConn* c2 = new MockClientConn(handlerFactory, &pool,
                                            "1.2.3.4", 1235);
    MockClientConn* c3 = new MockClientConn(handlerFactory, &pool,
                                            "1.2.3.4", 1236);

    pool.addPeer(c1);
    pool.addPeer(c2);
    pool.addPeer(c3);

    pool.setRoles(c1,
                  OFConstants::POLICY_REPOSITORY |
                  OFConstants::OBSERVER |
                  OFConstants::ENDPOINT_REGISTRY);

    BOOST_CHECK_EQUAL(c1, pool.getMasterForRole(OFConstants::POLICY_REPOSITORY));
    BOOST_CHECK_EQUAL(c1, pool.getMasterForRole(OFConstants::OBSERVER));
    BOOST_CHECK_EQUAL(c1, pool.getMasterForRole(OFConstants::ENDPOINT_REGISTRY));
    BOOST_CHECK_EQUAL(1, pool.getRoleCount(OFConstants::POLICY_REPOSITORY));
    BOOST_CHECK_EQUAL(1, pool.getRoleCount(OFConstants::OBSERVER));
    BOOST_CHECK_EQUAL(1, pool.getRoleCount(OFConstants::ENDPOINT_REGISTRY));

    pool.setRoles(c2,
                  OFConstants::POLICY_REPOSITORY |
                  OFConstants::ENDPOINT_REGISTRY);
    BOOST_CHECK_EQUAL(c1, pool.getMasterForRole(OFConstants::OBSERVER));
    pool.setRoles(c3, OFConstants::OBSERVER);

    BOOST_CHECK_EQUAL(2, pool.getRoleCount(OFConstants::POLICY_REPOSITORY));
    BOOST_CHECK_EQUAL(2, pool.getRoleCount(OFConstants::OBSERVER));
    BOOST_CHECK_EQUAL(2, pool.getRoleCount(OFConstants::ENDPOINT_REGISTRY));

    // fail to next master
    OpflexClientConnection* m1 =
        pool.getMasterForRole(OFConstants::POLICY_REPOSITORY);
    BOOST_CHECK_EQUAL(m1, pool.getMasterForRole(OFConstants::ENDPOINT_REGISTRY));
    ((MockClientConn*)m1)->ready = false;
    BOOST_CHECK(m1 != pool.getMasterForRole(OFConstants::ENDPOINT_REGISTRY));

    // check stickiness
    ((MockClientConn*)m1)->ready = true;
    BOOST_CHECK(m1 != pool.getMasterForRole(OFConstants::ENDPOINT_REGISTRY));

    c1->disconnect();
    BOOST_CHECK_EQUAL(1, pool.getRoleCount(OFConstants::POLICY_REPOSITORY));
    BOOST_CHECK_EQUAL(1, pool.getRoleCount(OFConstants::OBSERVER));
    BOOST_CHECK_EQUAL(1, pool.getRoleCount(OFConstants::ENDPOINT_REGISTRY));

    c2->disconnect();
    c3->disconnect();

    BOOST_CHECK_EQUAL(0, pool.getRoleCount(OFConstants::POLICY_REPOSITORY));
    BOOST_CHECK_EQUAL(0, pool.getRoleCount(OFConstants::OBSERVER));
    BOOST_CHECK_EQUAL(0, pool.getRoleCount(OFConstants::ENDPOINT_REGISTRY));
}

BOOST_FIXTURE_TEST_CASE( manage_ivxlan_roles , PoolFixture ) {
    MockClientConn* c1 = new MockClientConn(handlerFactory, &pool,
                                            "1.2.3.4", 1234);
    MockClientConn* c2 = new MockClientConn(handlerFactory, &pool,
                                            "1.2.3.4", 1235);
    MockClientConn* c3 = new MockClientConn(handlerFactory, &pool,
                                            "1.2.3.4", 1236);
    pool.setClientMode(opflex::ofcore::OFConstants::OpflexElementMode::TRANSPORT_MODE);
    pool.addPeer(c1);
    pool.addPeer(c2);
    pool.addPeer(c3);
    pool.setRoles(c1,
                  OFConstants::POLICY_REPOSITORY |
                  OFConstants::OBSERVER |
                  OFConstants::ENDPOINT_REGISTRY);
    OpflexPool::peer_name_set_t c1_peers, c2_peers;
    c1_peers.insert(std::make_pair<std::string, int>("1.2.3.4",1234));
    pool.validatePeerSet(c1,c1_peers);
    BOOST_CHECK_EQUAL(false, c1->closed);
    BOOST_CHECK_EQUAL(false, c2->closed);
    BOOST_CHECK_EQUAL(false, c3->closed);
    c2_peers.insert(std::make_pair<std::string, int>("1.2.3.4",1235));
    pool.validatePeerSet(c2,c2_peers);
    BOOST_CHECK_EQUAL(true, c1->closed);
    BOOST_CHECK_EQUAL(false, c2->closed);
    BOOST_CHECK_EQUAL(true, c3->closed);
    c1->disconnect();
    c2->disconnect();
    c3->disconnect();
}

BOOST_AUTO_TEST_SUITE_END()
