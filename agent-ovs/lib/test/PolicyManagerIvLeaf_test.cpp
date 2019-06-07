/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for objects related to ivleaf for policy manager
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
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/L3IfTypeEnumT.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include "Policies.h"

namespace opflexagent {

using std::shared_ptr;
using std::unordered_set;
using std::lock_guard;
using std::mutex;
using boost::optional;
using opflex::modb::Mutator;
using opflex::modb::URI;

using namespace std;
using namespace modelgbp;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace boost::assign;
using namespace modelgbp::epdr;

class PolicyIvLeafFixture : public BaseFixture {
public:
    PolicyIvLeafFixture():
        BaseFixture(
            opflex::ofcore::OFConstants::OpflexElementMode::TRANSPORT_MODE) {
        shared_ptr<policy::Universe> universe =
            policy::Universe::resolve(framework).get();

        Mutator mutator(framework, "policyreg");
        space = universe->addPolicySpace("test");
        common = universe->addPolicySpace("common");
        bd = space->addGbpBridgeDomain("bd");
        rd = space->addGbpRoutingDomain("rd");
        bd->addGbpBridgeDomainToNetworkRSrc()
        ->setTargetRoutingDomain(rd->getURI());

        rd->addGbpeInstContext()->setEncapId(2983);
        bd->addGbpeInstContext()->setEncapId(2984);

        classifier7 = space->addGbpeL24Classifier("classifier7");

        action2 = space->addGbpAllowDenyAction("action2");
        action2->setAllow(1).setOrder(10);

        redirDstGrp1 = space->addGbpRedirectDestGroup("redirDstGrp1");
        redirDst1 = redirDstGrp1->addGbpRedirectDest("redirDst1");
        redirDst2 = redirDstGrp1->addGbpRedirectDest("redirDst2");
        opflex::modb::MAC mac1("00:01:02:03:04:05"), mac2("01:02:03:04:05:06");
        redirDst1->setIp("1.1.1.1");
        redirDst1->setMac(mac1);
        redirDst1->addGbpRedirectDestToDomainRSrcBridgeDomain(
                   bd->getURI().toString());
        redirDst1->addGbpRedirectDestToDomainRSrcRoutingDomain(
                   rd->getURI().toString());
        redirDst2->setIp("2.2.2.2");
        redirDst2->setMac(mac2);
        redirDst2->addGbpRedirectDestToDomainRSrcBridgeDomain(
                   bd->getURI().toString());
        redirDst2->addGbpRedirectDestToDomainRSrcRoutingDomain(
                   rd->getURI().toString());
        action3 = space->addGbpRedirectAction("action3");
        action3->addGbpRedirectActionToDestGrpRSrc()
               ->setTargetRedirectDestGroup(redirDstGrp1->getURI());
        redirDstGrp2 = space->addGbpRedirectDestGroup("redirDstGrp2");
        redirDst4 = redirDstGrp2->addGbpRedirectDest("redirDst4");
        opflex::modb::MAC mac3("02:03:04:05:06:07"), mac4("03:04:05:06:07:08");
        redirDst4->setIp("4.4.4.4");
        redirDst4->setMac(mac4);
        redirDst4->addGbpRedirectDestToDomainRSrcBridgeDomain(
                   bd->getURI().toString());
        redirDst4->addGbpRedirectDestToDomainRSrcRoutingDomain(
                   rd->getURI().toString());

        con4 = space->addGbpContract("contract4");
        con4->addGbpSubject("4_subject1")->addGbpRule("3_1_rule1")
        ->setDirection(DirectionEnumT::CONST_IN)
        .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());
        con4->addGbpSubject("4_subject1")->addGbpRule("3_1_rule1")
        ->addGbpRuleToActionRSrcRedirectAction(action3->getURI().toString());

        con5 = space->addGbpContract("contract5");
        con5->addGbpSubject("5_subject1")->addGbpRule("5_1_rule1")
        ->setDirection(DirectionEnumT::CONST_IN)
        .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_1_rule1")
        ->addGbpRuleToActionRSrcAllowDenyAction(action2->getURI().toString());

