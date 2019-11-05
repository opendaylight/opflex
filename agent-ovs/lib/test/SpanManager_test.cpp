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

#include <list>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <opflex/modb/Mutator.h>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include "Policies.h"
#include <modelgbp/span/Universe.hpp>
#include <opflexagent/SpanSessionState.h>
#include <opflexagent/SpanManager.h>

namespace opflexagent {

using std::shared_ptr;
using namespace modelgbp;
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

class SpanFixture : public BaseFixture {

public:
    SpanFixture() : BaseFixture(), epSource(&agent.getEndpointManager()) {
        shared_ptr<policy::Universe> pUniverse =
            policy::Universe::resolve(framework).get();
        shared_ptr<span::Universe> sUniverse =
            span::Universe::resolve(framework).get();
        Mutator mutator(framework, "policyreg");
        space = pUniverse->addPolicySpace("test");
        common = pUniverse->addPolicySpace("common");
        bd = space->addGbpBridgeDomain("bd");
        eg1 = space->addGbpEpGroup("group1");
        eg2 = space->addGbpEpGroup("group2");
        sess = sUniverse->addSpanSession("sess1");
        shared_ptr<EpGroupToSpanSessionRSrc> epg1Rsrc =
            eg1->addGbpEpGroupToSpanSessionRSrc(sess->getName().get());
        epg1Rsrc->setTargetSession(sess->getURI());
        shared_ptr<EpGroupToSpanSessionRSrc> epg2Rsrc =
            eg2->addGbpEpGroupToSpanSessionRSrc(sess->getName().get());
        epg2Rsrc->setTargetSession(sess->getURI());
        srcGrp1 = sess->addSpanSrcGrp("SrcGrp1");
        shared_ptr<span::FilterGrp> fltGrp1 = sUniverse->addSpanFilterGrp("fltGrp1");
        shared_ptr<span::FilterEntry> fltEnt1 = fltGrp1->addSpanFilterEntry("fltEnt1");
        fltEnt1->addSpanFilterEntryToFltEntryRSrc()->setTargetL24Classifier(
                "test", "filter1");
        srcMem1 = srcGrp1->addSpanSrcMember("SrcMem1");

        shared_ptr<span::DstGrp> dstGrp1 = sess->addSpanDstGrp("DstGrp1");
        shared_ptr<span::DstMember> dstMem1 =
            dstGrp1->addSpanDstMember("DstMem1");
        dstSumm1 = dstMem1->addSpanDstSummary();
        lEp1 = sess->addSpanLocalEp("localEp1");
        lEp1->setName("localEp1");
        srcMem1->addSpanMemberToRefRSrc()->setTargetLocalEp(lEp1->
            getURI());
        srcMem1->setDir('0');
        srcGrp1->addSpanSrcMember(srcMem1->getName().get());
        srcGrp1->addSpanSrcGrpToFltGrpRSrc(fltGrp1->getName().get())
            ->setTargetFilterGrp("fltGrp1");
        srcMem2 = srcGrp1->addSpanSrcMember("SrcMem2");
        srcMem2->addSpanMemberToRefRSrc()->setTargetEpGroup(eg2->getURI());
        srcMem2->setDir('1');

        dstGrp1->addSpanDstMember(dstMem1->getName().get());
        dstSumm1->setDest("192.168.20.100");
        dstSumm1->setVersion(1);

        mutator.commit();

        shared_ptr<L2Universe> l2u =
                    L2Universe::resolve(framework).get();

        Endpoint ep1("e82e883b-851d-4cc6-bedb-fb5e27530043");
        ep1.setMAC(MAC("00:00:00:00:00:01"));
        ep1.addIP("10.1.1.2");
        ep1.addIP("10.1.1.3");
        ep1.setInterfaceName("veth1");
        ep1.setEgURI(eg1->getURI());
        epSource.updateEndpoint(ep1);

        Endpoint ep2("e82e883b-851d-4cc6-bedb-fb5e27530044");
        ep2.setMAC(MAC("00:00:00:00:00:02"));
        ep2.addIP("10.1.1.4");
        ep2.addIP("10.1.1.5");
        ep2.setInterfaceName("veth2");
        ep2.setEgURI(eg2->getURI());
        epSource.updateEndpoint(ep2);

        Mutator mutatorElem(framework, "policyelement");
        l2E1 = l2u->addEprL2Ep(bd->getURI().toString(),
                ep1.getMAC().get());
        l2E1->setUuid(ep1.getUUID());
        l2E1->setGroup(eg1->getURI().toString());
        l2E1->setInterfaceName(ep1.getInterfaceName().get());

        l2e2 = l2u->addEprL2Ep(bd->getURI().toString(),
                ep2.getMAC().get());
        l2e2->setUuid(ep2.getUUID());
        l2e2->setGroup(eg2->getURI().toString());
        l2e2->setInterfaceName(ep2.getInterfaceName().get());

        mutatorElem.commit();

        Mutator mutator2(framework, "policyreg");
        lEp1->addSpanLocalEpToEpRSrc()->setTargetL2Ep(l2E1->getURI());

        mutator2.commit();

    }

