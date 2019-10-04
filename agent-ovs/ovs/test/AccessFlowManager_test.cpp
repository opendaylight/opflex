/*
 * Test suite for class AccessFlowManager
 *
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/test/BaseFixture.h>
#include <opflexagent/LearningBridgeSource.h>
#include "FlowManagerFixture.h"
#include "AccessFlowManager.h"
#include "CtZoneManager.h"
#include "FlowConstants.h"
#include "FlowUtils.h"

#include <opflex/modb/Mutator.h>
#include <modelgbp/gbp/SecGroup.hpp>

#include <memory>
#include <vector>

BOOST_AUTO_TEST_SUITE(AccessFlowManager_test)

using std::vector;
using std::string;
using std::shared_ptr;
using std::thread;
using boost::asio::io_service;
using opflex::modb::URI;
using namespace opflexagent;
using namespace modelgbp::gbp;
using modelgbp::gbpe::L24Classifier;
using opflex::modb::Mutator;

class AccessFlowManagerFixture : public FlowManagerFixture {
public:
    AccessFlowManagerFixture()
        : accessFlowManager(agent, switchManager, idGen, ctZoneManager) {
        expTables.resize(AccessFlowManager::NUM_FLOW_TABLES);
        switchManager.registerStateHandler(&accessFlowManager);
        idGen.initNamespace("l24classifierRule");
        start();
        accessFlowManager.enableConnTrack();
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
    uint16_t initExpSecGrp1(uint32_t setId, int remoteAddress);
    uint16_t initExpSecGrp2(uint32_t setId);
    void initExpSecGrpSet1();
    void initExpSecGrpSet12(bool second = true, int remoteAddress = 0);

    AccessFlowManager accessFlowManager;

    shared_ptr<SecGroup> secGrp1;
    shared_ptr<SecGroup> secGrp2;

    /* Initialize dhcp flow entries */
    void initExpDhcpEp(shared_ptr<Endpoint>& ep);

    /* Initialize learning bridge entries */
    void initExpLearningBridge();
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

    ep0->setAccessIfaceVlan(223);
    epSrc.updateEndpoint(*ep0);

    clearExpFlowTables();
    initExpStatic();
    initExpEp(ep0);
    initExpEp(ep2);
    WAIT_FOR_TABLES("access-vlan-added", 500);

    Endpoint::DHCPv4Config v4;
    Endpoint::DHCPv6Config v6;
    ep0->setAccessIfaceVlan(223);
    ep0->setDHCPv4Config(v4);
    ep0->setDHCPv6Config(v6);
    epSrc.updateEndpoint(*ep0);

    clearExpFlowTables();
    initExpStatic();
    initExpDhcpEp(ep0);
    WAIT_FOR_TABLES("dhcp-configured", 500);
}

