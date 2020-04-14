/*
 * Test suite for class SpanRenderer
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include <SpanRenderer.h>
#include "MockJsonRpc.h"

namespace opflexagent {

using namespace std;
using namespace rapidjson;

BOOST_AUTO_TEST_SUITE(SpanRenderer_test)

class SpanRendererFixture : public BaseFixture {
public:
    SpanRendererFixture() : BaseFixture() {
        spr = make_shared<SpanRenderer>(agent);
        initLogging("debug", false, "");
        conn.reset(new MockRpcConnection());
        spr->start("br-int", conn.get());
        spr->connect();
    }

    virtual ~SpanRendererFixture() {
        spr->stop();
    };

    shared_ptr<SpanRenderer> spr;
    unique_ptr<OvsdbConnection> conn;
};

static bool verifyCreateDestroy(const shared_ptr<SpanRenderer>& spr) {
    spr->jRpc->setNextId(1000);
    JsonRpc::mirror mir;
    if (!spr->jRpc->getOvsdbMirrorConfig(mir)) {
        return false;
    }
    string erspanUuid;
    spr->jRpc->getPortUuid("erspan", erspanUuid);
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

    string sessionName("abc");
    if (!spr->deleteMirror(sessionName)) {
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
    return spr->createMirror("sess1", src_ports, dst_ports);
}

BOOST_FIXTURE_TEST_CASE( verify_getport, SpanRendererFixture ) {
    WAIT_FOR(verifyCreateDestroy(spr), 500);
}

BOOST_FIXTURE_TEST_CASE( verify_add_remote_port, SpanRendererFixture ) {
    spr->jRpc->setNextId(1013);

    BOOST_CHECK_EQUAL(true, spr->addErspanPort("br-int", "3.3.3.3", 2));
    BOOST_CHECK_EQUAL(true, spr->deleteErspanPort("erspan"));
}

BOOST_AUTO_TEST_SUITE_END()

}
