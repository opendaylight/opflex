/*
 * Test suite for class ModelEndpointSource.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/ModelEndpointSource.h>
#include <opflexagent/EndpointManager.h>
#include <opflexagent/EndpointListener.h>

#include <opflexagent/test/BaseFixture.h>

#include <modelgbp/inv/IfaceSecurityEnumT.hpp>
#include <modelgbp/inv/DiscoveryModeEnumT.hpp>

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>

using opflex::modb::MAC;
using opflex::modb::URI;

class TestEpListener : public opflexagent::EndpointListener {
public:
    virtual void endpointUpdated(const std::string& uuid) {
        std::lock_guard<std::mutex> guard(mutex);
        updates.insert(uuid);
    }

    bool hasEp(const std::string& uuid) {
        std::lock_guard<std::mutex> guard(mutex);
        return updates.find(uuid) != updates.end();
    }

    std::mutex mutex;
    std::set<std::string> updates;
};

class ModelEndpointSourceFixture : public opflexagent::BaseFixture {
public:
    ModelEndpointSourceFixture()
        : epSource(&agent.getEndpointManager(), framework,
                   {"default"}) {
        agent.getEndpointManager().registerListener(&listener);
    }

    opflexagent::ModelEndpointSource epSource;
    TestEpListener listener;
};

BOOST_AUTO_TEST_SUITE(ModelEndpointSource_test)

BOOST_FIXTURE_TEST_CASE(endpoints, ModelEndpointSourceFixture) {
    using namespace modelgbp::inv;
    auto& mgr = agent.getEndpointManager();
    opflex::modb::Mutator mutator(framework, "policyreg");
    auto invu = Universe::resolve(framework);
    auto inv = invu.get()->addInvLocalEndpointInventory("default");
    auto ep1 = inv->addInvLocalInventoryEp("ep1");
    auto ep2 = inv->addInvLocalInventoryEp("ep2");
    mutator.commit();

    WAIT_FOR(listener.hasEp("ep1"), 500);
    WAIT_FOR(listener.hasEp("ep2"), 500);
    BOOST_CHECK_EQUAL("ep1", mgr.getEndpoint("ep1")->getUUID());
    BOOST_CHECK_EQUAL("ep2", mgr.getEndpoint("ep2")->getUUID());
    BOOST_CHECK(!mgr.getEndpoint("ep1")->getAccessIfaceVlan());
    BOOST_CHECK(!mgr.getEndpoint("ep1")->isDiscoveryProxyMode());
    BOOST_CHECK(!mgr.getEndpoint("ep1")->isPromiscuousMode());

    listener.updates.clear();

    ep1->setMac(MAC("aa:bb:cc:dd:ee:ff"))
        .setAccessVlan(243)
        .setEgMappingAlias("alias")
        .setDiscoveryMode(DiscoveryModeEnumT::CONST_PROXY)
        .setIfaceSecurity(IfaceSecurityEnumT::CONST_PROMISCUOUS);
    ep1->addInvLocalInventoryEpToGroupRSrc()
        ->setTargetEpGroup(URI("/epgroup1"));
    ep1->addInvLocalInventoryEpToSecGroupRSrc("/secgroup1");
    ep1->addInvLocalInventoryEpToSecGroupRSrc("/secgroup2");
    ep1->addInvIp("1.1.1.1", "default");
    ep1->addInvIp("1.1.1.2", "default");
    ep1->addInvIp("1.1.1.2", "anycast-return");
    ep1->addInvIp("1.1.1.3", "virtual")
        ->setMac(MAC("11:22:33:44:55:66"));
    ep1->addInvIp("1.1.1.4", "virtual");
    ep1->addInvInterface("veth1", "integration");
    ep1->addInvInterface("veth2", "access");
    ep1->addInvInterface("veth3", "access-uplink");
    ep1->addInvDHCPv4Config()
        ->setDomain("domain")
        .setInterfaceMTU(1500)
        .setIp("5.5.5.5")
        .setLeaseTime(5)
        .setPrefixLen(24)
        .setServerIp("6.6.6.6");
    ep1->addInvDHCPv4Config()
        ->addInvRouter("router1");
    ep1->addInvDHCPv4Config()
        ->addInvRouter("router2");
    ep1->addInvDHCPv4Config()
        ->addInvStaticRoute("192.168.0.0", 24, "192.168.0.1");
    ep1->addInvDHCPv6Config()
        ->setT1(1)
        .setT2(2)
        .setPreferredLifetime(3)
        .setValidLifetime(4)
        .addInvDNSServer("8.8.8.8");
    ep1->addInvDHCPv6Config()
        ->addInvDNSServer("8.8.8.9");
    ep1->addInvDHCPv6Config()
        ->addInvSearchListEntry("domain1");
    ep1->addInvDHCPv6Config()
        ->addInvSearchListEntry("domain1.domain2");
    ep1->addInvIpMapping("mapping1")
        ->setFloatingIp("10.10.10.10")
        .setMappedIp("9.9.9.9")
        .setNextHopIf("next-hop-iface")
        .setNextHopMac(MAC("ff:ee:dd:cc:bb:aa"))
        .addInvIpMappingToGroupRSrc()
        ->setTargetEpGroup(URI("/epgroup2"));

    mutator.commit();

    WAIT_FOR(listener.hasEp("ep1"), 500);
    {
        auto e = mgr.getEndpoint("ep1");

        BOOST_REQUIRE(e->getAccessIfaceVlan());
        BOOST_CHECK_EQUAL(243, e->getAccessIfaceVlan().get());
        BOOST_REQUIRE(e->getEgMappingAlias());
        BOOST_CHECK_EQUAL("alias", e->getEgMappingAlias().get());
        BOOST_CHECK(e->isDiscoveryProxyMode());
        BOOST_CHECK(e->isPromiscuousMode());
        BOOST_REQUIRE(e->getEgURI());
        BOOST_CHECK_EQUAL(URI("/epgroup1"), e->getEgURI().get());
        {
            std::set<URI> exp{URI("/secgroup1"), URI("/secgroup2")};
            BOOST_CHECK(exp == e->getSecurityGroups());
        }
        {
            std::unordered_set<std::string> exp{"1.1.1.1", "1.1.1.2"};
            BOOST_CHECK(exp == e->getIPs());
        }
        {
            std::unordered_set<std::string> exp{"1.1.1.2"};
            BOOST_CHECK(exp == e->getAnycastReturnIPs());
        }
        {
            opflexagent::Endpoint::virt_ip_set exp;
            exp.emplace(MAC("11:22:33:44:55:66"), "1.1.1.3");
            exp.emplace(MAC("aa:bb:cc:dd:ee:ff"), "1.1.1.4");
            BOOST_CHECK(exp == e->getVirtualIPs());
        }
        BOOST_REQUIRE(e->getInterfaceName());
        BOOST_CHECK_EQUAL("veth1", e->getInterfaceName().get());
        BOOST_REQUIRE(e->getAccessInterface());
        BOOST_CHECK_EQUAL("veth2", e->getAccessInterface().get());
        BOOST_REQUIRE(e->getAccessUplinkInterface());
        BOOST_CHECK_EQUAL("veth3", e->getAccessUplinkInterface().get());

        {
            BOOST_REQUIRE(e->getDHCPv4Config());
            auto d4 = e->getDHCPv4Config().get();
            BOOST_REQUIRE(d4.getDomain());
            BOOST_CHECK_EQUAL("domain", d4.getDomain().get());
            BOOST_REQUIRE(d4.getInterfaceMtu());
            BOOST_CHECK_EQUAL(1500, d4.getInterfaceMtu().get());
            BOOST_REQUIRE(d4.getIpAddress());
            BOOST_CHECK_EQUAL("5.5.5.5", d4.getIpAddress().get());
            BOOST_REQUIRE(d4.getLeaseTime());
            BOOST_CHECK_EQUAL(5, d4.getLeaseTime().get());
            BOOST_REQUIRE(d4.getPrefixLen());
            BOOST_CHECK_EQUAL(24, d4.getPrefixLen().get());
            BOOST_REQUIRE(d4.getServerIp());
            BOOST_CHECK_EQUAL("6.6.6.6", d4.getServerIp().get());
            BOOST_CHECK_EQUAL(2, d4.getRouters().size());
            BOOST_CHECK(std::find(std::begin(d4.getRouters()),
                                  std::end(d4.getRouters()),
                                  "router1") != std::end(d4.getRouters()));
            BOOST_CHECK(std::find(std::begin(d4.getRouters()),
                                  std::end(d4.getRouters()),
                                  "router2") != std::end(d4.getRouters()));
            BOOST_REQUIRE(d4.getStaticRoutes().size() == 1);
            BOOST_CHECK_EQUAL("192.168.0.0", d4.getStaticRoutes()[0].dest);
            BOOST_CHECK_EQUAL(24, d4.getStaticRoutes()[0].prefixLen);
            BOOST_CHECK_EQUAL("192.168.0.1", d4.getStaticRoutes()[0].nextHop);
        }
        {
            BOOST_REQUIRE(e->getDHCPv6Config());
            auto d6 = e->getDHCPv6Config().get();
            BOOST_REQUIRE(d6.getT1());
            BOOST_CHECK_EQUAL(1, d6.getT1().get());
            BOOST_REQUIRE(d6.getT2());
            BOOST_CHECK_EQUAL(2, d6.getT2().get());
            BOOST_REQUIRE(d6.getPreferredLifetime());
            BOOST_CHECK_EQUAL(3, d6.getPreferredLifetime().get());
            BOOST_REQUIRE(d6.getValidLifetime());
            BOOST_CHECK_EQUAL(4, d6.getValidLifetime().get());
            BOOST_CHECK_EQUAL(2, d6.getSearchList().size());
            BOOST_CHECK(std::find(std::begin(d6.getSearchList()),
                                  std::end(d6.getSearchList()),
                                  "domain1") != std::end(d6.getSearchList()));
            BOOST_CHECK(std::find(std::begin(d6.getSearchList()),
                                  std::end(d6.getSearchList()),
                                  "domain1.domain2") !=
                        std::end(d6.getSearchList()));
            BOOST_CHECK_EQUAL(2, d6.getDnsServers().size());
            BOOST_CHECK(std::find(std::begin(d6.getDnsServers()),
                                  std::end(d6.getDnsServers()),
                                  "8.8.8.8") != std::end(d6.getDnsServers()));
            BOOST_CHECK(std::find(std::begin(d6.getDnsServers()),
                                  std::end(d6.getDnsServers()),
                                  "8.8.8.9") != std::end(d6.getDnsServers()));
        }
        {
            opflexagent::Endpoint::IPAddressMapping ipm("mapping1");
            ipm.setFloatingIP("10.10.10.10");
            ipm.setMappedIP("9.9.9.9");
            ipm.setNextHopIf("next-hop-iface");
            ipm.setNextHopMAC(MAC("ff:ee:dd:cc:bb:aa"));
            ipm.setEgURI(URI("/eggroup2"));
            BOOST_CHECK_EQUAL(1, e->getIPAddressMappings().size());
            BOOST_CHECK(e->getIPAddressMappings().find(ipm) !=
                        e->getIPAddressMappings().end());
        }
    }

    listener.updates.clear();

    ep1->remove();
    ep2->remove();
    mutator.commit();

    WAIT_FOR(listener.hasEp("ep1"), 500);
    WAIT_FOR(listener.hasEp("ep2"), 500);
    BOOST_CHECK(!mgr.getEndpoint("ep1"));
    BOOST_CHECK(!mgr.getEndpoint("ep2"));
}

BOOST_AUTO_TEST_SUITE_END()
