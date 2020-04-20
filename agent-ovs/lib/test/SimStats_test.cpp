/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for SimStats
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/test/BaseFixture.h>
#include <opflexagent/test/MockEndpointSource.h>
#include <opflexagent/SimStats.h>


namespace opflexagent {

class SimStatsFixture : public BaseFixture {
public:
    SimStatsFixture() : BaseFixture(), epSource(&agent.getEndpointManager()), simStats(agent) {
        simStats.start();
        shared_ptr<modelgbp::policy::Universe> pUniverse =
            modelgbp::policy::Universe::resolve(framework).get();
        Mutator mutator(framework, "policyreg");
        simSpace = pUniverse->addPolicySpace("simTest");
        auto epg1 = simSpace->addGbpEpGroup("a");
        auto epg2 = simSpace->addGbpEpGroup("b");
        auto simClassifier = simSpace->addGbpeL24Classifier("classifier");
        simClassifier->setDFromPort(80);
        simClassifier->setDToPort(80);
        simContract = simSpace->addGbpContract("test");
        auto simSubj = simContract->addGbpSubject("subj");
        auto simRule = simSubj->addGbpRule("rule");
        simRule->setDirection(0);
        auto classifierTarget = simRule->addGbpRuleToClassifierRSrc("targetName");
        classifierTarget->setTargetL24Classifier(simClassifier->getURI());
        auto targetA = epg1->addGbpEpGroupToProvContractRSrc("targetA");
        targetA->setTargetContract(simContract->getURI());
        auto targetB = epg2->addGbpEpGroupToConsContractRSrc("targetB");
        targetB->setTargetContract(simContract->getURI());
        auto targetC = epg1->addGbpEpGroupToIntraContractRSrc("targetC");
        targetC->setTargetContract(simContract->getURI());

        simSG = simSpace->addGbpSecGroup("abc");
        auto sgSubj = simSG->addGbpSecGroupSubject("subj");
        auto sgRule = sgSubj->addGbpSecGroupRule("rule");
        sgRule->setDirection(0);
        sgRule->setOrder(1);
        auto sgRuleTarget = sgRule->addGbpRuleToClassifierRSrc("sgRuleTarget");
        sgRuleTarget->setTargetL24Classifier(simClassifier->getURI());
        mutator.commit();
    }

    virtual ~SimStatsFixture() {
        simStats.stop();
    }

    shared_ptr<modelgbp::policy::Space> simSpace;
    shared_ptr<Contract> simContract;
    shared_ptr<SecGroup> simSG;
    MockEndpointSource epSource;
    SimStats simStats;
};

BOOST_FIXTURE_TEST_CASE(interface_counters, SimStatsFixture ) {
    URI epgu = URI("/PolicyUniverse/PolicySpace/test/GbpEpGroup/epg/");
    Endpoint ep1("e82e883b-851d-4cc6-bedb-fb5e27530043");
    ep1.setMAC(MAC("00:00:00:00:00:01"));
    ep1.addIP("10.1.1.2");
    ep1.addIP("10.1.1.3");
    ep1.setInterfaceName("veth1");
    ep1.setEgURI(epgu);
    epSource.updateEndpoint(ep1);

    std::unordered_set<std::string> eps;
    agent.getEndpointManager().getEndpointUUIDs(eps);
    BOOST_CHECK_EQUAL(1, eps.size());

    simStats.updateInterfaceCounters();
}

static bool hasConsContracts(const shared_ptr<EpGroup>& epg, std::vector<OF_SHARED_PTR<EpGroupToConsContractRSrc> > consRefs) {
    epg->resolveGbpEpGroupToConsContractRSrc(consRefs);
    return !consRefs.empty();
}

static bool hasRules(PolicyManager& pm, const shared_ptr<Contract>& contract, list<shared_ptr<PolicyRule> >& rules) {
    pm.getContractRules(contract->getURI(), rules);
    return !rules.empty();
}

bool hasSgRules(PolicyManager& pm, const shared_ptr<SecGroup>& sg, list<shared_ptr<PolicyRule> >& rules) {
    pm.getSecGroupRules(sg->getURI(), rules);
    return !rules.empty();
}

BOOST_FIXTURE_TEST_CASE(contract_counters, SimStatsFixture ) {
    simStats.contractUpdated(simContract->getURI());
    WAIT_FOR(simSpace->resolveGbpEpGroup("b"), 500);
    auto epgB = simSpace->resolveGbpEpGroup("b");
    std::vector<OF_SHARED_PTR<EpGroupToConsContractRSrc> > consRefs;
    WAIT_FOR(hasConsContracts(epgB.get(), consRefs), 500);
    list<shared_ptr<PolicyRule> > rules;
    WAIT_FOR(hasRules(agent.getPolicyManager(), simContract, rules), 500);
    simStats.updateContractCounters();
}

BOOST_FIXTURE_TEST_CASE(sg_counters, SimStatsFixture ) {
    simStats.secGroupUpdated(simSG->getURI());
    list<shared_ptr<PolicyRule> > rules;
    WAIT_FOR(hasSgRules(agent.getPolicyManager(), simSG, rules), 500);
    simStats.updateSecGrpCounters();
}
}