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
#include "FlowConstants.h"
#include "FlowUtils.h"

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

    /** Initialize static entries */
    void initExpStatic();

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

    initExpStatic();
    initExpEp(ep0);
    WAIT_FOR_TABLES("create", 500);

    ep1.reset(new Endpoint("0-0-0-1"));
    ep1->setAccessInterface("ep1-access");
    portmapper.ports[ep1->getAccessInterface().get()] = 17;
    portmapper.RPortMap[17] = ep1->getAccessInterface().get();
    epSrc.updateEndpoint(*ep1);
    epSrc.removeEndpoint(ep0->getUUID());

    clearExpFlowTables();
    initExpStatic();
    WAIT_FOR_TABLES("remove", 500);

    ep1->setAccessUplinkInterface("ep1-uplink");
    portmapper.ports[ep1->getAccessUplinkInterface().get()] = 18;
    portmapper.RPortMap[18] = ep1->getAccessUplinkInterface().get();
    epSrc.updateEndpoint(*ep1);

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep1);
    WAIT_FOR_TABLES("uplink-added", 500);

    ep2.reset(new Endpoint("0-0-0-2"));
    ep2->setAccessInterface("ep2-access");
    ep2->setAccessUplinkInterface("ep2-uplink");
    epSrc.updateEndpoint(*ep2);
    epSrc.updateEndpoint(*ep0);
    epSrc.removeEndpoint(ep1->getUUID());

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0);
    WAIT_FOR_TABLES("missing-portmap", 500);

    portmapper.ports[ep1->getAccessInterface().get()] = 91;
    portmapper.ports[ep1->getAccessUplinkInterface().get()] = 92;
    portmapper.RPortMap[91] = ep1->getAccessInterface().get();
    portmapper.RPortMap[92] = ep1->getAccessUplinkInterface().get();
    accessFlowManager.portStatusUpdate("ep2-access", 91, false);

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0);
    initExpEp(ep2);
    WAIT_FOR_TABLES("portmap-added", 500);
}

#define ADDF(flow) addExpFlowEntry(expTables, flow)
enum TABLE {
    GRP = 0, IN_POL = 1, OUT_POL = 2, OUT = 3,
};

void AccessFlowManagerFixture::initExpStatic() {
    ADDF(Bldr().table(OUT).priority(1).isMdAct(0)
         .actions().out(OUTPORT).done());
}

void AccessFlowManagerFixture::initExpEp(shared_ptr<Endpoint>& ep) {
    uint32_t access = portmapper.FindPort(ep->getAccessInterface().get());
    uint32_t uplink = portmapper.FindPort(ep->getAccessUplinkInterface().get());

    if (access != OFPP_NONE && uplink != OFPP_NONE) {
        ADDF(Bldr().table(GRP).priority(100).in(access)
             .actions().load(SEPG, 1).load(OUTPORT, uplink).go(OUT_POL).done());
        ADDF(Bldr().table(GRP).priority(100).in(uplink)
             .actions().load(SEPG, 1).load(OUTPORT, access).go(IN_POL).done());
    }
}

BOOST_AUTO_TEST_SUITE_END()
