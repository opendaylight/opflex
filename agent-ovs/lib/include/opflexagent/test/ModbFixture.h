/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEXAGENT_TEST_MODBFIXTURE_H_
#define OPFLEXAGENT_TEST_MODBFIXTURE_H_

#include "BaseFixture.h"
#include <opflexagent/EndpointSource.h>
#include <opflexagent/ServiceSource.h>
#include <opflexagent/EndpointManager.h>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/l4/TcpFlagsEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>
#include <opflex/modb/Mutator.h>

#include <memory>

namespace opflexagent {

class DummyEpSrc : public EndpointSource {
public:
    DummyEpSrc(EndpointManager *manager)
        : EndpointSource(manager) { }
    virtual ~DummyEpSrc() { }
};
class DummyServiceSrc : public ServiceSource {
public:
    DummyServiceSrc(ServiceManager *manager)
        : ServiceSource(manager) { }
    virtual ~DummyServiceSrc() { }
};

class ModbFixture : public BaseFixture {
public:
    ModbFixture() : BaseFixture(),
                    epSrc(&agent.getEndpointManager()),
                    servSrc(&agent.getServiceManager()),
                    policyOwner("policyreg") {
        universe = modelgbp::policy::Universe::resolve(framework).get();
        opflex::modb::Mutator mutator(framework, policyOwner);
        space = universe->addPolicySpace("tenant0");
        mutator.commit();
    }

    virtual ~ModbFixture() {
    }

    DummyEpSrc epSrc;
    DummyServiceSrc servSrc;
    std::shared_ptr<modelgbp::policy::Universe> universe;
    std::shared_ptr<modelgbp::policy::Space> space;
    std::shared_ptr<modelgbp::platform::Config> config;
    std::shared_ptr<Endpoint> ep0, ep1, ep2, ep3, ep4;
    std::shared_ptr<modelgbp::gbp::EpGroup> epg0, epg1, epg2, epg3, epg4;
    std::shared_ptr<modelgbp::gbp::FloodDomain> fd0, fd1;
    std::shared_ptr<modelgbp::gbpe::FloodContext> fd0ctx;
    std::shared_ptr<modelgbp::gbp::RoutingDomain> rd0;
    std::shared_ptr<modelgbp::gbp::BridgeDomain> bd0, bd1;
    std::shared_ptr<modelgbp::gbp::Subnets> subnetsfd0, subnetsfd1,
        subnetsbd0, subnetsbd1;
    std::shared_ptr<modelgbp::gbp::Subnet> subnetsfd0_1, subnetsfd0_2,
        subnetsfd1_1, subnetsbd0_1, subnetsbd1_1, subnet;

    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier0;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier1;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier2;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier3;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier4;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier5;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier6;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier7;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier8;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier9;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier10;

    std::shared_ptr<modelgbp::gbp::AllowDenyAction> action1;

    std::shared_ptr<modelgbp::gbp::Contract> con1;
    std::shared_ptr<modelgbp::gbp::Contract> con2;
    std::shared_ptr<modelgbp::gbp::Contract> con3;
    std::string policyOwner;

protected:

    void createObjects() {
        using opflex::modb::Mutator;
        using namespace modelgbp;
        using namespace modelgbp::gbp;
        using namespace modelgbp::gbpe;

        /* create EPGs and forwarding objects */
        Mutator mutator(framework, policyOwner);
        config = universe->addPlatformConfig("default");
        config->setMulticastGroupIP("224.1.1.1");

        fd0 = space->addGbpFloodDomain("fd0");
        fd1 = space->addGbpFloodDomain("fd1");
        fd1->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
        bd0 = space->addGbpBridgeDomain("bd0");
        bd1 = space->addGbpBridgeDomain("bd1");
        rd0 = space->addGbpRoutingDomain("rd0");

        fd0->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        fd0ctx = fd0->addGbpeFloodContext();

        fd1->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        bd0->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());
        bd1->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        subnetsfd0 = space->addGbpSubnets("subnetsfd0");
        subnetsfd0_1 = subnetsfd0->addGbpSubnet("subnetsfd0_1");
        subnetsfd0_1->setAddress("10.20.44.1")
            .setPrefixLen(24)
            .setVirtualRouterIp("10.20.44.1");
        subnetsfd0_2 = subnetsfd0->addGbpSubnet("subnetsfd0_2");
        subnetsfd0_2->setAddress("2001:db8::")
            .setPrefixLen(32)
            .setVirtualRouterIp("2001:db8::1");
        fd0->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd0->getURI().toString());