    virtual ~SpanFixture() {}

    shared_ptr<policy::Space> space;
    shared_ptr<policy::Space> common;
    shared_ptr<EpGroup> eg1;
    shared_ptr<EpGroup> eg2;
    shared_ptr<BridgeDomain> bd;
    shared_ptr<span::Session> sess;
    shared_ptr<span::SrcGrp> srcGrp1;
    MockEndpointSource epSource;
    shared_ptr<span::DstSummary> dstSumm1;
    boost::optional<shared_ptr<SessionState>> pSess;
    shared_ptr<L2Ep> l2E1;
    shared_ptr<L2Ep> l2e2;
    shared_ptr<span::SrcMember> srcMem1;
    shared_ptr<span::SrcMember> srcMem2;
    shared_ptr<span::LocalEp> lEp1;
};

BOOST_AUTO_TEST_SUITE(SpanManager_test)


static bool checkSpan(boost::optional<shared_ptr<SessionState>> pSess,
                      const URI& spanUri) {
    if (!pSess)
        return false;
    if (spanUri == pSess.get()->getUri())
        return true;
    else
        return false;
}

static bool checkSrcEps(boost::optional<shared_ptr<SessionState>> pSess,
    shared_ptr<span::SrcMember> srcMem, shared_ptr<L2Ep> l2e) {
    SessionState::srcEpSet srcEps = pSess.get()->getSrcEndPointSet();
    if (srcEps.size() == 0) {
        return false;
    }
    SessionState::srcEpSet::iterator it = srcEps.begin();
    for (; it != srcEps.end(); it++) {
        bool retVal = true;
        if (srcMem->getDir().get() != (*it)->getDirection())
            retVal = false;
        if (l2e->getInterfaceName().get().compare((*it)->getPort()) != 0)
            retVal = false;
        if (retVal)
            return true;
    }

    LOG(DEBUG) << "returning false";
    return false;
}

static bool checkDst(boost::optional<shared_ptr<SessionState>> pSess,
    shared_ptr<span::DstSummary> dstSumm1) {
    unordered_map<URI, shared_ptr<DstEndPoint>> dstMap =
            pSess.get()->getDstEndPointMap();
    unordered_map<URI, shared_ptr<DstEndPoint>>::iterator it;
    it = dstMap.find(dstSumm1->getURI());
    if (it == dstMap.end())
        return false;
    if (it->second->getAddress().to_string().
            compare(dstSumm1->getDest().get()) != 0)
        return false;
    return true;
}

static bool testGetSession(shared_ptr<LocalEp> le, optional<URI> uri) {
    if (!(uri && SpanManager::getSession(le)))
        return true;
    if (uri && !SpanManager::getSession(le))
        return false;
    if (!uri && SpanManager::getSession(le))
        return false;

    if (SpanManager::getSession(le).get() != uri)
        return false;
    else
        return true;
}

BOOST_FIXTURE_TEST_CASE( verify_artifacts, SpanFixture ) {
    WAIT_FOR(checkSpan(agent.getSpanManager().getSessionState(sess->getURI()),
            sess->getURI()), 500);
    WAIT_FOR(agent.getSpanManager().getSessionState(sess->getURI())
            .get()->getSrcEndPointSet().size() == 2, 500);
    WAIT_FOR(checkSrcEps(agent.getSpanManager().getSessionState(sess->getURI()),
            srcMem1, l2E1), 500);
    WAIT_FOR(checkSrcEps(agent.getSpanManager().getSessionState(sess->getURI()),
            srcMem2, l2e2), 500);
    WAIT_FOR(checkDst(agent.getSpanManager().getSessionState(sess->getURI()),
            dstSumm1), 500);
    boost::optional<URI> uri("/SpanUniverse/SpanSession/sess1/");
    BOOST_CHECK(testGetSession(lEp1, uri));
}

BOOST_AUTO_TEST_SUITE_END()
}

