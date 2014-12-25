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
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
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

    virtual ~ModbFixture() {
    }

    DummyEpSrc epSrc;
    shared_ptr<policy::Universe> universe;
    shared_ptr<policy::Space> space;
    shared_ptr<Endpoint> ep0, ep1, ep2, ep3, ep4;
    shared_ptr<EpGroup> epg0, epg1, epg2, epg3, epg4;
    shared_ptr<FloodDomain> fd0, fd1;
    shared_ptr<RoutingDomain> rd0;
    shared_ptr<BridgeDomain> bd0;
    shared_ptr<Subnets> subnetsfd0, subnetsfd1, subnetsbd0, subnetsrd0;
    shared_ptr<Subnet> subnetsfd0_1, subnetsfd0_2, subnetsfd1_1,
        subnetsbd0_1, subnetsrd0_1;

    shared_ptr<L24Classifier> classifier0;
    shared_ptr<L24Classifier> classifier1;
    shared_ptr<L24Classifier> classifier2;
    shared_ptr<L24Classifier> classifier3;
    shared_ptr<L24Classifier> classifier4;

    shared_ptr<Contract> con1;
    shared_ptr<Contract> con2;
    shared_ptr<Contract> con3;
    string policyOwner;

protected:

    void createObjects() {
        /* create EPGs and forwarding objects */
        universe = policy::Universe::resolve(framework).get();

        Mutator mutator(framework, policyOwner);
        space = universe->addPolicySpace("tenant0");
        fd0 = space->addGbpFloodDomain("fd0");
        fd1 = space->addGbpFloodDomain("fd1");
        fd1->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
        bd0 = space->addGbpBridgeDomain("bd0");
        rd0 = space->addGbpRoutingDomain("rd0");

        fd0->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        fd1->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        bd0->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        subnetsfd0 = space->addGbpSubnets("subnetsfd0");
        subnetsfd0_1 = subnetsfd0->addGbpSubnet("subnetsfd0_1");
        subnetsfd0_1->setAddress("10.20.44.0")
            .setPrefixLen(24)
            .setVirtualRouterIp("10.20.44.1");
        subnetsfd0_2 = subnetsfd0->addGbpSubnet("subnetsfd0_2");
        subnetsfd0_2->setAddress("2001:db8::")
            .setPrefixLen(32);
        subnetsfd0->addGbpSubnetsToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());

        subnetsfd1 = space->addGbpSubnets("subnetsfd1");
        subnetsfd1_1 = subnetsfd0->addGbpSubnet("subnetsfd1_1");
        subnetsfd1_1->setAddress("10.20.45.0")
            .setPrefixLen(24)
            .setVirtualRouterIp("10.20.45.1");
        subnetsfd1->addGbpSubnetsToNetworkRSrc()
            ->setTargetFloodDomain(fd1->getURI());

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
        epg0->addGbpeInstContext()->setEncapId(0xA0A);

        epg1 = space->addGbpEpGroup("epg1");
        epg1->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsrd0->getURI());
        epg1->addGbpeInstContext()->setEncapId(0xA0B);

        epg2 = space->addGbpEpGroup("epg2");
        epg2->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
        epg2->addGbpeInstContext()->setEncapId(0xD0A);

        epg3 = space->addGbpEpGroup("epg3");
        epg3->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd1->getURI());
        epg3->addGbpeInstContext()->setEncapId(0xD0B);

        epg4 = space->addGbpEpGroup("epg4");
        epg4->addGbpeInstContext()->setEncapId(0xE0E);
        epg4->addGbpEpGroupToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        createPolicyObjects();

        mutator.commit();

        /* create endpoints */
        ep0.reset(new Endpoint("0-0-0-0"));
        ep0->setInterfaceName("port80");
        ep0->setMAC(opflex::modb::MAC("00:00:00:00:80:00"));
        ep0->addIP("10.20.44.2");
        ep0->addIP("10.20.44.3");
        ep0->addIP("2001:db8::2");
        ep0->addIP("2001:db8::3");
        ep0->setEgURI(epg0->getURI());
        epSrc.updateEndpoint(*ep0);

        ep1.reset(new Endpoint("0-0-0-1"));
        ep1->setMAC(opflex::modb::MAC("00:00:00:00:00:01"));
        ep1->setEgURI(epg1->getURI());
        epSrc.updateEndpoint(*ep1);

        ep2.reset(new Endpoint("0-0-0-2"));
        ep2->setMAC(opflex::modb::MAC("00:00:00:00:00:02"));
        ep2->addIP("10.20.44.21");
        ep2->setInterfaceName("port11");
        ep2->setEgURI(epg0->getURI());
        epSrc.updateEndpoint(*ep2);

        ep3.reset(new Endpoint("0-0-0-3"));
        ep3->setMAC(opflex::modb::MAC("00:00:00:00:00:03"));
        ep3->addIP("10.20.45.31");
        ep3->setInterfaceName("eth3");
        ep3->setEgURI(epg3->getURI());
        epSrc.updateEndpoint(*ep3);

        ep4.reset(new Endpoint("0-0-0-4"));
        ep4->setMAC(opflex::modb::MAC("00:00:00:00:00:04"));
        ep4->addIP("10.20.45.32");
        ep4->setInterfaceName("eth4");
        ep4->setEgURI(epg3->getURI());
        ep4->setPromiscuousMode(true);
        epSrc.updateEndpoint(*ep4);
    }

    void createPolicyObjects() {
        /* allow everything */
        classifier0 = space->addGbpeL24Classifier("classifier0");

        /* allow TCP to dst port 80 cons->prov */
        classifier1 = space->addGbpeL24Classifier("classifier1");
        classifier1->setOrder(100).setDirection(DirectionEnumT::CONST_IN)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setDFromPort(80);
        /* allow ARP from prov->cons */
        classifier2 = space->addGbpeL24Classifier("classifier2");
        classifier2->setOrder(200).setDirection(DirectionEnumT::CONST_OUT)
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

        /* classifiers with port ranges */
        classifier3 = space->addGbpeL24Classifier("classifier3");
        classifier3->setOrder(10).setDirection(DirectionEnumT::CONST_IN)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setDFromPort(80).setDToPort(85);

        classifier4 = space->addGbpeL24Classifier("classifier4");
        classifier4->setOrder(20).setDirection(DirectionEnumT::CONST_IN)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setSFromPort(66).setSToPort(69)
            .setDFromPort(94).setDToPort(95);
        con3 = space->addGbpContract("contract3");
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier4->getURI().toString());
        epg0->addGbpEpGroupToProvContractRSrc(con3->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con3->getURI().toString());

    }
};

#endif /* MODBFIXTURE_H_ */
