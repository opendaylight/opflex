/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Policies
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/AutoconfigEnumT.hpp>
#include <modelgbp/span/Universe.hpp>
#include <opflex/modb/Mutator.h>

#include "Policies.h"
#include <modelgbp/gbp/L3IfTypeEnumT.hpp>

namespace opflexagent {

using std::shared_ptr;
using boost::optional;
using opflex::modb::Mutator;
using opflex::modb::URI;

using namespace modelgbp;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace modelgbp::l2;

void Policies::writeBasicInit(opflex::ofcore::OFFramework& framework) {
    Mutator mutator(framework, "init");
    shared_ptr<modelgbp::dmtree::Root> root =
        modelgbp::dmtree::Root::createRootElement(framework);
    root->addPolicyUniverse();
    root->addRelatorUniverse();
    root->addEprL2Universe();
    root->addEprL3Universe();
    root->addEpdrL2Discovered();
    root->addEpdrL3Discovered();
    root->addSpanUniverse();
    mutator.commit();
}

void Policies::writeTestPolicy(opflex::ofcore::OFFramework& framework) {
    shared_ptr<policy::Space> space;
    shared_ptr<policy::Space> common;
    shared_ptr<FloodDomain> fd1;
    shared_ptr<FloodDomain> fd2;
    shared_ptr<FloodDomain> fd3;
    shared_ptr<FloodDomain> fd4;
    shared_ptr<FloodDomain> fd5;
    shared_ptr<FloodDomain> fd6;
    shared_ptr<FloodDomain> fd_ext;
    shared_ptr<RoutingDomain> rd;
    shared_ptr<L3ExternalDomain> l3ext;
    shared_ptr<L3ExternalNetwork> l3ext_net;
    shared_ptr<RoutingDomain> rd_ext;
    shared_ptr<BridgeDomain> bd;
    shared_ptr<BridgeDomain> bd2;
    shared_ptr<BridgeDomain> bd_ext;
    shared_ptr<BridgeDomain> bd3;
    shared_ptr<BridgeDomain> bd4;
    shared_ptr<BridgeDomain> bd5;
    shared_ptr<BridgeDomain> bd6;
    shared_ptr<Subnets> subnetsfd1;
    shared_ptr<Subnets> subnetsfd2;
    shared_ptr<Subnets> subnetsfd3;
    shared_ptr<Subnet> subnetsfd1_1;
    shared_ptr<Subnet> subnetsfd1_2;
    shared_ptr<Subnet> subnetsfd1_3;
    shared_ptr<Subnet> subnetsfd2_1;
    shared_ptr<Subnet> subnetsfd2_2;
    shared_ptr<Subnet> subnetsfd3_1;
    shared_ptr<Subnets> subnetsbd;
    shared_ptr<Subnet> subnetsbd1;
    shared_ptr<Subnets> subnetsrd;
    shared_ptr<Subnet> subnetsrd1;
    shared_ptr<Subnets> subnets_ext;
    shared_ptr<Subnets> subnetsfd4;
    shared_ptr<Subnet> subnetsfd4_1;
    shared_ptr<Subnets> subnetsfd5;
    shared_ptr<Subnet> subnetsfd5_1;
    shared_ptr<Subnets> subnetsfd6;
    shared_ptr<Subnet> subnetsfd6_1;

    shared_ptr<EpGroup> eg1;
    shared_ptr<EpGroup> eg2;
    shared_ptr<EpGroup> eg3;
    shared_ptr<EpGroup> eg4;
    shared_ptr<EpGroup> eg5;
    shared_ptr<EpGroup> eg_nat;
    shared_ptr<EpGroup> eg6;

    shared_ptr<L24Classifier> classifier1;
    shared_ptr<L24Classifier> classifier2;
    shared_ptr<L24Classifier> classifier3;
    shared_ptr<L24Classifier> classifier4;
    shared_ptr<L24Classifier> classifier5;
    shared_ptr<L24Classifier> classifier6;
    shared_ptr<L24Classifier> classifier7;

    shared_ptr<Contract> con1;
    shared_ptr<Contract> con2;
    shared_ptr<Contract> con3;
    shared_ptr<AllowDenyAction> action1;
    shared_ptr<RedirectAction> action3;

    shared_ptr<SecGroup> secGrp1;
    shared_ptr<SecGroup> secGrp2;
    shared_ptr<SecGroup> secGrp3;

    shared_ptr<RedirectDestGroup> redirDstGrp1;
    shared_ptr<RedirectDest> redirDst1;
    shared_ptr<RedirectDest> redirDst2;
    shared_ptr<RedirectDest> redirDst3;

    shared_ptr<L3ExternalDomain> l3ext2;
    shared_ptr<L3ExternalNetwork> l3ext2_net;
    shared_ptr<L3ExternalNetwork> l3ext2_net2;
    shared_ptr<ExternalInterface> ext_int1;
    shared_ptr<ExternalL3BridgeDomain> ext_bd1;
    shared_ptr<ExternalNode> ext_node1;
    shared_ptr<StaticRoute> static_route1, static_route2;
    shared_ptr<RemoteRoute> remote_route1;
    shared_ptr<Subnets> subnets_l3out;
    shared_ptr<Subnet> subnets_l3out_sub1;

    shared_ptr<policy::Universe> universe =
        policy::Universe::resolve(framework).get();

    shared_ptr<span::Universe> span =
        span::Universe::resolve(framework).get();

    Mutator mutator(framework, "policyreg");
    universe->addPlatformConfig("openstack")
        ->setMulticastGroupIP("224.1.1.1");

    space = universe->addPolicySpace("test");
    fd1 = space->addGbpFloodDomain("fd");
    fd2 = space->addGbpFloodDomain("fd2");
    //fd2->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
    bd = space->addGbpBridgeDomain("bd");
    bd2 = space->addGbpBridgeDomain("bd2");
    rd = space->addGbpRoutingDomain("rd");
    rd->setIpv6Autoconfig(AutoconfigEnumT::CONST_DHCP);

    common = universe->addPolicySpace("common");
    bd_ext = common->addGbpBridgeDomain("bd_ext");
    rd_ext = common->addGbpRoutingDomain("rd_ext");
    fd_ext = common->addGbpFloodDomain("fd_ext");

    fd1->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd->getURI());
    fd1->addGbpeFloodContext()->setMulticastGroupIP("224.10.1.1");
    fd2->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd2->getURI());
    bd->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    bd->addGbpeInstContext()->setEncapId(15001);
    bd->addGbpeInstContext()->setClassid(10001);
    bd->addGbpeInstContext()->setMulticastGroupIP("224.100.1.1");
    bd2->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    bd2->addGbpeInstContext()->setEncapId(15002);
    bd2->addGbpeInstContext()->setClassid(10002);
    bd2->addGbpeInstContext()->setMulticastGroupIP("224.100.1.2");

    bd_ext->addGbpeInstContext()->setEncapId(16001);
    bd_ext->addGbpeInstContext()->setClassid(11001);
    bd_ext->addGbpeInstContext()->setMulticastGroupIP("224.200.1.1");
    fd_ext->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd_ext->getURI());
    bd_ext->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd_ext->getURI());

    subnetsfd1 = space->addGbpSubnets("subnetsfd1");
    subnetsfd1_1 = subnetsfd1->addGbpSubnet("subnetsfd1_1");
    subnetsfd1_1->setAddress("10.0.0.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("10.0.0.128");
    subnetsfd1_2 = subnetsfd1->addGbpSubnet("subnetsfd1_2");
    subnetsfd1_2->setAddress("fd8f:69d8:c12c:ca62::")
        .setIpv6AdvAutonomousFlag(0)
        .setPrefixLen(64);
    subnetsfd1_3 = subnetsfd1->addGbpSubnet("subnetsfd1_3");
    subnetsfd1_3->setAddress("fd8f:69d8:c12c:ca63::")
        .setIpv6AdvAutonomousFlag(0)
        .setPrefixLen(64);
    fd1->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsfd1->getURI());
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd1->getURI().toString());

    subnetsfd2 = space->addGbpSubnets("subnetsfd2");
    subnetsfd2_1 = subnetsfd2->addGbpSubnet("subnetsfd2_1");
    subnetsfd2_1->setAddress("10.0.1.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("10.0.1.128");
    subnetsfd2_2 = subnetsfd2->addGbpSubnet("subnetsfd2_2");
    subnetsfd2_2->setAddress("fd8c:ad36:ceb3:601f::")
        .setPrefixLen(64);
    fd2->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsfd2->getURI());
    fd2->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_HWPROXY);
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd2->getURI().toString());

    subnetsbd = space->addGbpSubnets("subnetsbd");
    subnetsbd1 = subnetsbd->addGbpSubnet("subnetsbd1");
    bd->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsbd->getURI());
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsbd1->getURI().toString());

    subnetsrd = space->addGbpSubnets("subnetsrd");
    subnetsrd1 = subnetsrd->addGbpSubnet("subnetsrd1");
    rd->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsrd->getURI());
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsrd->getURI().toString());

    subnets_ext = common->addGbpSubnets("subnets_ext");
    subnets_ext->addGbpSubnet("subnet_ext4")
        ->setAddress("5.5.5.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("5.5.5.128");
    subnets_ext->addGbpSubnet("subnet_ext6")
        ->setAddress("fdf1:9f86:d1af:6cc9::")
        .setPrefixLen(64);
    rd_ext->addGbpRoutingDomainToIntSubnetsRSrc(subnets_ext->
                                                getURI().toString());
    rd_ext->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnets_ext->getURI());
    rd_ext->addGbpeInstContext()->setEncapId(200);

    action1 = space->addGbpAllowDenyAction("action1");
    action1->setAllow(1).setOrder(5);

    // ARP
    classifier1 = space->addGbpeL24Classifier("classifier1");
    classifier1->setOrder(102)
        .setEtherT(EtherTypeEnumT::CONST_ARP);

    // ICMP
    classifier2 = space->addGbpeL24Classifier("classifier2");
    classifier2->setOrder(101)
        .setEtherT(EtherTypeEnumT::CONST_IPV4)
        .setProt(1);
    classifier3 = space->addGbpeL24Classifier("classifier3");
    classifier3->setOrder(100)
        .setEtherT(EtherTypeEnumT::CONST_IPV6)
        .setProt(58);

    // HTTP, reflexive
    classifier4 = space->addGbpeL24Classifier("classifier4");
    classifier4->setEtherT(EtherTypeEnumT::CONST_IPV4)
        .setProt(6)
        .setDFromPort(80)
        .setDToPort(80)
        .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE);

    // TCP, reflexive
    classifier5 = space->addGbpeL24Classifier("classifier5");
    classifier5->setEtherT(EtherTypeEnumT::CONST_IPV4)
        .setProt(6)
        .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE);

    // HTTP, not reflexive
    classifier6 = space->addGbpeL24Classifier("classifier6");
    classifier6->setEtherT(EtherTypeEnumT::CONST_IPV4)
        .setProt(6)
        .setDFromPort(80);

    // Pass all Ipv4 packets
    classifier7 = space->addGbpeL24Classifier("classifier7");
    classifier7->setOrder(90)
    .setEtherT(EtherTypeEnumT::CONST_IPV4);

    // Basic ARP and ICMP
    con1 = space->addGbpContract("contract1");
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->setOrder(10).setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToActionRSrcAllowDenyAction(action1->getURI().toString());

    // HTTP to provider, reflexive
    con2 = space->addGbpContract("contract2");
    con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
        ->setDirection(DirectionEnumT::CONST_IN)
        .addGbpRuleToClassifierRSrc(classifier4->getURI().toString());
    con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
        ->addGbpRuleToActionRSrcAllowDenyAction(action1->getURI().toString());

    eg1 = space->addGbpEpGroup("group1");
    eg1->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd1->getURI());
    eg1->addGbpeInstContext()->setEncapId(0x1234);
    eg1->addGbpeInstContext()->setClassid(50001);
    eg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToProvContractRSrc(con2->getURI().toString());

    eg2 = space->addGbpEpGroup("group2");
    eg2->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd2->getURI());
    eg2->addGbpeInstContext()->setEncapId(0x3000);
    eg2->addGbpeInstContext()->setClassid(50002);
    eg2->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con2->getURI().toString());

    eg3 = space->addGbpEpGroup("group3");
    eg3->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg3->addGbpeInstContext()->setEncapId(0x3456);
    eg3->addGbpeInstContext()->setClassid(50003);

    eg_nat = common->addGbpEpGroup("nat-epg");
    eg_nat->addGbpeInstContext()->setEncapId(0x4242);
    eg_nat->addGbpeInstContext()->setClassid(50500);
    eg_nat->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd_ext->getURI());

    l3ext = rd->addGbpL3ExternalDomain("ext");
    l3ext_net = l3ext->addGbpL3ExternalNetwork("outside");
    l3ext_net
        ->addGbpL3ExternalNetworkToProvContractRSrc(con1->getURI().toString());
    l3ext_net->addGbpL3ExternalNetworkToNatEPGroupRSrc()
        ->setTargetEpGroup(eg_nat->getURI());
    l3ext_net->addGbpExternalSubnet("outside")
        ->setAddress("0.0.0.0")
        .setPrefixLen(0);
    l3ext_net->addGbpExternalSubnet("outside_v6")
        ->setAddress("0::")
        .setPrefixLen(0);
    l3ext_net->addGbpeInstContext()->setClassid(40002);

    // Basic ARP and ICMP
    secGrp1 = space->addGbpSecGroup("secGrp1");
    secGrp1->addGbpSecGroupSubject("1_subject1")
        ->addGbpSecGroupRule("1_1_rule1")
        ->setOrder(10).setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
    secGrp1->addGbpSecGroupSubject("1_subject1")
        ->addGbpSecGroupRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
    secGrp1->addGbpSecGroupSubject("1_subject1")
        ->addGbpSecGroupRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());

    // HTTP out, reflexive
    secGrp2 = space->addGbpSecGroup("secGrp2");
    secGrp2->addGbpSecGroupSubject("2_subject1")
        ->addGbpSecGroupRule("2_1_rule1")
        ->setDirection(DirectionEnumT::CONST_OUT)
        .addGbpRuleToClassifierRSrc(classifier4->getURI().toString());

    // TCP out reflexive, and HTTP in
    secGrp3 = space->addGbpSecGroup("secGrp3");
    secGrp3->addGbpSecGroupSubject("3_subject1")
        ->addGbpSecGroupRule("3_1_rule1")
        ->setDirection(DirectionEnumT::CONST_OUT)
        .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());
    secGrp3->addGbpSecGroupSubject("3_subject2")
        ->addGbpSecGroupRule("3_2_rule1")
        ->setDirection(DirectionEnumT::CONST_IN)
        .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());

    // Add span related artifacts
    shared_ptr<span::Session> sess =
        span->addSpanSession("sess1");
    shared_ptr<EpGroupToSpanSessionRSrc> epg1Rsrc =
        eg1->addGbpEpGroupToSpanSessionRSrc(sess->getName().get());
    epg1Rsrc->setTargetSession(sess->getURI());
    shared_ptr<span::SrcGrp> srcGrp1 = sess->addSpanSrcGrp("SrcGrp1");
    shared_ptr<span::FilterGrp> fltGrp1 = span->addSpanFilterGrp("fltGrp1");
    shared_ptr<span::FilterEntry> fltEnt1 = fltGrp1->addSpanFilterEntry("fltEnt1");
    fltEnt1->addSpanFilterEntryToFltEntryRSrc()->setTargetL24Classifier(
            "test", "filter1");
    shared_ptr<span::SrcMember> srcMem1 =
        srcGrp1->addSpanSrcMember("SrcMem1");
    shared_ptr<span::DstGrp> dstGrp1 = sess->addSpanDstGrp("DstGrp1");
    shared_ptr<span::DstMember> dstMem1 =
        dstGrp1->addSpanDstMember("DstMem1");
    shared_ptr<span::DstSummary> dstSumm1 = dstMem1->addSpanDstSummary();
    shared_ptr<span::LocalEp> lEp1 = sess->addSpanLocalEp("localEp1");
    opflex::modb::MAC mac = opflex::modb::MAC("01:02:03:04:05:06");
    lEp1->addSpanLocalEpToEpRSrc()->setTargetL2Ep(bd->getURI().toString(), mac);
    lEp1->setName("localEp1");
    srcMem1->addSpanMemberToRefRSrc()->setTargetLocalEp(lEp1->
        getURI());
    srcGrp1->addSpanSrcMember(srcMem1->getName().get());
    srcGrp1->addSpanSrcGrpToFltGrpRSrc(fltGrp1->getName().get())
        ->setTargetFilterGrp("fltGrp1");
    dstGrp1->addSpanDstMember(dstMem1->getName().get());
    dstSumm1->setDest("192.168.20.100");
    dstSumm1->setVersion(1);

    rd->addGbpeInstContext()->setEncapId(100);
    fd3 = space->addGbpFloodDomain("fd3");
    fd3->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_HWPROXY);
    subnetsfd3 = space->addGbpSubnets("subnetsfd3");
    subnetsfd3_1 = subnetsfd3->addGbpSubnet("subnetsfd3_1");
    subnetsfd3_1->setAddress("1.1.1.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("1.1.1.128");
    fd3->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsfd3->getURI());
    eg3->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd3->getURI());
    fd4 = space->addGbpFloodDomain("fd4");
    fd4->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
    fd5 = space->addGbpFloodDomain("fd5");
    fd5->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
    subnetsfd4 = space->addGbpSubnets("subnetsfd4");
    subnetsfd4_1 = subnetsfd4->addGbpSubnet("subnetsfd4_1");
    subnetsfd4_1->setAddress("11.0.0.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("11.0.0.1");
    fd4->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsfd4->getURI());
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd4->getURI().toString());
    subnetsfd5 = space->addGbpSubnets("subnetsfd5");
    subnetsfd5_1 = subnetsfd5->addGbpSubnet("subnetsfd5_1");
    subnetsfd5_1->setAddress("12.0.0.0")
    .setPrefixLen(24)
    .setVirtualRouterIp("12.0.0.1");
    fd5->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsfd5->getURI());
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd5->getURI().toString());

    bd3 = space->addGbpBridgeDomain("bd3");
    bd3->addGbpeInstContext()->setEncapId(15003);
    bd3->addGbpeInstContext()->setClassid(10003);
    bd3->addGbpeInstContext()->setMulticastGroupIP("224.100.1.3");
    bd3->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    fd3->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd3->getURI());

    bd4 = space->addGbpBridgeDomain("bd4");
    bd4->addGbpeInstContext()->setEncapId(15004);
    bd4->addGbpeInstContext()->setClassid(10004);
    bd4->addGbpeInstContext()->setMulticastGroupIP("224.100.1.4");
    bd4->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    fd4->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd4->getURI());

    bd5 = space->addGbpBridgeDomain("bd5");
    bd5->addGbpeInstContext()->setEncapId(15005);
    bd5->addGbpeInstContext()->setClassid(10005);
    bd5->addGbpeInstContext()->setMulticastGroupIP("224.100.1.5");
    bd5->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    fd5->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd5->getURI());

    redirDstGrp1 = space->addGbpRedirectDestGroup("redirDstGrp1");
    redirDst1 = redirDstGrp1->addGbpRedirectDest("redirDst1");
    redirDst2 = redirDstGrp1->addGbpRedirectDest("redirDst2");
    redirDst3 = redirDstGrp1->addGbpRedirectDest("redirDst3");
    opflex::modb::MAC mac1("ce:3b:f9:1e:5b:94"), mac2("00:02:03:04:05:06"),
    mac3("00:03:04:05:06:07");
    redirDst1->setIp("1.1.1.3");
    redirDst1->setMac(mac1);
    redirDst1->addGbpRedirectDestToDomainRSrcBridgeDomain(
                                                    bd3->getURI().toString());
    redirDst1->addGbpRedirectDestToDomainRSrcRoutingDomain(
                                                    rd->getURI().toString());
    redirDst2->setIp("1.1.1.10");
    redirDst2->setMac(mac2);
    redirDst2->addGbpRedirectDestToDomainRSrcBridgeDomain(
                                                    bd3->getURI().toString());
    redirDst2->addGbpRedirectDestToDomainRSrcRoutingDomain(
                                                    rd->getURI().toString());
    redirDst3->setIp("1.1.1.20");
    redirDst3->setMac(mac3);
    redirDst3->addGbpRedirectDestToDomainRSrcBridgeDomain(
                                                    bd3->getURI().toString());
    redirDst3->addGbpRedirectDestToDomainRSrcRoutingDomain(
                                                    rd->getURI().toString());
    action3 = space->addGbpRedirectAction("action3");
    action3->addGbpRedirectActionToDestGrpRSrc()
        ->setTargetRedirectDestGroup(redirDstGrp1->getURI());

    con3 = space->addGbpContract("contract3");
    con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
        ->setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());
    con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
        ->addGbpRuleToActionRSrcRedirectAction(action3->getURI().toString());

    eg4 = space->addGbpEpGroup("group4");
    eg4->addGbpEpGroupToProvContractRSrc(con3->getURI().toString());
    eg4->addGbpEpGroupToConsContractRSrc(con3->getURI().toString());
    eg4->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg4->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd4->getURI());
    eg4->addGbpeInstContext()->setEncapId(0x1010);
    eg4->addGbpeInstContext()->setClassid(50004);

    eg5 = space->addGbpEpGroup("group5");
    eg5->addGbpEpGroupToProvContractRSrc(con3->getURI().toString());
    eg5->addGbpEpGroupToConsContractRSrc(con3->getURI().toString());
    eg5->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg5->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd5->getURI());
    eg5->addGbpeInstContext()->setEncapId(0x1011);
    eg5->addGbpeInstContext()->setClassid(50005);

    fd6 = space->addGbpFloodDomain("fd6");
    fd6->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_HWPROXY);
    subnetsfd6 = space->addGbpSubnets("subnetsfd6");
    subnetsfd6_1 = subnetsfd6->addGbpSubnet("subnetsfd6_1");
    subnetsfd6_1->setAddress("13.0.0.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("13.0.0.1");
    fd6->addGbpForwardingBehavioralGroupToSubnetsRSrc()
        ->setTargetSubnets(subnetsfd6->getURI());
    rd->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd6->getURI().toString());
    bd6 = space->addGbpBridgeDomain("bd6");
    bd6->addGbpeInstContext()->setEncapId(15006);
    bd6->addGbpeInstContext()->setClassid(10006);
    bd6->addGbpeInstContext()->setMulticastGroupIP("224.100.1.6");
    bd6->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    fd6->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd6->getURI());

    eg6 = space->addGbpEpGroup("group6");
    eg6->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd6->getURI());
    eg6->addGbpeInstContext()->setEncapId(0x1012);
    eg6->addGbpeInstContext()->setClassid(50006);

    eg6->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg6->addGbpEpGroupToProvContractRSrc(con3->getURI().toString());
    eg6->addGbpEpGroupToConsContractRSrc(con3->getURI().toString());

    l3ext2 = rd->addGbpL3ExternalDomain("ext2");
    l3ext2_net = l3ext2->addGbpL3ExternalNetwork("ext2_net1");
    l3ext2_net->addGbpExternalSubnet("ext2_net1_sub1")
        ->setAddress("105.0.0.0")
        .setPrefixLen(24);
    l3ext2_net2 = l3ext2->addGbpL3ExternalNetwork("ext2_net2");
    l3ext2_net2->addGbpExternalSubnet("ext2_net2_sub1")
        ->setAddress("106.0.0.0")
        .setPrefixLen(24);
    l3ext2_net->addGbpL3ExternalNetworkToProvContractRSrc(con3->getURI().toString());
    l3ext2_net->addGbpL3ExternalNetworkToConsContractRSrc(con3->getURI().toString());
    l3ext2_net->addGbpL3ExternalNetworkToProvContractRSrc(con1->getURI().toString());
    l3ext2_net2->addGbpL3ExternalNetworkToProvContractRSrc(con3->getURI().toString());
    l3ext2_net2->addGbpL3ExternalNetworkToConsContractRSrc(con3->getURI().toString());
    l3ext2_net2->addGbpL3ExternalNetworkToProvContractRSrc(con1->getURI().toString());
    l3ext2_net->addGbpeInstContext()->setClassid(40000);
    l3ext2_net2->addGbpeInstContext()->setClassid(40001);
    subnets_l3out = space->addGbpSubnets("subnets_l3out");
    subnets_l3out_sub1 = subnets_l3out->addGbpSubnet("subnets_l3out_sub1");
    subnets_l3out_sub1->setAddress("100.100.100.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("100.100.100.1");

    ext_bd1 = space->addGbpExternalL3BridgeDomain("ext_bd1");
    ext_bd1->addGbpeInstContext()->setEncapId(20000).setClassid(45000);
    ext_bd1->addGbpeInstContext()->setMulticastGroupIP("224.100.1.100");
    ext_bd1->addGbpExternalL3BridgeDomainToVrfRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    ext_node1 = space->addGbpExternalNode("ext_node1");
    ext_int1 = space->addGbpExternalInterface("ext_int1");
    ext_int1->setAddress("100.100.100.2");
    ext_int1->setEncap(100);
    ext_int1->addGbpExternalInterfaceToLocalPfxRSrc()
        ->setTargetSubnets(subnets_l3out->getURI());
    ext_int1->setIfInstT(L3IfTypeEnumT::CONST_EXTSVI);
    ext_int1->addGbpExternalInterfaceToExtl3bdRSrc()
        ->setTargetExternalL3BridgeDomain(ext_bd1->getURI());
    ext_int1->addGbpExternalInterfaceToL3outRSrc()
        ->setTargetL3ExternalDomain(l3ext2->getURI());
    static_route1 = ext_node1->addGbpStaticRoute("static_route1");
    static_route1->addGbpStaticRouteToVrfRSrc()->
        setTargetRoutingDomain(rd->getURI());
    static_route1->setAddress("105.0.0.0");
    static_route1->setPrefixLen(24);
    static_route1->addGbpStaticNextHop("100.100.100.3");
    static_route1->addGbpStaticNextHop("100.100.100.4");
    static_route2 = ext_node1->addGbpStaticRoute("static_route2");
    static_route2->addGbpStaticRouteToVrfRSrc()->
        setTargetRoutingDomain(rd->getURI());
    static_route2->setAddress("106.0.0.0");
    static_route2->setPrefixLen(24);
    static_route2->addGbpStaticNextHop("100.100.100.5");

    remote_route1 = rd->addGbpRemoteRoute("remote_route1");
    remote_route1->setAddress("105.0.0.0");
    remote_route1->setPrefixLen(24);
    remote_route1->addGbpRemoteNextHop("10.1.0.59");
    remote_route1->addGbpRemoteNextHop("10.1.0.60");
    mutator.commit();
}

} /* namespace opflexagent */