        eg4 = space->addGbpEpGroup("group4");
        eg4->addGbpeInstContext()->setEncapId(3867);
        eg4->addGbpEpGroupToProvContractRSrc(con4->getURI().toString());
	    eg4->addGbpEpGroupToNetworkRSrc()->setTargetBridgeDomain(bd->getURI());

        eg5 = space->addGbpEpGroup("group5");
        eg5->addGbpeInstContext()->setEncapId(3948);
        eg5->addGbpEpGroupToConsContractRSrc(con4->getURI().toString());
	    eg5->addGbpEpGroupToNetworkRSrc()->setTargetBridgeDomain(bd->getURI());

        rd_ext1 = space->addGbpRoutingDomain("rd_ext1");
        rd_ext1->addGbpeInstContext()->setEncapId(9999);
        l3ext2 = rd_ext1->addGbpL3ExternalDomain("ext_dom2");
        l3ext2_net = l3ext2->addGbpL3ExternalNetwork("ext_dom2_net1");
        l3ext2_net->addGbpExternalSubnet("ext_dom2_net1_sub1")
            ->setAddress("105.0.0.0")
            .setPrefixLen(24);
        l3ext2_net2 = l3ext2->addGbpL3ExternalNetwork("ext_dom2_net2");
        l3ext2_net2->addGbpExternalSubnet("ext_dom2_net2_sub1")
            ->setAddress("106.0.0.0")
            .setPrefixLen(16);
        l3ext2_net->addGbpL3ExternalNetworkToProvContractRSrc(con5->getURI().toString());
        l3ext2_net2->addGbpL3ExternalNetworkToConsContractRSrc(con5->getURI().toString());
        l3ext2_net2->addGbpeInstContext()->setClassid(1234);
        l3ext2_net->addGbpeInstContext()->setClassid(40000);
        subnets_ext = space->addGbpSubnets("subnets_ext");
        subnets_ext_sub1 = subnets_ext->addGbpSubnet("subnets_ext_sub1");
        subnets_ext_sub1->setAddress("100.100.100.0")
            .setPrefixLen(24)
            .setVirtualRouterIp("100.100.100.1");
        rd_ext1->addGbpRoutingDomainToIntSubnetsRSrc(
            subnets_ext->getURI().toString());
        ext_bd1 = space->addGbpExternalL3BridgeDomain("ext_bd1");
        ext_bd1->addGbpeInstContext()->setEncapId(1991).setClassid(1992);
        ext_bd1->addGbpExternalL3BridgeDomainToVrfRSrc()->
            setTargetRoutingDomain(rd_ext1->getURI());
        ext_node1 = space->addGbpExternalNode("ext_node1");
        ext_int1 = space->addGbpExternalInterface("ext_int1");
        ext_int1->setAddress("100.100.100.1");
        ext_int1->addGbpExternalInterfaceToLocalPfxRSrc()
            ->setTargetSubnets(subnets_ext->getURI());
        ext_int1->setEncap(100);
        ext_int1->setIfInstT(L3IfTypeEnumT::CONST_EXTSVI);
        ext_int1->addGbpExternalInterfaceToExtl3bdRSrc()->
            setTargetExternalL3BridgeDomain(ext_bd1->getURI());
        ext_int1->addGbpExternalInterfaceToL3outRSrc()->
            setTargetL3ExternalDomain(l3ext2->getURI());
        static_route1 = ext_node1->addGbpStaticRoute("static_route1");
        static_route1->addGbpStaticRouteToVrfRSrc()->
        setTargetRoutingDomain(rd_ext1->getURI());
        static_route1->setAddress("105.0.0.0");
        static_route1->setPrefixLen(16);
        static_route1->addGbpStaticNextHop("100.100.100.2");
        static_nh1 = static_route1->addGbpStaticNextHop("100.100.100.3");
        static_route1->addGbpStaticNextHop("100.100.100.4");
        remote_route1 = rd_ext1->addGbpRemoteRoute("remote_route1");
        remote_route1->setAddress("105.0.0.0");
        remote_route1->setPrefixLen(16);
        remote_nh2 = remote_route1->addGbpRemoteNextHop("10.10.10.1");
        remote_nh1 = remote_route1->addGbpRemoteNextHop("10.10.10.2");
        mutator.commit();
    }

