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
#include <boost/assign/list_of.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/gbpe/EncapTypeEnumT.hpp>
#include <opflex/modb/Mutator.h>

#include "BaseFixture.h"
#include "Policies.h"

namespace ovsagent {

using boost::shared_ptr;
using boost::optional;
using boost::unordered_set;
using opflex::modb::Mutator;
using opflex::modb::URI;

using namespace modelgbp;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace boost::assign;

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
                ->setOrder(15)
                .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
                ->addGbpRuleToClassifierRSrc(classifier4->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule2")
                ->setOrder(10)
                .addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule3")
                ->setOrder(25)
                .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule4")
                ->setOrder(5)
                .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
        con1->addGbpSubject("1_subject2")->addGbpRule("1_2_rule1")
                ->addGbpRuleToClassifierRSrc(classifier3->getURI().toString());

        con2 = space->addGbpContract("contract2");
        con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
              ->addGbpRuleToClassifierRSrc(classifier1->getURI().toString());

        eg1 = space->addGbpEpGroup("group1");
        eg1->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd->getURI());
        eg1->addGbpeInstContext()->setEncapValue(1234)
            .setEncapType(EncapTypeEnumT::CONST_VXLAN);
        eg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
        eg1->addGbpEpGroupToProvContractRSrc(con2->getURI().toString());

        eg2 = space->addGbpEpGroup("group2");
        eg2->addGbpeInstContext()->setEncapValue(5678)
            .setEncapType(EncapTypeEnumT::CONST_VXLAN);
        eg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
        eg2->addGbpEpGroupToConsContractRSrc(con2->getURI().toString());

        eg3 = space->addGbpEpGroup("group3");
        eg3->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());

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
    WAIT_FOR(hasUriRef(agent.getPolicyManager(), fd->getURI(), 
                       subnetsfd2->getURI()), 500);
    WAIT_FOR(hasUriRef(agent.getPolicyManager(), bd->getURI(), 
                       subnetsbd1->getURI()), 500);
    WAIT_FOR(hasUriRef(agent.getPolicyManager(), rd->getURI(), 
                       subnetsrd1->getURI()), 500);
}

static bool checkFd(PolicyManager& policyManager, 
                    const URI& egURI,
                    const URI& domainURI) {
    optional<shared_ptr<FloodDomain> > rfd = 
        policyManager.getFDForGroup(egURI);
    if (!rfd) return false;
    URI u = rfd.get()->getURI();
    return (u == domainURI);
}

BOOST_FIXTURE_TEST_CASE( domain, PolicyFixture ) {
    WAIT_FOR(checkFd(agent.getPolicyManager(),
                     eg1->getURI(), fd->getURI()), 500);
}

BOOST_FIXTURE_TEST_CASE( group, PolicyFixture ) {
    PolicyManager& pm = agent.getPolicyManager();
    WAIT_FOR(pm.groupExists(eg1->getURI()), 500);

    BOOST_CHECK(pm.groupExists(URI("bad")) == false);

    optional<uint32_t> vnid = pm.getVnidForGroup(eg1->getURI());
    BOOST_CHECK(vnid.get() == 1234);

    Mutator mutator(framework, "policyreg");
    eg1->remove();
    mutator.commit();
    WAIT_FOR(pm.groupExists(eg1->getURI()) == false, 500);
}

static bool checkContains(const PolicyManager::uri_set_t& s,
        const URI& u) {
    return s.find(u) != s.end();
}

