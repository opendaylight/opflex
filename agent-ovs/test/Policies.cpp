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
    shared_ptr<FloodDomain> fd;
    shared_ptr<RoutingDomain> rd;
    shared_ptr<BridgeDomain> bd;
    shared_ptr<Subnets> subnetsfd;
    shared_ptr<Subnet> subnetsfd1;
    shared_ptr<Subnet> subnetsfd2;
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
    shared_ptr<L24Classifier> classifier5;
    shared_ptr<L24Classifier> classifier6;

    shared_ptr<Contract> con1;
    shared_ptr<Contract> con2;

    shared_ptr<policy::Universe> universe = 
        policy::Universe::resolve(framework).get();

    Mutator mutator(framework, "policyreg");
    space = universe->addPolicySpace("test");
    fd = space->addGbpFloodDomain("fd");
    bd = space->addGbpBridgeDomain("bd");
    rd = space->addGbpRoutingDomain("rd");
    
    fd->addGbpFloodDomainToNetworkRSrc()
        ->setTargetBridgeDomain(bd->getURI());
    bd->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());
    
    subnetsfd = space->addGbpSubnets("subnetsfd");
    subnetsfd1 = subnetsfd->addGbpSubnet("subnetsfd1");
    subnetsfd2 = subnetsfd->addGbpSubnet("subnetsfd2");
    subnetsfd->addGbpSubnetsToNetworkRSrc()
        ->setTargetFloodDomain(fd->getURI());
    
    subnetsbd = space->addGbpSubnets("subnetsbd");
    subnetsbd1 = subnetsbd->addGbpSubnet("subnetsbd1");
    subnetsbd->addGbpSubnetsToNetworkRSrc()
        ->setTargetBridgeDomain(bd->getURI());
    
    subnetsrd = space->addGbpSubnets("subnetsrd");
    subnetsrd1 = subnetsrd->addGbpSubnet("subnetsrd1");
    subnetsrd->addGbpSubnetsToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());

    classifier1 = space->addGbpeL24Classifier("classifier1");
    classifier1->setOrder(100);
    classifier2 = space->addGbpeL24Classifier("classifier2");
    classifier3 = space->addGbpeL24Classifier("classifier3");
    classifier4 = space->addGbpeL24Classifier("classifier4");
    classifier5 = space->addGbpeL24Classifier("classifier5");
    classifier6 = space->addGbpeL24Classifier("classifier6");

    con1 = space->addGbpContract("contract1");
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->setOrder(10)
        .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier4->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule2")
        ->setOrder(15)
        .addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule3")
        ->setOrder(5)
        .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule4")
        ->setOrder(25)
        .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
    con1->addGbpSubject("1_subject2")->addGbpRule("1_2_rule1")
        ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());

    con2 = space->addGbpContract("contract2");
    con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier1->getURI().toString());

    eg1 = space->addGbpEpGroup("group1");
    eg1->addGbpEpGroupToNetworkRSrc()
        ->setTargetSubnet(subnetsfd1->getURI());
    eg1->addGbpeInstContext()->setVnid(1234);
    eg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg1->addGbpEpGroupToProvContractRSrc(con2->getURI().toString());

    eg2 = space->addGbpEpGroup("group2");
    eg2->addGbpeInstContext()->setVnid(5678);
    eg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
    eg2->addGbpEpGroupToConsContractRSrc(con2->getURI().toString());

    eg3 = space->addGbpEpGroup("group3");
    eg3->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());

    mutator.commit();
}

} /* namespace ovsagent */