    virtual ~PolicyIvLeafFixture() {
    }

    shared_ptr<policy::Space> space;
    shared_ptr<policy::Space> common;
    shared_ptr<RoutingDomain> rd;
    shared_ptr<BridgeDomain> bd;
    shared_ptr<Subnets> subnets_ext;
    shared_ptr<Subnet> subnets_ext_sub1;
    shared_ptr<EpGroup> eg4;
    shared_ptr<EpGroup> eg5;

    shared_ptr<L24Classifier> classifier7;

    shared_ptr<RedirectDestGroup> redirDstGrp1;
    shared_ptr<RedirectDestGroup> redirDstGrp2;
    shared_ptr<RedirectDest> redirDst1;
    shared_ptr<RedirectDest> redirDst2;
    shared_ptr<RedirectDest> redirDst3;
    shared_ptr<RedirectDest> redirDst4;
    shared_ptr<RedirectDest> redirDst5;
    shared_ptr<AllowDenyAction> action2;
    shared_ptr<RedirectAction> action3;

    shared_ptr<Contract> con4;
    shared_ptr<Contract> con5;

    shared_ptr<L3ExternalDomain> l3ext2;
    shared_ptr<L3ExternalNetwork> l3ext2_net;
    shared_ptr<L3ExternalNetwork> l3ext2_net2;
    shared_ptr<RoutingDomain> rd_ext1;
    shared_ptr<ExternalInterface> ext_int1;
    shared_ptr<ExternalL3BridgeDomain> ext_bd1;
    shared_ptr<ExternalNode> ext_node1;
    shared_ptr<StaticRoute> static_route1;
    shared_ptr<StaticNextHop> static_nh1;
    shared_ptr<RemoteRoute> remote_route1;
    shared_ptr<RemoteNextHop> remote_nh1;
    shared_ptr<RemoteNextHop> remote_nh2;
};

class MockIvLeafListener : public PolicyListener {
public:
    MockIvLeafListener(PolicyManager& pm_) : pm(pm_) {
        pm.registerListener(this);
    }

    ~MockIvLeafListener() {
        pm.unregisterListener(this);
    }

    void egDomainUpdated(const opflex::modb::URI& egURI) {
        onUpdate(egURI);
    }

    void domainUpdated(opflex::modb::class_id_t,
                       const opflex::modb::URI& domURI) {
        onUpdate(domURI);
    }

    void contractUpdated(const opflex::modb::URI& contractURI) {
        onUpdate(contractURI);
    }

    void configUpdated(const opflex::modb::URI& configURI) {
         onUpdate(configURI);
    }

    void externalInterfaceUpdated(const opflex::modb::URI& extIntURI) {
         onUpdate(extIntURI);
    }

    void localRouteUpdated(const opflex::modb::URI& routeURI) {
             onUpdate(routeURI);
    }

    bool hasNotif(const URI& uri) {
        lock_guard<mutex> guard(notifMutex);
        return notifRcvd.find(uri) != notifRcvd.end();
    }

    void clear() {
        lock_guard<mutex> guard(notifMutex);
        notifRcvd.clear();
    }

private:
    void onUpdate(const URI& uri) {
        LOG(INFO) << "NOTIF: " << uri;
        lock_guard<mutex> guard(notifMutex);
        notifRcvd.insert(uri);
    }

    PolicyManager& pm;
    PolicyManager::uri_set_t notifRcvd;
    mutex notifMutex;
};

BOOST_AUTO_TEST_SUITE(PolicyManager_IvLeaf_test)

