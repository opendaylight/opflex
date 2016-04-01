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

#include <opflex/modb/Mutator.h>
#include <modelgbp/gbp/SecGroup.hpp>

#include <boost/shared_ptr.hpp>

#include <vector>

BOOST_AUTO_TEST_SUITE(AccessFlowManager_test)

using std::vector;
using std::string;
using boost::shared_ptr;
using boost::thread;
using boost::asio::io_service;
using boost::ref;
using boost::bind;
using namespace ovsagent;
using namespace modelgbp::gbp;
using modelgbp::gbpe::L24Classifier;
using opflex::modb::Mutator;

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

    /** Initialize security group flow entries */
    uint16_t initExpSecGrp1(uint16_t prio, uint32_t setId);
    uint16_t initExpSecGrp2(uint16_t prio, uint32_t setId);
    void initExpSecGrpSet1();
    void initExpSecGrpSet12(bool second = true);

    AccessFlowManager accessFlowManager;

    shared_ptr<SecGroup> secGrp1;
    shared_ptr<SecGroup> secGrp2;
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

BOOST_FIXTURE_TEST_CASE(secGrp, AccessFlowManagerFixture) {
    createObjects();
    createPolicyObjects();
    {
        Mutator mutator(framework, "policyreg");
        secGrp1 = space->addGbpSecGroup("secgrp1");
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule1")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(100)
            .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule2")
            ->setDirection(DirectionEnumT::CONST_OUT).setOrder(200)
            .addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule3")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(300)
            .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule4")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(400)
            .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());
        mutator.commit();
    }

    ep0.reset(new Endpoint("0-0-0-0"));
    epSrc.updateEndpoint(*ep0);

    initExpStatic();
    WAIT_FOR_TABLES("empty-secgrp", 500);

    ep0->addSecurityGroup(secGrp1->getURI());
    epSrc.updateEndpoint(*ep0);

    clearExpFlowTables();
    initExpStatic();
    initExpSecGrpSet1();
    WAIT_FOR_TABLES("one-secgrp", 500);

    ep0->addSecurityGroup(opflex::modb::URI("/PolicyUniverse/PolicySpace"
                                            "/tenant0/GbpSecGroup/secgrp2/"));
    epSrc.updateEndpoint(*ep0);

    clearExpFlowTables();
    initExpStatic();
    initExpSecGrpSet12(false);
    WAIT_FOR_TABLES("two-secgrp-nocon", 500);

    {
        Mutator mutator(framework, "policyreg");
        secGrp2 = space->addGbpSecGroup("secgrp2");
        secGrp2->addGbpSecGroupSubject("2_subject1")
            ->addGbpSecGroupRule("2_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier0->getURI().toString());
        secGrp2->addGbpSecGroupSubject("2_subject1")
            ->addGbpSecGroupRule("2_1_rule2")
            ->setDirection(DirectionEnumT::CONST_BIDIRECTIONAL).setOrder(20)
            .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());
        mutator.commit();
    }

    clearExpFlowTables();
    initExpStatic();
    initExpSecGrpSet12(true);
    WAIT_FOR_TABLES("two-secgrp", 500);
}

#define ADDF(flow) addExpFlowEntry(expTables, flow)
enum TABLE {
    GRP = 0, IN_POL = 1, OUT_POL = 2, OUT = 3,
};

void AccessFlowManagerFixture::initExpStatic() {
    ADDF(Bldr().table(OUT).priority(1).isMdAct(0)
         .actions().out(OUTPORT).done());

    ADDF(Bldr().table(OUT_POL).priority(flowutils::MAX_POLICY_RULE_PRIORITY)
         .reg(SEPG, 1).actions().go(OUT).done());
    ADDF(Bldr().table(IN_POL).priority(flowutils::MAX_POLICY_RULE_PRIORITY)
         .reg(SEPG, 1).actions().go(OUT).done());
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

void AccessFlowManagerFixture::initExpSecGrpSet1() {
    uint32_t setId = idGen.getId("secGroupSet", secGrp1->getURI().toString());
    initExpSecGrp1(flowutils::MAX_POLICY_RULE_PRIORITY, setId);
}

void AccessFlowManagerFixture::initExpSecGrpSet12(bool second) {
    uint32_t setId = idGen.getId("secGroupSet",
                                 secGrp1->getURI().toString() +
                                 ",/PolicyUniverse/PolicySpace/tenant0"
                                 "/GbpSecGroup/secgrp2/");
    uint16_t prio = flowutils::MAX_POLICY_RULE_PRIORITY;
    prio = prio - initExpSecGrp1(prio, setId);
    if (second)
        prio = prio - initExpSecGrp2(prio, setId);
}

uint16_t AccessFlowManagerFixture::initExpSecGrp1(uint16_t prio,
                                                  uint32_t setId) {
    uint32_t grpId = idGen.getId("secGroup", secGrp1->getURI().toString());

    /* classifer 1  */
    ADDF(Bldr().table(IN_POL).priority(prio).cookie(grpId)
         .tcp().reg(SEPG, setId).isTpDst(80).actions().go(OUT).done());
    /* classifier 2  */
    ADDF(Bldr().table(OUT_POL).priority(prio-1).cookie(grpId)
         .arp().reg(SEPG, setId).actions().go(OUT).done());
    /* classifier 6 */
    ADDF(Bldr().table(IN_POL).priority(prio-2).cookie(grpId)
         .tcp().reg(SEPG, setId).isTpSrc(22)
         .isTcpFlags("+syn+ack").actions().go(OUT).done());
    /* classifier 7 */
    ADDF(Bldr().table(IN_POL).priority(prio-3).cookie(grpId)
         .tcp().reg(SEPG, setId).isTpSrc(21)
         .isTcpFlags("+ack").actions().go(OUT).done());
    ADDF(Bldr().table(IN_POL).priority(prio-3).cookie(grpId)
         .tcp().reg(SEPG, setId).isTpSrc(21)
         .isTcpFlags("+rst").actions().go(OUT).done());

    return 4;
}

uint16_t AccessFlowManagerFixture::initExpSecGrp2(uint16_t prio,
                                                  uint32_t setId) {
    uint32_t grpId = idGen.getId("secGroup", secGrp2->getURI().toString());
    ADDF(Bldr().table(IN_POL).priority(prio).cookie(grpId)
         .reg(SEPG, setId).isEth(0x8906).actions().go(OUT).done());
    ADDF(Bldr().table(OUT_POL).priority(prio).cookie(grpId)
         .reg(SEPG, setId).isEth(0x8906).actions().go(OUT).done());
    return 1;
}

BOOST_AUTO_TEST_SUITE_END()
