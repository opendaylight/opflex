/*
 * Test suite for class AccessFlowManager
 *
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "BaseFixture.h"
#include "FlowManagerFixture.h"
#include "AccessFlowManager.h"

#include <boost/shared_ptr.hpp>

#include <vector>

BOOST_AUTO_TEST_SUITE(AccessFlowManager_test)

using namespace ovsagent;
using std::vector;
using std::string;
using boost::shared_ptr;
using boost::thread;
using boost::asio::io_service;
using boost::ref;
using boost::bind;

class AccessFlowManagerFixture : public FlowManagerFixture {
public:
    AccessFlowManagerFixture()
        : accessFlowManager(agent, switchManager, idGen) {
        expTables.resize(AccessFlowManager::NUM_FLOW_TABLES);
        switchManager.registerStateHandler(&accessFlowManager);
        start();
        accessFlowManager.start();
    }
    virtual ~AccessFlowManagerFixture() {
        accessFlowManager.stop();
        stop();
    }

    /** Initialize endpoint-scoped flow entries */
    void initExpEp(shared_ptr<Endpoint>& ep);

    AccessFlowManager accessFlowManager;
};

BOOST_FIXTURE_TEST_CASE(endpoint, AccessFlowManagerFixture) {
    setConnected();
    ep0.reset(new Endpoint("0-0-0-0"));
    ep0->setAccessInterface("ep0-access");
    ep0->setAccessUplinkInterface("ep0-uplink");

    portmapper.ports[ep0->getAccessInterface().get()] = 42;
    portmapper.ports[ep0->getAccessUplinkInterface().get()] = 24;
    portmapper.RPortMap[42] = ep0->getAccessInterface().get();
    portmapper.RPortMap[24] = ep0->getAccessUplinkInterface().get();
    epSrc.updateEndpoint(*ep0);

    initExpEp(ep0);
    WAIT_FOR_TABLES("create", 500);
}

#define ADDF(flow) addExpFlowEntry(expTables, flow)
enum TABLE {
    GRP = 0, IN = 1, OUT = 2
};

void AccessFlowManagerFixture::initExpEp(shared_ptr<Endpoint>& ep) {
    uint32_t access = portmapper.FindPort(ep->getAccessInterface().get());
    uint32_t uplink = portmapper.FindPort(ep->getAccessUplinkInterface().get());

    if (access != OFPP_NONE && uplink != OFPP_NONE) {
        ADDF(Bldr().table(GRP).priority(100).in(access)
             .actions().load(SEPG, 1).load(OUTPORT, uplink).go(OUT).done());
        ADDF(Bldr().table(GRP).priority(100).in(uplink)
             .actions().load(SEPG, 1).load(OUTPORT, access).go(IN).done());
    }
}

BOOST_AUTO_TEST_SUITE_END()