BOOST_FIXTURE_TEST_CASE( redirect_action_rules, PolicyIvLeafFixture ) {
    using boost::asio::ip::address;
    PolicyManager& pm = agent.getPolicyManager();
    WAIT_FOR(pm.contractExists(con4->getURI()), 500);

    BOOST_CHECK(pm.contractExists(URI("invalid")) == false);

    PolicyManager::rule_list_t rules;
    WAIT_FOR_DO(rules.size() == 1, 500,
                rules.clear(); pm.getContractRules(con4->getURI(), rules));
    uint8_t hashOpt, hashParam;
    int ctr=0;
    std::vector<address> testIps;
    boost::system::error_code ec;
    testIps.insert(testIps.end(),address::from_string("1.1.1.1", ec));
    testIps.insert(testIps.end(),address::from_string("2.2.2.2", ec));
    testIps.insert(testIps.end(),address::from_string("3.3.3.3", ec));
    std::vector<opflex::modb::MAC> testMacs;
    testMacs.insert(testMacs.end(),opflex::modb::MAC("00:01:02:03:04:05"));
    testMacs.insert(testMacs.end(),opflex::modb::MAC("01:02:03:04:05:06"));
    testMacs.insert(testMacs.end(),opflex::modb::MAC("02:03:04:05:06:07"));
    PolicyManager::redir_dest_list_t redirList;
    PolicyManager::rule_list_t::const_iterator rIter = rules.begin();
    while(rIter != rules.end()) {
        BOOST_CHECK(((*rIter)->getL24Classifier()->getURI() ==
                     classifier7->getURI()) &&
                    ((*rIter)->getDirection() ==
                     DirectionEnumT::CONST_IN) &&
                    ((*rIter)->getRedirect() == true) &&
                    ((*rIter)->getRedirectDestGrpURI() ==
                     redirDstGrp1->getURI()));
        pm.getPolicyDestGroup(redirDstGrp1->getURI(),redirList,
                              hashOpt,hashParam);
        WAIT_FOR_DO(redirList.size()==2, 500, redirList.clear();
                    pm.getPolicyDestGroup(redirDstGrp1->getURI(),redirList,
                                          hashOpt,hashParam));
        for(auto it = redirList.begin(); it != redirList.end(); it++, ctr++){
            BOOST_CHECK((it->get()->getIp() == testIps[ctr]) &&
                        (it->get()->getMac() == testMacs[ctr]) &&
                        (it->get()->getBD()->getURI() == bd->getURI()) &&
                        (it->get()->getRD()->getURI() == rd->getURI()));
        }
        rIter++;
    }

    /*
     * Modify Action to RedirectDestGrp
     */
    Mutator mutator1(framework, "policyreg");
    action3->addGbpRedirectActionToDestGrpRSrc()
    ->setTargetRedirectDestGroup(redirDstGrp2->getURI());
    mutator1.commit();

    rules.clear();
    WAIT_FOR_DO((rules.size() == 1) &&
                ((*rules.begin())->getRedirectDestGrpURI()
                 == redirDstGrp2->getURI()) , 500,
                rules.clear(); pm.getContractRules(con4->getURI(), rules));

    /*
     * Modify RedirectDestGrp
     */
    Mutator mutator2(framework, "policyreg");
    opflex::modb::MAC mac3("02:03:04:05:06:07");
    redirDst3 = redirDstGrp1->addGbpRedirectDest("redirDst3");
    redirDst3->setIp("3.3.3.3");
    redirDst3->setMac(mac3);
    redirDst3->addGbpRedirectDestToDomainRSrcBridgeDomain(
                                                          bd->getURI().toString());
    redirDst3->addGbpRedirectDestToDomainRSrcRoutingDomain(
                                                           rd->getURI().toString());
    mutator2.commit();
    ctr = 0;
    redirList.clear();
    WAIT_FOR_DO((redirList.size()==3),1000,
                redirList.clear();
                pm.getPolicyDestGroup(redirDstGrp1->getURI(),redirList,
                                      hashOpt, hashParam));
    for(auto it = redirList.begin(); it != redirList.end(); it++, ctr++) {
        BOOST_CHECK((it->get()->getIp() == testIps[ctr]) &&
                    (it->get()->getMac() == testMacs[ctr]) &&
                    (it->get()->getBD()->getURI() == bd->getURI()) &&
                    (it->get()->getRD()->getURI() == rd->getURI()));
    }

}