BOOST_FIXTURE_TEST_CASE( group_contract, PolicyFixture ) {
    PolicyManager& pm = agent.getPolicyManager();
    WAIT_FOR(pm.contractExists(con1->getURI()), 500);

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 2, 500,
            egs.clear(); pm.getContractProviders(con1->getURI(), egs));
    BOOST_CHECK(checkContains(egs, eg1->getURI()));
    BOOST_CHECK(checkContains(egs, eg3->getURI()));

    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
            egs.clear(); pm.getContractConsumers(con1->getURI(), egs));
    BOOST_CHECK(checkContains(egs, eg2->getURI()));

    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
            egs.clear(); pm.getContractProviders(con2->getURI(), egs));
    BOOST_CHECK(checkContains(egs, eg1->getURI()));

    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
            egs.clear(); pm.getContractConsumers(con2->getURI(), egs));
    BOOST_CHECK(checkContains(egs, eg2->getURI()));
}

BOOST_FIXTURE_TEST_CASE( group_contract_update, PolicyFixture ) {
    /* remove eg1, interchange roles of eg2 and eg3 w.r.t con1 */
    Mutator mutator(framework, "policyreg");

    eg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString())
            ->unsetTarget();
    eg3->addGbpEpGroupToProvContractRSrc(con1->getURI().toString())
            ->unsetTarget();

    eg2->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
    eg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());

    eg1->remove();
    mutator.commit();

    PolicyManager& pm = agent.getPolicyManager();
    WAIT_FOR(pm.groupExists(eg1->getURI()) == false, 500);

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 1, 500,
            egs.clear(); pm.getContractProviders(con1->getURI(), egs));
    BOOST_CHECK(checkContains(egs, eg2->getURI()));

    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
            egs.clear(); pm.getContractConsumers(con1->getURI(), egs));
    BOOST_CHECK(checkContains(egs, eg3->getURI()));

    egs.clear();
    pm.getContractProviders(con2->getURI(), egs);
    WAIT_FOR_DO(egs.empty(), 500,
            egs.clear(); pm.getContractProviders(con2->getURI(), egs));
}

static bool checkRules(const PolicyManager::rule_list_t& lhs,
        const PolicyManager::rule_list_t& rhs) {
    PolicyManager::rule_list_t::const_iterator li = lhs.begin();
    PolicyManager::rule_list_t::const_iterator ri = rhs.begin();
    while (li != lhs.end() && ri != rhs.end() &&
           (*li)->getURI() == (*ri)->getURI()) {
        ++li;
        ++ri;
    }
    return li == lhs.end() && ri == rhs.end();
}

BOOST_FIXTURE_TEST_CASE( contract_rules, PolicyFixture ) {
    PolicyManager& pm = agent.getPolicyManager();
    WAIT_FOR(pm.contractExists(con1->getURI()), 500);

    BOOST_CHECK(pm.contractExists(URI("invalid")) == false);

    PolicyManager::rule_list_t rules;
    WAIT_FOR_DO(rules.size() == 6, 500,
            rules.clear(); pm.getContractRules(con1->getURI(), rules));
    BOOST_CHECK(
        checkRules(rules,
            list_of(classifier6)(classifier2)(classifier1)(classifier4)
                (classifier5)(classifier3)) ||
        checkRules(rules,
            list_of(classifier3)(classifier6)(classifier2)(classifier1)(classifier4)
                (classifier5)));

    /*
     *  remove classifier2 & subject2
     *  move classifier4 from 1_1_rule1 to 1_1_rule2
     */
    Mutator mutator(framework, "policyreg");
    con1->addGbpSubject("1_subject2")->remove();
    classifier2->remove();
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
        ->addGbpRuleToClassifierRSrc(classifier4->getURI().toString())
        ->unsetTarget();
    con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule2")
        ->addGbpRuleToClassifierRSrc(classifier4->getURI().toString());

    shared_ptr<Contract> con3 = space->addGbpContract("contract3");
    mutator.commit();
    WAIT_FOR(pm.contractExists(con3->getURI()) == true, 500);

    rules.clear();
    WAIT_FOR_DO(rules.size() == 4, 500,
            rules.clear(); pm.getContractRules(con1->getURI(), rules));
    BOOST_CHECK(checkRules(rules,
            list_of(classifier6)(classifier4)(classifier1)(classifier5)));
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace ovsagent */