BOOST_FIXTURE_TEST_CASE(learningBridge, AccessFlowManagerFixture) {
    setConnected();

    ep0.reset(new Endpoint("0-0-0-0"));
    ep0->setInterfaceName("ep0-int");
    ep0->setAccessInterface("ep0-access");
    ep0->setAccessUplinkInterface("ep0-uplink");
    portmapper.ports[ep0->getAccessInterface().get()] = 42;
    portmapper.ports[ep0->getAccessUplinkInterface().get()] = 24;
    portmapper.RPortMap[42] = ep0->getAccessInterface().get();
    portmapper.RPortMap[24] = ep0->getAccessUplinkInterface().get();
    epSrc.updateEndpoint(*ep0);

    LearningBridgeSource lbSource(&agent.getLearningBridgeManager());
    LearningBridgeIface if1;
    if1.setUUID("1");
    if1.setInterfaceName(ep0->getInterfaceName().get());
    if1.setTrunkVlans({ {0x400,0x4ff} });
    lbSource.updateLBIface(if1);

    initExpStatic();
    initExpEp(ep0);
    initExpLearningBridge();
    WAIT_FOR_TABLES("create", 500);
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
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(150)
            .addGbpRuleToClassifierRSrc(classifier8->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule3")
            ->setDirection(DirectionEnumT::CONST_OUT).setOrder(200)
            .addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule4")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(300)
            .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule5")
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

    LOG(DEBUG) << "two-secgrp-nocon";
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
        secGrp2->addGbpSecGroupSubject("2_subject1")
            ->addGbpSecGroupRule("2_1_rule3")
            ->setDirection(DirectionEnumT::CONST_OUT).setOrder(30)
            .addGbpRuleToClassifierRSrc(classifier9->getURI().toString());
        mutator.commit();
    }

    clearExpFlowTables();
    initExpStatic();
    initExpSecGrpSet12(true);
    WAIT_FOR_TABLES("two-secgrp", 500);

    shared_ptr<modelgbp::gbp::Subnets> rs;
    {
        Mutator mutator(framework, "policyreg");
        rs = space->addGbpSubnets("subnets_rule1");

        rs->addGbpSubnet("subnets_rule1_1")
            ->setAddress("192.168.0.0")
            .setPrefixLen(16);
        rs->addGbpSubnet("subnets_rule1_2")
            ->setAddress("fd80::")
            .setPrefixLen(32);

        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule1")
            ->addGbpSecGroupRuleToRemoteAddressRSrc(rs->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule2")
            ->addGbpSecGroupRuleToRemoteAddressRSrc(rs->getURI().toString());
        secGrp1->addGbpSecGroupSubject("1_subject1")
            ->addGbpSecGroupRule("1_1_rule3")
            ->addGbpSecGroupRuleToRemoteAddressRSrc(rs->getURI().toString());

        mutator.commit();
    }
    clearExpFlowTables();
    initExpStatic();
    initExpSecGrpSet12(true, 1);
    WAIT_FOR_TABLES("remote-secgrp", 500);

    {
        Mutator mutator(framework, "policyreg");

        rs->addGbpSubnet("subnets_rule1_3")
            ->setAddress("10.0.0.0")
            .setPrefixLen(8);
        rs->addGbpSubnet("subnets_rule1_4")
            ->setAddress("fd34:9c39:1374:358c::")
            .setPrefixLen(64);

        mutator.commit();
    }

    clearExpFlowTables();
    initExpStatic();
    initExpSecGrpSet12(true, 2);
    WAIT_FOR_TABLES("remote-addsubnets", 500);
}

#define ADDF(flow) addExpFlowEntry(expTables, flow)
enum TABLE {
    DROP_LOG=0, GRP = 1, IN_POL = 2, OUT_POL = 3, OUT = 4, EXP_DROP=5
};

void AccessFlowManagerFixture::initExpStatic() {
    ADDF(Bldr().table(OUT).priority(1).isMdAct(0)
         .actions().out(OUTPORT).done());
    ADDF(Bldr().table(OUT).priority(1)
         .isMdAct(flow::meta::access_out::PUSH_VLAN)
         .actions().pushVlan().move(FD12, VLAN).out(OUTPORT).done());
    ADDF(Bldr().table(OUT).priority(1)
         .isMdAct(flow::meta::access_out::POP_VLAN)
         .isVlanTci("0x1000/0x1000")
         .actions().popVlan().out(OUTPORT).done());

    ADDF(Bldr().table(OUT_POL).priority(PolicyManager::MAX_POLICY_RULE_PRIORITY)
         .reg(SEPG, 1).actions().go(OUT).done());
    ADDF(Bldr().table(IN_POL).priority(PolicyManager::MAX_POLICY_RULE_PRIORITY)
         .reg(SEPG, 1).actions().go(OUT).done());
    ADDF(Bldr().table(DROP_LOG).priority(0)
            .actions().go(GRP).done());
}

void AccessFlowManagerFixture::initExpDhcpEp(shared_ptr<Endpoint>& ep) {
    uint32_t access = portmapper.FindPort(ep->getAccessInterface().get());
    uint32_t uplink = portmapper.FindPort(ep->getAccessUplinkInterface().get());

    if (access == OFPP_NONE || uplink == OFPP_NONE) return;

    initExpEp(ep);
    if (ep->getDHCPv4Config()) {
        ADDF(Bldr()
             .table(GRP).priority(ep->getAccessIfaceVlan() ? 201 : 200).udp().in(access)
             .isVlan(ep->getAccessIfaceVlan().get())
             .isTpSrc(68).isTpDst(67)
             .actions()
             .load(OUTPORT, uplink)
             .mdAct(flow::meta::access_out::POP_VLAN)
             .go(OUT).done());
    }
    if (ep->getDHCPv6Config()) {
        ADDF(Bldr()
             .table(GRP).priority(ep->getAccessIfaceVlan() ? 201 : 200).udp6().in(access)
             .isVlan(ep->getAccessIfaceVlan().get())
             .isTpSrc(546).isTpDst(547)
             .actions()
             .load(OUTPORT, uplink)
             .mdAct(flow::meta::access_out::POP_VLAN)
             .go(OUT).done());
    }
}

void AccessFlowManagerFixture::initExpEp(shared_ptr<Endpoint>& ep) {
    uint32_t access = portmapper.FindPort(ep->getAccessInterface().get());
    uint32_t uplink = portmapper.FindPort(ep->getAccessUplinkInterface().get());
    uint32_t zoneId = idGen.getId("conntrack", ep->getUUID());

    if (access == OFPP_NONE || uplink == OFPP_NONE) return;

    if (ep->getAccessIfaceVlan()) {
        ADDF(Bldr().table(GRP).priority(100).in(access)
             .isVlan(ep->getAccessIfaceVlan().get())
             .actions()
             .load(RD, zoneId).load(SEPG, 1)
             .load(OUTPORT, uplink)
             .mdAct(flow::meta::access_out::POP_VLAN)
             .go(OUT_POL).done());
        ADDF(Bldr().table(GRP).priority(100).in(uplink)
             .actions().load(RD, zoneId).load(SEPG, 1).load(OUTPORT, access)
             .load(FD, ep->getAccessIfaceVlan().get())
             .mdAct(flow::meta::access_out::PUSH_VLAN)
             .go(IN_POL).done());
    } else {
        ADDF(Bldr().table(GRP).priority(100).in(access)
             .noVlan()
             .actions().load(RD, zoneId).load(SEPG, 1)
             .load(OUTPORT, uplink).go(OUT_POL).done());
        ADDF(Bldr().table(GRP).priority(100).in(uplink)
             .actions().load(RD, zoneId).load(SEPG, 1)
             .load(OUTPORT, access).go(IN_POL).done());
    }
}

void AccessFlowManagerFixture::initExpLearningBridge() {
    ADDF(Bldr().table(GRP).priority(500).in(24)
         .isVlanTci("0x1400/0x1f00")
         .actions().outPort(42).done());
    ADDF(Bldr().table(GRP).priority(500).in(42)
         .isVlanTci("0x1400/0x1f00")
         .actions().outPort(24).done());
}

void AccessFlowManagerFixture::initExpSecGrpSet1() {
    uint32_t setId = idGen.getId("secGroupSet", secGrp1->getURI().toString());
    initExpSecGrp1(setId, 0);
}

void AccessFlowManagerFixture::initExpSecGrpSet12(bool second,
                                                  int remoteAddress) {
    uint32_t setId = idGen.getId("secGroupSet",
                                 secGrp1->getURI().toString() +
                                 ",/PolicyUniverse/PolicySpace/tenant0"
                                 "/GbpSecGroup/secgrp2/");
    initExpSecGrp1(setId, remoteAddress);
    if (second)
        initExpSecGrp2(setId);
}

uint16_t AccessFlowManagerFixture::initExpSecGrp1(uint32_t setId,
                                                  int remoteAddress) {
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::rule_list_t rules;
    agent.getPolicyManager().getSecGroupRules(secGrp1->getURI(), rules);
    uint32_t ruleId;

    /* classifer 1  */
    ruleId = idGen.getId("l24classifierRule",
                         classifier1->getURI().toString());
    if (remoteAddress) {
        ADDF(Bldr().table(IN_POL).priority(prio).cookie(ruleId)
             .tcp().reg(SEPG, setId).isIpSrc("192.168.0.0/16").isTpDst(80)
             .actions().go(OUT).done());
        if (remoteAddress > 1)
            ADDF(Bldr().table(IN_POL).priority(prio).cookie(ruleId)
                 .tcp().reg(SEPG, setId).isIpSrc("10.0.0.0/8").isTpDst(80)
                 .actions().go(OUT).done());
    } else {
        ADDF(Bldr().table(IN_POL).priority(prio).cookie(ruleId)
             .tcp().reg(SEPG, setId).isTpDst(80).actions().go(OUT).done());
    }
    /* classifer 8  */
    ruleId = idGen.getId("l24classifierRule",
                         classifier8->getURI().toString());
    if (remoteAddress) {
        ADDF(Bldr().table(IN_POL).priority(prio-128).cookie(ruleId)
             .tcp6().reg(SEPG, setId).isIpv6Src("fd80::/32").isTpDst(80)
             .actions().go(OUT).done());
        if (remoteAddress > 1)
            ADDF(Bldr().table(IN_POL).priority(prio-128).cookie(ruleId)
                 .tcp6().reg(SEPG, setId)
                 .isIpv6Src("fd34:9c39:1374:358c::/64")
                 .isTpDst(80).actions().go(OUT).done());
    } else {
        ADDF(Bldr().table(IN_POL).priority(prio-128).cookie(ruleId)
             .tcp6().reg(SEPG, setId).isTpDst(80).actions().go(OUT).done());
    }
    /* classifier 2  */
    ruleId = idGen.getId("l24classifierRule",
                         classifier2->getURI().toString());
    if (remoteAddress) {
        ADDF(Bldr().table(OUT_POL).priority(prio-256).cookie(ruleId)
             .arp().reg(SEPG, setId).isTpa("192.168.0.0/16").actions()
             .go(OUT).done());
        if (remoteAddress > 1)
            ADDF(Bldr().table(OUT_POL).priority(prio-256).cookie(ruleId)
                 .arp().reg(SEPG, setId).isTpa("10.0.0.0/8").actions()
                 .go(OUT).done());
    } else {
        ADDF(Bldr().table(OUT_POL).priority(prio-256).cookie(ruleId)
             .arp().reg(SEPG, setId).actions().go(OUT).done());
    }
    /* classifier 6 */
    ruleId = idGen.getId("l24classifierRule",
                         classifier6->getURI().toString());
    ADDF(Bldr().table(IN_POL).priority(prio-384).cookie(ruleId)
         .tcp().reg(SEPG, setId).isTpSrc(22)
         .isTcpFlags("+syn+ack").actions().go(OUT).done());
    /* classifier 7 */
    ruleId = idGen.getId("l24classifierRule",
                         classifier7->getURI().toString());
    ADDF(Bldr().table(IN_POL).priority(prio-512).cookie(ruleId)
         .tcp().reg(SEPG, setId).isTpSrc(21)
         .isTcpFlags("+ack").actions().go(OUT).done());
    ADDF(Bldr().table(IN_POL).priority(prio-512).cookie(ruleId)
         .tcp().reg(SEPG, setId).isTpSrc(21)
         .isTcpFlags("+rst").actions().go(OUT).done());

    return 512;
}

uint16_t AccessFlowManagerFixture::initExpSecGrp2(uint32_t setId) {
    uint16_t prio = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    PolicyManager::rule_list_t rules;
    agent.getPolicyManager().getSecGroupRules(secGrp2->getURI(), rules);
    uint32_t ruleId;

    /* classifier 5 */
    ruleId = idGen.getId("l24classifierRule",
                         classifier5->getURI().toString());
    ADDF(Bldr().table(IN_POL).priority(prio).cookie(ruleId)
         .reg(SEPG, setId).isEth(0x8906).actions().go(OUT).done());
    ADDF(Bldr().table(OUT_POL).priority(prio).cookie(ruleId)
         .reg(SEPG, setId).isEth(0x8906).actions().go(OUT).done());

    /* classifier 9 */
    ruleId = idGen.getId("l24classifierRule",
                         classifier9->getURI().toString());
    ADDF(Bldr().table(IN_POL).priority(prio - 128).cookie(ruleId)
         .isCtState("-new+est-rel+rpl-inv+trk").tcp().reg(SEPG, setId)
         .actions().go(OUT).done());
    ADDF(Bldr().table(IN_POL).priority(prio - 128).cookie(ruleId)
         .isCtState("-new-est+rel-inv+trk").tcp().reg(SEPG, setId)
         .actions().go(OUT).done());
    ADDF(Bldr().table(IN_POL).priority(prio - 128).cookie(ruleId)
         .isCtState("-trk").tcp().reg(SEPG, setId)
         .actions().ct("table=1,zone=NXM_NX_REG6[0..15]").done());
    ADDF(Bldr().table(OUT_POL).priority(prio - 128).cookie(ruleId)
         .isCtState("-trk")
         .tcp().reg(SEPG, setId).isTpDst(22)
         .actions().ct("table=1,zone=NXM_NX_REG6[0..15]").done());
    ADDF(Bldr().table(OUT_POL).priority(prio - 128).cookie(ruleId)
         .isCtState("+est+trk")
         .tcp().reg(SEPG, setId).isTpDst(22)
         .actions()
         .go(OUT).done());
    ADDF(Bldr().table(OUT_POL).priority(prio - 128).cookie(ruleId)
         .isCtState("+new+trk")
         .tcp().reg(SEPG, setId).isTpDst(22)
         .actions().ct("commit,zone=NXM_NX_REG6[0..15]")
         .go(OUT).done());

    return 1;
}

BOOST_AUTO_TEST_SUITE_END()