BOOST_FIXTURE_TEST_CASE( static_route_add_mod_del, PolicyIvLeafFixture ) {
    using boost::asio::ip::address;
    PolicyManager& pm = agent.getPolicyManager();
    shared_ptr<RoutingDomain> rd_,rd2_;
    shared_ptr<modelgbp::gbpe::InstContext> rdInst_;
    address addr_, self_tep = address::from_string("10.10.10.1");
    std::list<address> nhList;
    uint8_t pfx_len;
    boost::optional<uint32_t> sclass;
    bool are_nhs_remote = false;
    std::string expectedRoute("105.0.0.0");
    WAIT_FOR_DO(nhList.size() == 3, 500, nhList.clear();
                pm.getRoute(StaticRoute::CLASS_ID, static_route1->getURI(),
                            self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                            are_nhs_remote, sclass));
    BOOST_CHECK((addr_ == address::from_string(expectedRoute))
                && (pfx_len == 16));
    std::list<address> expectedNh;
    std::vector<std::string> nhIps =
    {"100.100.100.2","100.100.100.3","100.100.100.4"};
    expectedNh.push_back(address::from_string(nhIps[0]));
    expectedNh.push_back(address::from_string(nhIps[1]));
    expectedNh.push_back(address::from_string(nhIps[2]));
    BOOST_CHECK(nhList == expectedNh);
    rd2_ = rd_;
    WAIT_FOR_DO(nhList.size() == 2, 500, nhList.clear();
                pm.getRoute(RemoteRoute::CLASS_ID, remote_route1->getURI(),
                            self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                            are_nhs_remote, sclass));
    BOOST_CHECK((addr_ == address::from_string(expectedRoute))
                && (pfx_len == 16));
    std::vector<std::string> remoteNhIps =
    {"10.10.10.1","10.10.10.2"};
    expectedNh.clear();
    expectedNh.push_back(address::from_string(remoteNhIps[0]));
    expectedNh.push_back(address::from_string(remoteNhIps[1]));
    BOOST_CHECK(nhList == expectedNh);
    boost::optional<shared_ptr<LocalRouteDiscovered>> lD;
    boost::optional<shared_ptr<LocalRoute>> localRoute;
    boost::optional<shared_ptr<modelgbp::epr::ReportedRoute>> reportedRoute;
    boost::optional<shared_ptr<modelgbp::epr::PeerRouteUniverse>> pU;
    std::string repAddress;
    sleep(1);
    pU = modelgbp::epr::PeerRouteUniverse::resolve(framework);
    localRoute = LocalRoute::resolve(framework,
                    rd_->getURI().toString(), addr_.to_string(), pfx_len);
    BOOST_CHECK(localRoute);
    reportedRoute = pU.get()->resolveEprReportedRoute(
                        rd_->getURI().toString(), addr_.to_string(), pfx_len);
    BOOST_CHECK(reportedRoute);
    BOOST_CHECK(reportedRoute.get()->getAddress().get() == expectedRoute);
    std::vector<std::shared_ptr<LocalRouteToSrtRSrc>> srt;
    WAIT_FOR_DO(srt.size()==1, 500, srt.clear();
                    localRoute.get()->resolveEpdrLocalRouteToSrtRSrc(srt));
    
    BOOST_CHECK(srt[0]->getTargetURI().get() == static_route1->getURI());
    boost::optional<std::shared_ptr<LocalRouteToPrtRSrc>>
        lrtToPrt = localRoute.get()->resolveEpdrLocalRouteToPrtRSrc();
    boost::optional<std::shared_ptr<LocalRouteToRrtRSrc>>
        lrtToRrt = localRoute.get()->resolveEpdrLocalRouteToRrtRSrc();
    BOOST_CHECK(!lrtToPrt);
    BOOST_CHECK(lrtToRrt.get()->getTargetURI() == remote_route1->getURI());
    //Check for the policy prefix
    localRoute = LocalRoute::resolve(framework,
                     rd_->getURI().toString(), addr_.to_string(), 24);
    lrtToPrt = localRoute.get()->resolveEpdrLocalRouteToPrtRSrc();
    lrtToRrt = localRoute.get()->resolveEpdrLocalRouteToRrtRSrc();
    BOOST_CHECK(lrtToPrt.get()->getTargetURI() == l3ext2_net->getURI());
    BOOST_CHECK(lrtToRrt.get()->getTargetURI() == remote_route1->getURI());
    expectedNh.clear();
    expectedNh.push_back(address::from_string(nhIps[0]));
    expectedNh.push_back(address::from_string(nhIps[1]));
    expectedNh.push_back(address::from_string(nhIps[2]));
    WAIT_FOR_DO((nhList == expectedNh), 1000, nhList.clear();
                pm.getRoute(LocalRoute::CLASS_ID,
                    localRoute.get()->getURI(),
                    self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                    are_nhs_remote, sclass));
    BOOST_CHECK(are_nhs_remote == false);
    BOOST_CHECK(sclass.get() == 40000);
    Mutator m0(framework, "policyreg");
    static_nh1->remove();
    m0.commit();
    nhList.clear();
    WAIT_FOR_DO(nhList.size() == 2, 500, nhList.clear();
                pm.getRoute(StaticRoute::CLASS_ID, static_route1->getURI(),
                            self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                            are_nhs_remote, sclass));
    static_route1->remove();
    m0.commit();
    WAIT_FOR_DO(nhList.size() == 0, 500, nhList.clear();
                pm.getRoute(StaticRoute::CLASS_ID, static_route1->getURI(),
                            self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                            are_nhs_remote, sclass));
    localRoute = LocalRoute::resolve(framework,
                     rd2_->getURI().toString(), expectedRoute, 16);
    BOOST_CHECK(localRoute);
    reportedRoute = pU.get()->resolveEprReportedRoute(
                    rd2_->getURI().toString(), expectedRoute, 16);
    BOOST_CHECK(!reportedRoute);
}

