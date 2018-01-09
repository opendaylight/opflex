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
#include <opflex/modb/Mutator.h>

#include "Policies.h"

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
    mutator.commit();
}

void Policies::writeTestPolicy(opflex::ofcore::OFFramework& framework) {
    shared_ptr<policy::Space> space;
    shared_ptr<policy::Space> common;
    shared_ptr<FloodDomain> fd1;
    shared_ptr<FloodDomain> fd2;
    shared_ptr<FloodDomain> fd_ext;
    shared_ptr<RoutingDomain> rd;
    shared_ptr<L3ExternalDomain> l3ext;
    shared_ptr<L3ExternalNetwork> l3ext_net;
    shared_ptr<RoutingDomain> rd_ext;
    shared_ptr<BridgeDomain> bd;
    shared_ptr<BridgeDomain> bd2;
    shared_ptr<BridgeDomain> bd_ext;
    shared_ptr<Subnets> subnetsfd1;
    shared_ptr<Subnets> subnetsfd2;
    shared_ptr<Subnet> subnetsfd1_1;
    shared_ptr<Subnet> subnetsfd1_2;
    shared_ptr<Subnet> subnetsfd1_3;
    shared_ptr<Subnet> subnetsfd2_1;
    shared_ptr<Subnet> subnetsfd2_2;
    shared_ptr<Subnets> subnetsbd;
    shared_ptr<Subnet> subnetsbd1;
    shared_ptr<Subnets> subnetsrd;
    shared_ptr<Subnet> subnetsrd1;
    shared_ptr<Subnets> subnets_ext;

    shared_ptr<EpGroup> eg1;
    shared_ptr<EpGroup> eg2;
    shared_ptr<EpGroup> eg3;
    shared_ptr<EpGroup> eg_nat;

    shared_ptr<L24Classifier> classifier1;
    shared_ptr<L24Classifier> classifier2;
    shared_ptr<L24Classifier> classifier3;
    shared_ptr<L24Classifier> classifier4;
    shared_ptr<L24Classifier> classifier5;
    shared_ptr<L24Classifier> classifier6;

    shared_ptr<Contract> con1;
    shared_ptr<Contract> con2;

    shared_ptr<SecGroup> secGrp1;
    shared_ptr<SecGroup> secGrp2;
    shared_ptr<SecGroup> secGrp3;

    shared_ptr<policy::Universe> universe =
        policy::Universe::resolve(framework).get();

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
    bd2->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());

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

    // Basic ARP and ICMP
    con1 = space->addGbpContract("contract1");
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->setOrder(10).setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());

    // HTTP to provider, reflexive
    con2 = space->addGbpContract("contract2");
    con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
        ->setDirection(DirectionEnumT::CONST_IN)
        .addGbpRuleToClassifierRSrc(classifier4->getURI().toString());

    eg1 = space->addGbpEpGroup("group1");
    eg1->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd1->getURI());
    eg1->addGbpeInstContext()->setEncapId(0x1234);
    eg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToProvContractRSrc(con2->getURI().toString());

    eg2 = space->addGbpEpGroup("group2");
    eg2->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd1->getURI());
    eg2->addGbpeInstContext()->setEncapId(0x3000);
    eg2->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con2->getURI().toString());

    eg3 = space->addGbpEpGroup("group3");
    eg3->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg3->addGbpEpGroupToNetworkRSrc()
        ->setTargetFloodDomain(fd2->getURI());
    eg3->addGbpeInstContext()->setEncapId(0x3456);

    eg_nat = common->addGbpEpGroup("nat-epg");
    eg_nat->addGbpeInstContext()->setEncapId(0x4242);
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

    mutator.commit();
}

} /* namespace opflexagent */
