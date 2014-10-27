/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for endpoint manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/modb/ObjectListener.h>
#include <boost/test/unit_test.hpp>

#include "BaseFixture.h"

namespace ovsagent {

using opflex::modb::ObjectListener;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::URIBuilder;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using boost::optional;
using boost::shared_ptr;

using namespace modelgbp;
using namespace modelgbp::epdr;
using namespace modelgbp::epr;
using namespace modelgbp::gbp;

class MockEndpointSource : public EndpointSource {
public:
    MockEndpointSource(EndpointManager* manager)
        : EndpointSource(manager) {

    }

    virtual ~MockEndpointSource() { }

    virtual void start() { }
    virtual void stop() { }
};

class EndpointFixture : public BaseFixture, public ObjectListener {
public:
    EndpointFixture()
        : BaseFixture(), 
          epSource(&agent.getEndpointManager()),
          bduri("/PolicyUniverse/PolicySpace/test/GbpBridgeDomain/bd/"),
          rduri("/PolicyUniverse/PolicySpace/test/GbpRoutingDomain/rd/") {
        Mutator mutator(framework, "policyreg");
        shared_ptr<policy::Universe> universe = 
            policy::Universe::resolve(framework).get();
        space = universe->addPolicySpace("test");
        mutator.commit();

        LocalL2Ep::registerListener(framework, this);
        LocalL3Ep::registerListener(framework, this);
        BridgeDomain::registerListener(framework, this);
        EpGroup::registerListener(framework, this);
    }

    virtual ~EndpointFixture() {
        LocalL2Ep::unregisterListener(framework, this);
        LocalL3Ep::unregisterListener(framework, this);
        BridgeDomain::unregisterListener(framework, this);
        EpGroup::unregisterListener(framework, this);
    }

    void addBd() {
        shared_ptr<BridgeDomain> bd =
            space->addGbpBridgeDomain("bd");
        bd->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rduri);
    }
    void rmBd() {
        BridgeDomain::remove(framework, "test", "bd");
    }

    void addRd() {
        shared_ptr<RoutingDomain> rd =
            space->addGbpRoutingDomain("rd");
    }
    void rmRd() {
        RoutingDomain::remove(framework, "test", "rd");
    }

    void addEpg() {
        shared_ptr<EpGroup> epg =
            space->addGbpEpGroup("epg");
        epg->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bduri);
    }
    void rmEpg() {
        RoutingDomain::remove(framework, "test", "epg");
    }

    virtual void objectUpdated(class_id_t class_id,
                               const URI& uri) {
        // Simulate policy resolution from the policy repository by
        // writing the referenced object in response to any changes

        Mutator mutator(framework, "policyreg");
            
        switch (class_id) {
        case LocalL2Ep::CLASS_ID:
            {
                optional<shared_ptr<LocalL2Ep> > obj = 
                    LocalL2Ep::resolve(framework, uri);
                if (obj) addEpg();
                else rmEpg();
            }
            break;
        case LocalL3Ep::CLASS_ID:
            break;
        case BridgeDomain::CLASS_ID:
            {
                optional<shared_ptr<BridgeDomain> > obj = 
                    BridgeDomain::resolve(framework, uri);
                if (obj) addRd();
                else rmRd();
            }
            break;
        case EpGroup::CLASS_ID:
            {
                optional<shared_ptr<EpGroup> > obj = 
                    EpGroup::resolve(framework, uri);
                if (obj) addBd();
                else rmBd();
            }
        default:
            break;
        }
        mutator.commit();
    }

    shared_ptr<policy::Space> space;
    MockEndpointSource epSource;
    URI bduri;
    URI rduri;
};

BOOST_AUTO_TEST_SUITE(EndpointManager_test)

template<typename T>
bool hasEPREntry(OFFramework& framework, const URI& uri) {
    return T::resolve(framework, uri);
}

BOOST_FIXTURE_TEST_CASE( basic, EndpointFixture ) {
    URI epgu = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/");
    Endpoint ep1("e82e883b-851d-4cc6-bedb-fb5e27530043");
    ep1.setMac(1);
    ep1.addIp("10.1.1.2");
    ep1.addIp("10.1.1.3");
    ep1.setInterfaceName("veth1");
    ep1.setEgUri(epgu);
    Endpoint ep2("72ffb982-b2d5-4ae4-91ac-0dd61daf527a");
    ep2.setMac(2);
    ep2.setInterfaceName("veth2");
    ep2.addIp("10.1.1.4");
    ep2.setEgUri(epgu);

    epSource.updateEndpoint(ep1);
    epSource.updateEndpoint(ep2);

    URI l2epr1 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(1).build();
    URI l2epr2 = URIBuilder()
        .addElement("EprL2Universe")
        .addElement("EprL2Ep")
        .addElement(bduri.toString())
        .addElement(2).build();
    URI l3epr1_2 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.2").build();
    URI l3epr1_3 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.2").build();
    URI l3epr2_4 = URIBuilder()
        .addElement("EprL3Universe")
        .addElement("EprL3Ep")
        .addElement(rduri.toString())
        .addElement("10.1.1.4").build();
                    
    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr1), 500);
    WAIT_FOR(hasEPREntry<L2Ep>(framework, l2epr2), 500);

    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr1_2), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr1_3), 500);
    WAIT_FOR(hasEPREntry<L3Ep>(framework, l3epr2_4), 500);

}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace ovsagent */