BOOST_FIXTURE_TEST_CASE( remote_route_add_mod_del, PolicyIvLeafFixture ) {
    using boost::asio::ip::address;
    PolicyManager& pm = agent.getPolicyManager();
    MockIvLeafListener lsnr(pm);
    std::string expectedRoute("106.0.0.0");
    shared_ptr<RemoteRoute> remote_route2;
    address addr_, self_tep = address::from_string("10.10.10.3");
    shared_ptr<RoutingDomain> rd_,rd2_;
    shared_ptr<modelgbp::gbpe::InstContext> rdInst_;
    std::list<address> nhList, expectedNh;
    uint8_t pfx_len;
    boost::optional<uint32_t> sclass,expected_sclass = 1234;
    bool are_nhs_remote = false;
    opflex::modb::URI lRtURI =
            opflex::modb::URIBuilder()
            .addElement("EpdrLocalRouteDiscovered")
            .addElement("EpdrLocalRoute")
            .addElement(rd_ext1->getURI().toString())
            .addElement(expectedRoute)
            .addElement("24").build();
    Mutator m0(framework, "policyreg");
    remote_route2 = rd_ext1->addGbpRemoteRoute("remote_route2");
    remote_route2->setAddress("106.0.0.0");
    remote_route2->setPrefixLen(24);
    remote_route2->addGbpRemoteNextHop("10.10.10.1");
    remote_route2->addGbpRemoteNextHop("10.10.10.2");
    m0.commit();
    std::vector<std::string> remoteNhIps =
        {"10.10.10.1","10.10.10.2", "10.10.10.4"};
    expectedNh.push_back(address::from_string(remoteNhIps[0]));
    expectedNh.push_back(address::from_string(remoteNhIps[1]));
    WAIT_FOR(lsnr.hasNotif(lRtURI),500);
    lsnr.clear();
    boost::optional<shared_ptr<LocalRoute>> localRoute;
    localRoute = LocalRoute::resolve(framework,lRtURI);
    pm.getRoute(LocalRoute::CLASS_ID,
                localRoute.get()->getURI(),
                self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                are_nhs_remote, sclass);
    BOOST_CHECK(nhList == expectedNh);
    BOOST_CHECK(sclass.get() == 1234);
    BOOST_CHECK(are_nhs_remote == true);
    expectedNh.push_back(address::from_string(remoteNhIps[2]));
    remote_route2->addGbpRemoteNextHop("10.10.10.4");
    m0.commit();
    WAIT_FOR(lsnr.hasNotif(lRtURI),500);
    lsnr.clear();
    localRoute = LocalRoute::resolve(framework, lRtURI);
    pm.getRoute(LocalRoute::CLASS_ID,
                localRoute.get()->getURI(),
                self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                are_nhs_remote, sclass);
    BOOST_CHECK(nhList == expectedNh);
    BOOST_CHECK(sclass.get() == 1234);
    BOOST_CHECK(are_nhs_remote == true);
    remote_route2->remove();
    m0.commit();
    WAIT_FOR(lsnr.hasNotif(lRtURI),500);
    localRoute = LocalRoute::resolve(framework,lRtURI);
    BOOST_CHECK(!localRoute);
}

