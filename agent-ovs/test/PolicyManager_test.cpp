/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for policy manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <opflex/modb/Mutator.h>

#include "BaseFixture.h"

namespace ovsagent {

using boost::shared_ptr;
using boost::optional;
using boost::unordered_set;
using opflex::modb::Mutator;
using opflex::modb::URI;

using namespace modelgbp;
using namespace modelgbp::gbp;

class PolicyFixture : public BaseFixture {
public:
    PolicyFixture() {
        shared_ptr<policy::Universe> universe = 
            policy::Universe::resolve(framework).get();

        Mutator mutator(framework, "policyreg");
        space = universe->addPolicySpace("test");
        fd = space->addGbpFloodDomain("fd");
        bd = space->addGbpBridgeDomain("bd");
        rd = space->addGbpRoutingDomain("rd");
    
        fd->addGbpFloodDomainToNetworkDomainRSrc()
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
    
        mutator.commit();
    }

    virtual ~PolicyFixture() {

    }

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
};

BOOST_AUTO_TEST_SUITE(PolicyManager_test)

static bool hasUriRef(PolicyManager& policyManager,
                      const URI& domainUri,
                      const URI& subnetUri) {
    unordered_set<URI> uris;
    policyManager.getSubnetsForDomain(domainUri, uris);
    return (uris.find(subnetUri) != uris.end());
}

BOOST_FIXTURE_TEST_CASE( subnet, PolicyFixture ) {

    WAIT_FOR(hasUriRef(agent.getPolicyManager(), fd->getURI(), 
                       subnetsfd1->getURI()), 500);
    BOOST_CHECK(hasUriRef(agent.getPolicyManager(), fd->getURI(), 
                          subnetsfd2->getURI()));
    BOOST_CHECK(hasUriRef(agent.getPolicyManager(), bd->getURI(), 
                          subnetsbd1->getURI()));
    BOOST_CHECK(hasUriRef(agent.getPolicyManager(), rd->getURI(), 
                          subnetsrd1->getURI()));
}

BOOST_FIXTURE_TEST_CASE( epg, PolicyFixture ) {

}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace ovsagent */
