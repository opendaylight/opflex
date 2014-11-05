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

#include <boost/test/unit_test.hpp>

#include "opflex/engine/internal/OpflexPool.h"

using namespace opflex::engine;
using namespace opflex::engine::internal;

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
                               port), ready(true) { }

    virtual void connect() {}
    virtual void disconnect() {}
    virtual bool isReady() { return ready; }

    bool ready;
};

class PoolFixture {
public:
    PoolFixture() : pool(handlerFactory) { }

    EmptyHandlerFactory handlerFactory;
    OpflexPool pool;
};

BOOST_AUTO_TEST_SUITE(OpflexPool_test)

BOOST_FIXTURE_TEST_CASE( manage_roles , PoolFixture ) {

    MockClientConn c1(handlerFactory, &pool, "test1", 1234);
    MockClientConn c2(handlerFactory, &pool, "test2", 1234);
    MockClientConn c3(handlerFactory, &pool, "test2", 1235);

    pool.addPeer(&c1);
    pool.addPeer(&c2);
    pool.addPeer(&c3);

    pool.setRoles(&c1, 
                  OpflexHandler::POLICY_REPOSITORY |
                  OpflexHandler::OBSERVER |
                  OpflexHandler::ENDPOINT_REGISTRY);

    BOOST_CHECK_EQUAL(&c1, pool.getMasterForRole(OpflexHandler::POLICY_REPOSITORY));
    BOOST_CHECK_EQUAL(&c1, pool.getMasterForRole(OpflexHandler::OBSERVER));
    BOOST_CHECK_EQUAL(&c1, pool.getMasterForRole(OpflexHandler::ENDPOINT_REGISTRY));

    pool.setRoles(&c2, 
                  OpflexHandler::POLICY_REPOSITORY |
                  OpflexHandler::ENDPOINT_REGISTRY);
    BOOST_CHECK_EQUAL(&c1, pool.getMasterForRole(OpflexHandler::OBSERVER));
    pool.setRoles(&c3, OpflexHandler::OBSERVER);

    // fail to next master
    OpflexClientConnection* m1 = 
        pool.getMasterForRole(OpflexHandler::POLICY_REPOSITORY);
    BOOST_CHECK_EQUAL(m1, pool.getMasterForRole(OpflexHandler::ENDPOINT_REGISTRY));
    ((MockClientConn*)m1)->ready = false;
    BOOST_CHECK(m1 != pool.getMasterForRole(OpflexHandler::ENDPOINT_REGISTRY));

    // check stickiness
    ((MockClientConn*)m1)->ready = true;
    BOOST_CHECK(m1 != pool.getMasterForRole(OpflexHandler::ENDPOINT_REGISTRY));
}

BOOST_AUTO_TEST_SUITE_END()