BOOST_FIXTURE_TEST_CASE( external_subnet_add_del, PolicyIvLeafFixture ) {
    using boost::asio::ip::address;
    PolicyManager& pm = agent.getPolicyManager();
    MockIvLeafListener lsnr(pm);
    std::string expectedRoute("0.0.0.0");
    shared_ptr<RemoteRoute> remote_route3;
    address addr_, self_tep = address::from_string("10.10.10.3");
    shared_ptr<RoutingDomain> rd_,rd2_;
    shared_ptr<modelgbp::gbpe::InstContext> rdInst_;
    std::list<address> nhList, expectedNh;
    uint8_t pfx_len;
    boost::optional<uint32_t> sclass,expected_sclass = 1234;
    bool are_nhs_remote = false;
    shared_ptr<L3ExternalNetwork> l3ext2_net3;
    Mutator m0(framework, "policyreg");
    l3ext2_net3 = l3ext2->addGbpL3ExternalNetwork("ext_dom2_net3");
    l3ext2_net3->addGbpExternalSubnet("ext_dom2_net3_sub1")
        ->setAddress("0.0.0.0")
        .setPrefixLen(0);
    l3ext2_net3->addGbpL3ExternalNetworkToConsContractRSrc(con5->getURI().toString());
    l3ext2_net3->addGbpeInstContext()->setClassid(40001);
    m0.commit();
    opflex::modb::URI lRtURI =
                opflex::modb::URIBuilder()
                .addElement("EpdrLocalRouteDiscovered")
                .addElement("EpdrLocalRoute")
                .addElement(rd_ext1->getURI().toString())
                .addElement(expectedRoute)
                .addElement("0").build();
    WAIT_FOR(lsnr.hasNotif(lRtURI),500);
    lsnr.clear();
    boost::optional<shared_ptr<LocalRoute>> localRoute;
    localRoute = LocalRoute::resolve(framework,lRtURI);
    pm.getRoute(LocalRoute::CLASS_ID,
                localRoute.get()->getURI(),
                self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                are_nhs_remote, sclass);
    BOOST_CHECK(nhList == expectedNh);
    BOOST_CHECK(sclass.get() == 40001);
    BOOST_CHECK(are_nhs_remote == true);
    remote_route3 = rd_ext1->addGbpRemoteRoute("remote_route3");
    remote_route3->setAddress("107.0.0.0");
    remote_route3->setPrefixLen(24);
    remote_route3->addGbpRemoteNextHop("10.10.10.1");
    remote_route3->addGbpRemoteNextHop("10.10.10.2");
    m0.commit();
    lRtURI = opflex::modb::URIBuilder()
            .addElement("EpdrLocalRouteDiscovered")
            .addElement("EpdrLocalRoute")
            .addElement(rd_ext1->getURI().toString())
            .addElement("107.0.0.0")
            .addElement("24").build();
    WAIT_FOR(lsnr.hasNotif(lRtURI),500);
    lsnr.clear();
    std::vector<std::string> remoteNhIps =
            {"10.10.10.1","10.10.10.2"};
    expectedNh.push_back(address::from_string(remoteNhIps[0]));
    expectedNh.push_back(address::from_string(remoteNhIps[1]));
    localRoute = LocalRoute::resolve(framework,lRtURI);
    pm.getRoute(LocalRoute::CLASS_ID,
                localRoute.get()->getURI(),
                self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                are_nhs_remote, sclass);
    BOOST_CHECK(nhList == expectedNh);
    BOOST_CHECK(sclass.get() == 40001);
    BOOST_CHECK(are_nhs_remote == true);
    sclass = boost::none;
    l3ext2_net3->remove();
    m0.commit();
    WAIT_FOR(lsnr.hasNotif(lRtURI),500);
    localRoute = LocalRoute::resolve(framework,lRtURI);
    pm.getRoute(LocalRoute::CLASS_ID,
                localRoute.get()->getURI(),
                self_tep, rd_, rdInst_, addr_, pfx_len, nhList,
                are_nhs_remote, sclass);
    BOOST_CHECK(nhList == expectedNh);
    BOOST_CHECK(!sclass);
    BOOST_CHECK(are_nhs_remote == true);
}


