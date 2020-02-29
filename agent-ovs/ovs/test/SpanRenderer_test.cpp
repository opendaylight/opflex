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

#include "rpc/JsonRpc.h"
#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include <SpanRenderer.h>

namespace opflexagent {

using namespace std;
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

    virtual void connect() {
        getRpcConnectionPtr()->connect();
    }
};

class MockSpanRenderer : public SpanRenderer {
public:
    MockSpanRenderer(Agent& agent) : SpanRenderer(agent) {}
    virtual ~MockSpanRenderer() {};

    bool connect() {
         // connect to OVSDB
         // If connection fails, a timer is started to retry and
         // back off at periodic intervals.
         if (timerStarted) {
             LOG(DEBUG) << "Canceling timer";
             connection_timer->cancel();
             timerStarted = false;
         }
         jRpc.reset(new MockJsonRpc());
         jRpc->start();
         jRpc->connect();
         return true;
    }
};

class SpanRendererFixture : public BaseFixture {
public:
    SpanRendererFixture() : BaseFixture() {
        spr = make_shared<MockSpanRenderer>(agent);
        initLogging("debug", false, "");
        spr->connect();
    }

    virtual ~SpanRendererFixture() {};

    shared_ptr<MockSpanRenderer> spr;
};

static bool verifyCreateDestroy(const shared_ptr<MockSpanRenderer>& spr) {
    spr->jRpc->setNextId(999);
    JsonRpc::mirror mir;
    if (!spr->jRpc->getOvsdbMirrorConfig(mir)) {
        return false;
    }
    string erspanUuid = spr->jRpc->getPortUuid("erspan");
    if (erspanUuid.empty()) {
        return false;
    }
    JsonRpc::BrPortResult res;
    if (!spr->jRpc->getBridgePortList("br-int", res)) {
        return false;
    }
    tuple<string, set<string>> ports = make_tuple(res.brUuid, res.portUuids);
    if (!spr->jRpc->updateBridgePorts(ports, erspanUuid, false)) {
        return false;
    }

    if (!spr->jRpc->deleteMirror("br-int")) {
        return false;
    }
    shared_ptr<JsonRpc::erspan_ifc_v1> ep = make_shared<JsonRpc::erspan_ifc_v1>();
    ep->name = "erspan";
    ep->remote_ip = "10.20.120.240";
    ep->erspan_idx = 1;
    ep->erspan_ver = 1;
    ep->key = 1;
    if (!spr->jRpc->addErspanPort("br-int", ep)) {
        return false;
    }

    set<string> src_ports = {"p1-tap", "p2-tap"};
    set<string> dst_ports = {"p1-tap", "p2-tap"};
    set<string> out_ports = {"erspan"};
    mir.src_ports.insert(src_ports.begin(), src_ports.end());
    mir.dst_ports.insert(dst_ports.begin(), dst_ports.end());
    mir.out_ports.insert(out_ports.begin(), out_ports.end());
    spr->jRpc->addMirrorData("msandhu-sess1", mir);

    string brUuid = spr->jRpc->getBridgeUuid("br-int");
    if (brUuid.empty()) {
        return false;
    }
    return spr->jRpc->createMirror(brUuid, "msandhu-sess1");
}

BOOST_FIXTURE_TEST_CASE( verify_getport, SpanRendererFixture ) {
    WAIT_FOR(verifyCreateDestroy(spr), 500);
}
BOOST_AUTO_TEST_SUITE_END()

}
