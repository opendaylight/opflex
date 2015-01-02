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
#include <opflex/modb/Mutator.h>

#include "Policies.h"

namespace ovsagent {

using boost::shared_ptr;
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
    shared_ptr<FloodDomain> fd1;
    shared_ptr<FloodDomain> fd2;
    shared_ptr<RoutingDomain> rd;
    shared_ptr<BridgeDomain> bd;
    shared_ptr<Subnets> subnetsfd1;
    shared_ptr<Subnets> subnetsfd2;
    shared_ptr<Subnet> subnetsfd1_1;
    shared_ptr<Subnet> subnetsfd1_2;
    shared_ptr<Subnet> subnetsfd2_1;
    shared_ptr<Subnet> subnetsfd2_2;
    shared_ptr<Subnets> subnetsbd;
    shared_ptr<Subnet> subnetsbd1;
    shared_ptr<Subnets> subnetsrd;
    shared_ptr<Subnet> subnetsrd1;

    shared_ptr<EpGroup> eg1;
    shared_ptr<EpGroup> eg2;
    shared_ptr<EpGroup> eg3;

    shared_ptr<L24Classifier> classifier1;
    shared_ptr<L24Classifier> classifier2;
    shared_ptr<L24Classifier> classifier3;
    shared_ptr<L24Classifier> classifier4;

    shared_ptr<Contract> con1;
    shared_ptr<Contract> con2;

    shared_ptr<policy::Universe> universe = 
        policy::Universe::resolve(framework).get();

    Mutator mutator(framework, "policyreg");
    space = universe->addPolicySpace("test");
    fd1 = space->addGbpFloodDomain("fd");
    fd2 = space->addGbpFloodDomain("fd2");
    fd2->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
    bd = space->addGbpBridgeDomain("bd");
    rd = space->addGbpRoutingDomain("rd");
    
    fd1->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd->getURI());
    fd2->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd->getURI());
    bd->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    
    subnetsfd1 = space->addGbpSubnets("subnetsfd1");
    subnetsfd1_1 = subnetsfd1->addGbpSubnet("subnetsfd1_1");
    subnetsfd1_1->setAddress("10.0.0.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("10.0.0.128");
    subnetsfd1_2 = subnetsfd1->addGbpSubnet("subnetsfd1_2");
    subnetsfd1_2->setAddress("fd8f:69d8:c12c:ca62::")
        .setPrefixLen(64)
        .setVirtualRouterIp("fd8f:69d8:c12c:ca62::1");
    subnetsfd1->addGbpSubnetsToNetworkRSrc()
        ->setTargetFloodDomain(fd1->getURI());

    subnetsfd2 = space->addGbpSubnets("subnetsfd2");
    subnetsfd2_1 = subnetsfd2->addGbpSubnet("subnetsfd2_1");
    subnetsfd2_1->setAddress("10.0.1.0")
        .setPrefixLen(24)
        .setVirtualRouterIp("10.0.1.128");
    subnetsfd2_2 = subnetsfd2->addGbpSubnet("subnetsfd2_2");
    subnetsfd2_2->setAddress("fd8c:ad36:ceb3:601f::")
        .setPrefixLen(64)
        .setVirtualRouterIp("fd8c:ad36:ceb3:601f::1");
    subnetsfd2->addGbpSubnetsToNetworkRSrc()
        ->setTargetFloodDomain(fd2->getURI());
    
    subnetsbd = space->addGbpSubnets("subnetsbd");
    subnetsbd1 = subnetsbd->addGbpSubnet("subnetsbd1");
    subnetsbd->addGbpSubnetsToNetworkRSrc()
        ->setTargetBridgeDomain(bd->getURI());
    
    subnetsrd = space->addGbpSubnets("subnetsrd");
    subnetsrd1 = subnetsrd->addGbpSubnet("subnetsrd1");
    subnetsrd->addGbpSubnetsToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());

    // ARP
    classifier1 = space->addGbpeL24Classifier("classifier1");
    classifier1->setOrder(100)
        .setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .setEtherT(EtherTypeEnumT::CONST_ARP);

    // ICMP
    classifier2 = space->addGbpeL24Classifier("classifier2");
    classifier2->setOrder(100)
        .setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .setEtherT(EtherTypeEnumT::CONST_IPV4)
        .setProt(1);
    classifier3 = space->addGbpeL24Classifier("classifier3");
    classifier3->setOrder(100)
        .setDirection(DirectionEnumT::CONST_BIDIRECTIONAL)
        .setEtherT(EtherTypeEnumT::CONST_IPV6)
        .setProt(58);

    // HTTP
    classifier4 = space->addGbpeL24Classifier("classifier4");
    classifier4->setEtherT(EtherTypeEnumT::CONST_IPV4)
        .setProt(6)
        .setDFromPort(80)
        .setDToPort(80)
        .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE)
        .setDirection(DirectionEnumT::CONST_IN);

    // Basic ARP and ICMP
    con1 = space->addGbpContract("contract1");
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->setOrder(10)
        .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());

    // HTTP to provider, reflexive
    con2 = space->addGbpContract("contract2");
    con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier4->getURI().toString());

    eg1 = space->addGbpEpGroup("group1");
    eg1->addGbpEpGroupToNetworkRSrc()
        ->setTargetSubnets(subnetsfd1->getURI());
    eg1->addGbpeInstContext()->setEncapId(1234);
    eg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToProvContractRSrc(con2->getURI().toString());

    eg2 = space->addGbpEpGroup("group2");
    eg2->addGbpEpGroupToNetworkRSrc()
        ->setTargetSubnets(subnetsfd1->getURI());
    eg2->addGbpeInstContext()->setEncapId(3000);
    eg2->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con2->getURI().toString());

    eg3 = space->addGbpEpGroup("group3");
    eg3->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg3->addGbpEpGroupToNetworkRSrc()
        ->setTargetSubnets(subnetsfd2->getURI());
    eg3->addGbpeInstContext()->setEncapId(3456);

    mutator.commit();
}

} /* namespace ovsagent */