BOOST_FIXTURE_TEST_CASE( external_network_contract_remove_add, PolicyIvLeafFixture ) {
    // Remove contract, then add it back. Expect the providers/consumers to
    // remain the same as prior to remove.
    PolicyManager& pm = agent.getPolicyManager();
    WAIT_FOR(pm.contractExists(con5->getURI()), 500);

    PolicyManager::uri_set_t egs;
    WAIT_FOR_DO(egs.size() == 1, 500,
                egs.clear(); pm.getContractProviders(con5->getURI(), egs));

    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
                egs.clear(); pm.getContractConsumers(con5->getURI(), egs));

    // Remove contract
    PolicyManager::rule_list_t rules;
    WAIT_FOR_DO(!rules.empty(), 500,
                pm.getContractRules(con5->getURI(), rules));
    Mutator m0(framework, "policyreg");
    con5->addGbpSubject("5_subject1")->remove();
    con5->remove();
    m0.commit();
    WAIT_FOR_DO(rules.empty(), 500,
                rules.clear(); pm.getContractRules(con5->getURI(), rules));
    BOOST_CHECK(pm.contractExists(con5->getURI()));

    // Add contract back
    con5 = space->addGbpContract("contract5");
    m0.commit();

    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
                egs.clear(); pm.getContractProviders(con5->getURI(), egs));
    egs.clear();
    WAIT_FOR_DO(egs.size() == 1, 500,
                egs.clear(); pm.getContractConsumers(con5->getURI(), egs));

    pm.getContractRules(con5->getURI(), rules);
    BOOST_CHECK(rules.size() == 0);
}

BOOST_FIXTURE_TEST_CASE( external_interface_add_del, PolicyIvLeafFixture ) {
    PolicyManager& pm = agent.getPolicyManager();
    shared_ptr<ExternalInterface> ext_int2;
    shared_ptr<modelgbp::gbpe::InstContext> instCtx;
    MockIvLeafListener lsnr(pm);
    PolicyManager::subnet_vector_t subnets;
    Mutator m0(framework, "policyreg");
    ext_int2 = space->addGbpExternalInterface("ext_int2");
    ext_int2->setAddress("101.101.101.0");
    ext_int2->setEncap(101);
    ext_int2->setIfInstT(L3IfTypeEnumT::CONST_EXTSVI);
    ext_int2->addGbpExternalInterfaceToExtl3bdRSrc()->
        setTargetExternalL3BridgeDomain(ext_bd1->getURI());
    ext_int2->addGbpExternalInterfaceToL3outRSrc()->
        setTargetL3ExternalDomain(l3ext2->getURI());
    m0.commit();
    boost::optional<uint32_t> bd_vnid = 1991;
    WAIT_FOR(lsnr.hasNotif(ext_int2->getURI()),500);
    lsnr.clear();
    WAIT_FOR((pm.getBDVnidForExternalInterface(ext_int2->getURI())==bd_vnid),500);
    BOOST_CHECK(pm.getSclassForExternalNet(l3ext2_net2->getURI()) ==
                boost::optional<uint32_t>(1234));
    BOOST_CHECK(pm.getSclassForExternalInterface(ext_int2->getURI()) ==
                boost::optional<uint32_t>(1992));
    pm.getSubnetsForExternalInterface(ext_int1->getURI(), subnets);
    BOOST_CHECK(subnets.size()==1);
    boost::optional<boost::asio::ip::address> routerIp =
    PolicyManager::getRouterIpForSubnet(*subnets[0]);
    BOOST_CHECK(routerIp.get().to_string() == "100.100.100.1");
    ext_int2->remove();
    m0.commit();
    WAIT_FOR(lsnr.hasNotif(ext_int2->getURI()),500);
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace opflexagent */
