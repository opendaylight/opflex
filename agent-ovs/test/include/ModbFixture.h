/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef MODBFIXTURE_H_
#define MODBFIXTURE_H_

#include <boost/shared_ptr.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <opflex/modb/Mutator.h>

#include "BaseFixture.h"
#include "EndpointSource.h"
#include "EndpointManager.h"

using namespace std;
using namespace boost;
using namespace ovsagent;
using namespace modelgbp;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace opflex::modb;

class DummyEpSrc : public EndpointSource {
public:
    DummyEpSrc(EndpointManager *manager)
        : EndpointSource(manager) { }
    virtual ~DummyEpSrc() { }
    virtual void start() { }
    virtual void stop() { }
};

class ModbFixture : public BaseFixture {
public:
    ModbFixture() : BaseFixture(),
                    epSrc(&agent.getEndpointManager()),
                    policyOwner("policyreg") {
        createObjects();
    }

    ~ModbFixture() {
    }

    DummyEpSrc epSrc;
    shared_ptr<policy::Universe> universe;
    shared_ptr<policy::Space> space;
    shared_ptr<Endpoint> ep0, ep1, ep2;
    shared_ptr<EpGroup> epg0, epg1, epg2, epg3;
    shared_ptr<FloodDomain> fd0;
    shared_ptr<RoutingDomain> rd0;
    shared_ptr<BridgeDomain> bd0;
    shared_ptr<Subnets> subnetsfd0, subnetsbd0, subnetsrd0;
    shared_ptr<Subnet> subnetsfd0_1, subnetsbd0_1, subnetsrd0_1;

    shared_ptr<L24Classifier> classifier0;
    shared_ptr<L24Classifier> classifier1;
    shared_ptr<L24Classifier> classifier2;

    shared_ptr<Contract> con1;
    shared_ptr<Contract> con2;
    string policyOwner;

protected:

    void createObjects() {
        /* create EPGs and forwarding objects */
        universe = policy::Universe::resolve(framework).get();

        Mutator mutator(framework, policyOwner);
        space = universe->addPolicySpace("tenant0");
        fd0 = space->addGbpFloodDomain("fd0");
        bd0 = space->addGbpBridgeDomain("bd0");
        rd0 = space->addGbpRoutingDomain("rd0");

        fd0->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        bd0->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        subnetsfd0 = space->addGbpSubnets("subnetsfd0");
        subnetsfd0_1 = subnetsfd0->addGbpSubnet("subnetsfd0_1");
        subnetsfd0->addGbpSubnetsToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());

        subnetsbd0 = space->addGbpSubnets("subnetsbd0");
        subnetsbd0_1 = subnetsbd0->addGbpSubnet("subnetsbd0_1");
        subnetsbd0->addGbpSubnetsToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());

        subnetsrd0 = space->addGbpSubnets("subnetsrd0");
        subnetsrd0_1 = subnetsrd0->addGbpSubnet("subnetsrd0_1");
        subnetsrd0->addGbpSubnetsToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        epg0 = space->addGbpEpGroup("epg0");
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsbd0->getURI());
        epg0->addGbpeInstContext()->setVnid(0xA00A);

        epg1 = space->addGbpEpGroup("epg1");
        epg1->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsrd0->getURI());
        epg1->addGbpeInstContext()->setVnid(0xA00B);

        epg2 = space->addGbpEpGroup("epg2");
        epg2->addGbpeInstContext()->setVnid(0xD00A);

        epg3 = space->addGbpEpGroup("epg3");
        epg3->addGbpeInstContext()->setVnid(0xD00B);

        createPolicyObjects();

        mutator.commit();

        /* create endpoints */
        ep0.reset(new Endpoint("0-0-0-0"));
        ep0->setInterfaceName("port80");
        ep0->setMAC(opflex::modb::MAC("00:00:00:00:80:00"));
        ep0->addIP("10.20.44.1");
        ep0->addIP("10.20.44.2");
        ep0->setEgURI(epg0->getURI());
        epSrc.updateEndpoint(*ep0);

        ep1.reset(new Endpoint("0-0-0-1"));
        ep1->setMAC(opflex::modb::MAC("00:00:00:00:00:01"));
        ep1->setEgURI(epg1->getURI());
        epSrc.updateEndpoint(*ep1);

        ep2.reset(new Endpoint("0-0-0-2"));
        ep2->setMAC(opflex::modb::MAC("00:00:00:00:00:02"));
        ep2->addIP("10.20.44.21");
        ep2->setEgURI(epg0->getURI());
        epSrc.updateEndpoint(*ep2);
    }

    void createPolicyObjects() {
        /* allow everything */
        classifier0 = space->addGbpeL24Classifier("classifier0");

        /* allow TCP to dst port 80 cons->prov */
        classifier1 = space->addGbpeL24Classifier("classifier1");
        classifier1->setOrder(200).setDirection(DirectionEnumT::CONST_IN)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setDFromPort(80);
        /* allow ARP from prov->cons */
        classifier2 = space->addGbpeL24Classifier("classifier2");
        classifier2->setOrder(100).setDirection(DirectionEnumT::CONST_OUT)
            .setEtherT(l2::EtherTypeEnumT::CONST_ARP);

        con1 = space->addGbpContract("contract1");
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier2->getURI().toString());

        con2 = space->addGbpContract("contract2");
        con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier0->getURI().toString());

        epg0->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
        epg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());

        epg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
        epg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());

        epg2->addGbpEpGroupToProvContractRSrc(con2->getURI().toString());
        epg3->addGbpEpGroupToConsContractRSrc(con2->getURI().toString());
    }
};

#endif /* MODBFIXTURE_H_ */
