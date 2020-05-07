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
#include "MockRpcConnection.h"
#include <opflexagent/SpanSessionState.h>

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
    spr->setNextId(1000);
    JsonRpc::mirror mir;
    if (!spr->jRpc->getOvsdbMirrorConfig("br-int", mir)) {
        return false;
    }
    string erspanUuid;
    spr->jRpc->getPortUuid("erspan", erspanUuid);
    if (erspanUuid.empty()) {
        return false;
    }
    if (!spr->jRpc->updateBridgePorts("br-int", erspanUuid, false)) {
        return false;
    }

    string sessionName("abc");
    if (!spr->deleteMirror(sessionName)) {
        return false;
    }
    ErspanParams params;
    params.setPortName("erspan");
    params.setRemoteIp("10.20.120.240");
    params.setVersion(1);
    if (!spr->jRpc->addErspanPort("br-int", params)) {
        return false;
    }
    set<string> src_ports = {"p1-tap", "p2-tap"};
    set<string> dst_ports = {"p1-tap", "p2-tap"};
    set<string> out_ports = {"erspan"};
    return spr->createMirror("sess1", src_ports, dst_ports);
}

BOOST_FIXTURE_TEST_CASE( verify_getport, SpanRendererFixture ) {
    BOOST_CHECK_EQUAL(true,verifyCreateDestroy(spr));
}

BOOST_FIXTURE_TEST_CASE( verify_add_remote_port, SpanRendererFixture ) {
    spr->setNextId(1011);

    BOOST_CHECK_EQUAL(true, spr->addErspanPort(ERSPAN_PORT_PREFIX, "3.3.3.3", 2));
    BOOST_CHECK_EQUAL(true, spr->deleteErspanPort(ERSPAN_PORT_PREFIX));
}

BOOST_FIXTURE_TEST_CASE( verify_get_erspan_params, SpanRendererFixture ) {
    spr->setNextId(1014);

    ErspanParams params;
    BOOST_CHECK_EQUAL(true, spr->jRpc->getCurrentErspanParams(ERSPAN_PORT_PREFIX, params));

    URI spanUri("/SpanUniverse/SpanSession/ugh-vspan/");
    spr->spanUpdated(spanUri);
}

BOOST_AUTO_TEST_SUITE_END()

}
