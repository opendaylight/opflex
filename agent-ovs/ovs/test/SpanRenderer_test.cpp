/*
 * Test suite for class SpanRenderer
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <list>

#include <boost/test/unit_test.hpp>
#include <rapidjson/document.h>

#include "rpc/JsonRpc.h"
//#include "opflex/engine/internal/OvsdbConnection.h"
#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include <SpanRenderer.h>

namespace opflexagent {

using namespace std;
//using namespace opflex::engine::internal;
using namespace rapidjson;

BOOST_AUTO_TEST_SUITE(SpanRenderer_test)

class MockJsonRpc : public JsonRpc {
public:
    MockJsonRpc() : JsonRpc() {}
    virtual ~MockJsonRpc() {}

    virtual void start() {
        LOG(DEBUG) << "Starting MockJsonRpc ...";
        shared_ptr<MockRpcConnection> ptr = make_shared<MockRpcConnection>(*this);
        setRpcConnectionPtr(ptr);
        getRpcConnectionPtr()->start();
    }

    virtual void connect(const string& host, int port) {
        getRpcConnectionPtr()->connect(host, port);
    }

};

class MockSpanRenderer : public SpanRenderer {
public:
    MockSpanRenderer(Agent& agent) : SpanRenderer(agent) {}
    virtual ~MockSpanRenderer() {};

    bool connect() {
         // connect to OVSDB, destination is specified in agent config file.
         // If not the default is applied
         // If connection fails, a timer is started to retry and
         // back off at periodic intervals.
         if (timerStarted) {
             LOG(DEBUG) << "Canceling timer";
             connection_timer->cancel();
             timerStarted = false;
         }
         jRpc.reset(new MockJsonRpc());
         jRpc->start();
         jRpc->connect("localhost", 6640);
         return true;
    }
};

class SpanRendererFixture : public BaseFixture {
public:
    SpanRendererFixture() : BaseFixture() {
        spr = make_shared<MockSpanRenderer>(agent);
        initLogging("debug7", false, "");
        spr->connect();
    }

    virtual ~SpanRendererFixture() {};

    shared_ptr<MockSpanRenderer> spr;
};

static bool verifyPortUuid(shared_ptr<MockSpanRenderer> spr) {
    string uuid =  spr->jRpc->getPortUuid("p1-tap");
    LOG(DEBUG) << uuid;
    if (uuid.empty()) {
        return false;
    } else {
        return true;
    }
}

BOOST_FIXTURE_TEST_CASE( verify_getport, SpanRendererFixture ) {
    WAIT_FOR(verifyPortUuid(spr), 500);
}
BOOST_AUTO_TEST_SUITE_END()

}
