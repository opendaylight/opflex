/*
 * Test suite for class NetFlowRenderer
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include <NetFlowRenderer.h>
#include "MockJsonRpc.h"

namespace opflexagent {

using namespace std;
using namespace rapidjson;

BOOST_AUTO_TEST_SUITE(NetFlowRenderer_test)

class NetFlowRendererFixture : public BaseFixture {
public:
    NetFlowRendererFixture() : BaseFixture() {
        nfr = make_shared<NetFlowRenderer>(agent);
        initLogging("debug", false, "");
        conn.reset(new MockRpcConnection());
        nfr->start("br-int", conn.get());
        nfr->connect();
    }

    virtual ~NetFlowRendererFixture() {};

    shared_ptr<NetFlowRenderer> nfr;
    unique_ptr<OvsdbConnection> conn;
};

static bool verifyCreateDestroy(const shared_ptr<NetFlowRenderer>& nfr) {
    nfr->jRpc->setNextId(2000);
    string bridgeUuid;
    nfr->jRpc->getBridgeUuid("br-int", bridgeUuid);

    bool result = nfr->jRpc->createNetFlow(bridgeUuid, "5.5.5.6", 10, true);
    result = result && nfr->jRpc->deleteNetFlow(bridgeUuid);

    nfr->jRpc->setNextId(2001);
    result = result && nfr->jRpc->createIpfix(bridgeUuid, "5.5.5.5", 500);
    result = result && nfr->jRpc->deleteIpfix(bridgeUuid);
    return result;
}

BOOST_FIXTURE_TEST_CASE( verify_createdestroy, NetFlowRendererFixture ) {
    WAIT_FOR(verifyCreateDestroy(nfr), 1);
}
BOOST_AUTO_TEST_SUITE_END()

}