        subnetsfd1 = space->addGbpSubnets("subnetsfd1");
        subnetsfd1_1 = subnetsfd0->addGbpSubnet("subnetsfd1_1");
        subnetsfd1_1->setAddress("10.20.45.0")
            .setPrefixLen(24)
            .setVirtualRouterIp("10.20.45.1");
        fd1->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsfd1->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd1->getURI().toString());

        subnetsbd0 = space->addGbpSubnets("subnetsbd0");
        subnetsbd0_1 = subnetsbd0->addGbpSubnet("subnetsbd0_1");
        bd0->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsbd0->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsbd0->getURI().toString());

        subnetsbd1 = space->addGbpSubnets("subnetsbd1");
        subnetsbd1_1 = subnetsbd1->addGbpSubnet("subnetsbd1_1");
        bd1->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsbd1->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsbd1->getURI().toString());

        epg0 = space->addGbpEpGroup("epg0");
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        epg0->addGbpeInstContext()->setEncapId(0xA0A);

        epg1 = space->addGbpEpGroup("epg1");
        epg1->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bd1->getURI());
        epg1->addGbpeInstContext()->setEncapId(0xA0B);

        epg2 = space->addGbpEpGroup("epg2");
        epg2->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
        epg2->addGbpeInstContext()->setEncapId(0xD0A)
            .setMulticastGroupIP("224.5.1.1");

        epg3 = space->addGbpEpGroup("epg3");
        epg3->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd1->getURI());
        epg3->addGbpeInstContext()->setEncapId(0xD0B);

        epg4 = space->addGbpEpGroup("epg4");
        epg4->addGbpeInstContext()->setEncapId(0xE0E);
        epg4->addGbpEpGroupToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        mutator.commit();

        /* create endpoints */
        ep0.reset(new Endpoint("0-0-0-0"));
        ep0->setInterfaceName("port80");
        ep0->setMAC(opflex::modb::MAC("00:00:00:00:80:00"));
        ep0->addIP("10.20.44.2");
        ep0->addIP("10.20.44.3");
        ep0->addIP("2001:db8::2");
        ep0->addIP("2001:db8::3");
        ep0->addAnycastReturnIP("10.20.44.2");
        ep0->addAnycastReturnIP("2001:db8::2");
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
        using opflex::modb::Mutator;
        using namespace modelgbp;
        using namespace modelgbp::gbp;
        using namespace modelgbp::gbpe;

        Mutator mutator(framework, policyOwner);

        // deny action
        action1 = space->addGbpAllowDenyAction("action1");

        /* blank classifier */
        classifier0 = space->addGbpeL24Classifier("classifier0");
        classifier0->setOrder(10);

        /* allow bidirectional FCoE */
        classifier5 = space->addGbpeL24Classifier("classifier5");
        classifier5->setOrder(20).setEtherT(l2::EtherTypeEnumT::CONST_FCOE);

        /* allow TCP to dst port 80 cons->prov */
        classifier1 = space->addGbpeL24Classifier("classifier1");
        classifier1->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */).setDFromPort(80);

        /* allow TCPv6 to dst port 80 cons->prov */
        classifier8 = space->addGbpeL24Classifier("classifier8");
        classifier8->setEtherT(l2::EtherTypeEnumT::CONST_IPV6)
            .setProt(6 /* TCP */).setDFromPort(80);

        /* allow ARP from prov->cons */
        classifier2 = space->addGbpeL24Classifier("classifier2");
        classifier2->setEtherT(l2::EtherTypeEnumT::CONST_ARP);

        /* allow SSH from port 22 with ACK+SYN */
        classifier6 = space->addGbpeL24Classifier("classifier6");
        classifier6->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(22)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ACK |
                         l4::TcpFlagsEnumT::CONST_SYN);

        /* allow SSH from port 22 with EST */
        classifier7 = space->addGbpeL24Classifier("classifier7");
        classifier7->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(21)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ESTABLISHED);

        /* Allow 22 reflexive */
        classifier9 = space->addGbpeL24Classifier("classifier9");
        classifier9->setOrder(10)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setDFromPort(22)
            .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE);

        con1 = space->addGbpContract("contract1");
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(100)
            .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule2")
            ->setDirection(DirectionEnumT::CONST_OUT).setOrder(200)
            .addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule3")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(300)
            .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule4")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(400)
            .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());

        con2 = space->addGbpContract("contract2");
        con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier0->getURI().toString());
        con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule2")
            ->setDirection(DirectionEnumT::CONST_BIDIRECTIONAL).setOrder(20)
            .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());

        epg0->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
        epg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());

        epg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
        epg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());

        epg2->addGbpEpGroupToIntraContractRSrc(con2->getURI().toString());
        epg3->addGbpEpGroupToIntraContractRSrc(con2->getURI().toString());

        /* classifiers with port ranges */
        classifier3 = space->addGbpeL24Classifier("classifier3");
        classifier3->setOrder(10)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setDFromPort(80).setDToPort(85);

        classifier4 = space->addGbpeL24Classifier("classifier4");
        classifier4->setOrder(20)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setSFromPort(66).setSToPort(69)
            .setDFromPort(94).setDToPort(95);
        classifier10 = space->addGbpeL24Classifier("classifier10");
        classifier10->setOrder(30)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(1 /* ICMP */)
            .setIcmpType(10).setIcmpCode(5);
        con3 = space->addGbpContract("contract3");
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
            ->setOrder(1)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier3->getURI().toString())
            ->setTargetL24Classifier(classifier3->getURI());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
            ->addGbpRuleToActionRSrc(action1->getURI().toString())
            ->setTargetAllowDenyAction(action1->getURI());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule2")
            ->setOrder(2)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier4->getURI().toString())
            ->setTargetL24Classifier(classifier4->getURI());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule3")
            ->setOrder(3)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier10->getURI().toString())
            ->setTargetL24Classifier(classifier10->getURI());
        epg0->addGbpEpGroupToProvContractRSrc(con3->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con3->getURI().toString());

        mutator.commit();
    }
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MODBFIXTURE_H_ */
