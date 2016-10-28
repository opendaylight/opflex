/*
 * Test suite for class AdvertManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <boost/asio/ip/address.hpp>

#include <netinet/icmp6.h>
#include <netinet/ip.h>

#include <modelgbp/gbp/RoutingModeEnumT.hpp>

#include "AdvertManager.h"
#include "ModbFixture.h"
#include "MockSwitchManager.h"
#include "IntFlowManager.h"
#include "arp.h"
#include "eth.h"
#include "logging.h"
#include "Packets.h"
#include "ovs-shim.h"

// OVS lib
#include <lib/util.h>
extern "C" {
#include <openvswitch/flow.h>
}

using std::string;
using boost::asio::io_service;
using boost::bind;
using boost::ref;
using boost::thread;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using boost::unordered_set;
using namespace ovsagent;
using opflex::modb::Mutator;

#define CONTAINS(x, y) (x.find(y) != x.end())

class AdvertManagerFixture : public ModbFixture {
public:
    AdvertManagerFixture()
        : ModbFixture(), ctZoneManager(idGen),
          switchManager(agent, flowExecutor, flowReader, portMapper),
          intFlowManager(agent, switchManager, idGen, ctZoneManager),
          advertManager(agent, intFlowManager) {
        createObjects();
        switchManager.start("br-int");
        conn =
            static_cast<MockSwitchConnection*>(switchManager.getConnection());
        proto = ofputil_protocol_from_ofp_version
            ((ofp_version)conn->GetProtocolVersion());

        intFlowManager.setEncapIface("br0_vxlan0");
        intFlowManager.setEncapType(IntFlowManager::ENCAP_VXLAN);
        intFlowManager.setTunnel("10.11.12.13", 4789);
        intFlowManager.setVirtualRouter(true, true, "aa:bb:cc:dd:ee:ff");

        conn->connected = true;
        advertManager.registerConnection(conn);
        advertManager.setPortMapper(&portMapper);
        advertManager.setIOService(&agent.getAgentIOService());

        portMapper.ports["br0_vxlan0"] = 2048;
        portMapper.RPortMap[2048] = "br0_vxlan0";
        portMapper.ports[ep0->getInterfaceName().get()] = 80;
        portMapper.RPortMap[80] = ep0->getInterfaceName().get();

        ep1->addIP("10.20.54.11");
        epSrc.updateEndpoint(*ep1);

        Mutator mutator(framework, policyOwner);
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
        bd1->setRoutingMode(modelgbp::gbp::RoutingModeEnumT::CONST_DISABLED);
        mutator.commit();

        WAIT_FOR(agent.getPolicyManager().getRDForGroup(epg0->getURI()), 1000);
        WAIT_FOR(countSns(epg0->getURI()) == 4, 1000);
        BOOST_CHECK_EQUAL(4, countSns(epg0->getURI()));
        WAIT_FOR(countEps() == 5, 1000);
        BOOST_CHECK_EQUAL(5, countEps());
    }

    int countSns(const opflex::modb::URI& uri) {
        PolicyManager::subnet_vector_t subnets;
        agent.getPolicyManager().getSubnetsForGroup(uri, subnets);
        return subnets.size();
    }

    int countEps() {
        size_t i = 0;
        EndpointManager& epMgr = agent.getEndpointManager();
        PolicyManager& polMgr = agent.getPolicyManager();

        PolicyManager::uri_set_t epgURIs;
        polMgr.getGroups(epgURIs);
        BOOST_FOREACH(const opflex::modb::URI& epg, epgURIs) {
            unordered_set<string> eps;
            epMgr.getEndpointsForGroup(epg, eps);
            i += eps.size();
        }
        return i;
    }

    void start() {
        agent.getAgentIOService().reset();
        intFlowManager.start();
        advertManager.start();

        ioThread.reset(new thread(bind(&io_service::run,
                                       ref(agent.getAgentIOService()))));
    }

    void stop() {
        intFlowManager.stop();
        advertManager.stop();

        if (ioThread) {
            ioThread->join();
            ioThread.reset();
        }
    }

    void testEpAdvert(AdvertManager::EndpointAdvMode mode);

    IdGenerator idGen;
    CtZoneManager ctZoneManager;
    MockFlowReader flowReader;
    MockFlowExecutor flowExecutor;
    MockPortMapper portMapper;
    MockSwitchManager switchManager;
    MockSwitchConnection* conn;
    IntFlowManager intFlowManager;
    AdvertManager advertManager;
    ofputil_protocol proto;

    boost::scoped_ptr<boost::thread> ioThread;
};

class EpAdvertFixtureGU : public AdvertManagerFixture {
public:
    EpAdvertFixtureGU()
        : AdvertManagerFixture() {
        advertManager.enableEndpointAdv(AdvertManager::EPADV_GRATUITOUS_UNICAST);
        start();
        advertManager.scheduleInitialEndpointAdv(10);
    }

    ~EpAdvertFixtureGU() {
        stop();
    }
};

class EpAdvertFixtureGB : public AdvertManagerFixture {
public:
    EpAdvertFixtureGB()
        : AdvertManagerFixture() {
        advertManager.enableEndpointAdv(AdvertManager::EPADV_GRATUITOUS_BROADCAST);
        start();
        advertManager.scheduleInitialEndpointAdv(10);
    }

    ~EpAdvertFixtureGB() {
        stop();
    }
};

class EpAdvertFixtureRR : public AdvertManagerFixture {
public:
    EpAdvertFixtureRR()
        : AdvertManagerFixture() {
        advertManager.enableEndpointAdv(AdvertManager::EPADV_ROUTER_REQUEST);
        start();
        advertManager.scheduleInitialEndpointAdv(10);
    }

    ~EpAdvertFixtureRR() {
        stop();
    }
};

class RouterAdvertFixture : public AdvertManagerFixture {
public:
    RouterAdvertFixture()
        : AdvertManagerFixture() {
        advertManager.enableRouterAdv(true);
        start();
    }

    ~RouterAdvertFixture() {
        stop();
    }
};

static void verify_epadv(ofpbuf* msg, unordered_set<string>& found,
                         AdvertManager::EndpointAdvMode mode) {
    using namespace arp;

    struct ofputil_packet_out po;
    uint64_t ofpacts_stub[1024 / 8];
    struct ofpbuf ofpact;
    ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
    ofputil_decode_packet_out(&po,
                              (ofp_header*)msg->data,
                              &ofpact);

    DpPacketP pkt;
    struct flow flow;
    dp_packet_use_const(pkt.get(), po.packet, po.packet_len);
    flow_extract(pkt.get(), &flow);

    uint16_t dl_type = ntohs(flow.dl_type);

    if (mode == AdvertManager::EPADV_GRATUITOUS_BROADCAST) {
        if (dl_type == eth::type::ARP) {
            BOOST_CHECK(0 == memcmp(flow.dl_dst.ea,
                                    packets::MAC_ADDR_BROADCAST, 6));
        } else {
            BOOST_CHECK(0 == memcmp(flow.dl_dst.ea,
                                    packets::MAC_ADDR_IPV6MULTICAST, 6));
        }
    } else {
        opflex::modb::MAC routerMac("aa:bb:cc:dd:ee:ff");
        opflex::modb::MAC dstMac(flow.dl_dst.ea);
        BOOST_CHECK_EQUAL(routerMac, dstMac);
    }

    if (dl_type == eth::type::ARP) {
        BOOST_REQUIRE_EQUAL(sizeof(struct eth::eth_header) +
                            sizeof(struct arp_hdr) +
                            2 * eth::ADDR_LEN + 2 * 4,
                            dpp_size(pkt.get()));
        uint32_t ip =
            ntohl(*(uint32_t*)((char*)dpp_l2(pkt.get()) +
                               sizeof(struct eth::eth_header) +
                               sizeof(struct arp_hdr) + eth::ADDR_LEN));
        found.insert(address_v4(ip).to_string());

        if (mode != AdvertManager::EPADV_ROUTER_REQUEST) {
            BOOST_CHECK_EQUAL(flow.nw_src, flow.nw_dst);
        }
    } else if (dl_type == eth::type::IPV6) {
        size_t l4_size = dpp_l4_size(pkt.get());
        BOOST_REQUIRE(l4_size > sizeof(struct icmp6_hdr));
        struct ip6_hdr* ip6 = (struct ip6_hdr*) dpp_l3(pkt.get());
        address_v6::bytes_type bytes;
        memcpy(bytes.data(), &ip6->ip6_src, sizeof(struct in6_addr));
        address_v6 srcIp(bytes);
        memcpy(bytes.data(), &ip6->ip6_dst, sizeof(struct in6_addr));
        address_v6 dstIp(bytes);

        found.insert(srcIp.to_string());
        struct icmp6_hdr* icmp = (struct icmp6_hdr*) dpp_l4(pkt.get());

        if (mode == AdvertManager::EPADV_ROUTER_REQUEST) {
            BOOST_CHECK_EQUAL(ND_NEIGHBOR_SOLICIT, icmp->icmp6_type);
            BOOST_CHECK(srcIp != dstIp);
        } else {
            BOOST_CHECK_EQUAL(ND_NEIGHBOR_ADVERT, icmp->icmp6_type);
            BOOST_CHECK_EQUAL(dstIp, address_v6::from_string("ff02::1"));
        }
    } else {
        LOG(ERROR) << "Incorrect dl_type: " << std::hex << dl_type;
        BOOST_FAIL("Incorrect dl_type");
    }
}

void AdvertManagerFixture::testEpAdvert(AdvertManager::EndpointAdvMode mode) {
    WAIT_FOR(conn->sentMsgs.size() == 5, 1000);
    BOOST_CHECK_EQUAL(5, conn->sentMsgs.size());
    unordered_set<string> found;
    BOOST_FOREACH(ofpbuf* msg, conn->sentMsgs) {
        verify_epadv(msg, found, mode);
    }
    BOOST_CHECK(!CONTAINS(found, "10.20.45.31")); // unknown flood
    BOOST_CHECK(!CONTAINS(found, "10.20.45.32")); // unknown flood
    BOOST_CHECK(CONTAINS(found, "10.20.44.21"));
    BOOST_CHECK(CONTAINS(found, "10.20.44.3"));
    BOOST_CHECK(CONTAINS(found, "10.20.44.2"));
    BOOST_CHECK(CONTAINS(found, "2001:db8::2"));
    BOOST_CHECK(CONTAINS(found, "2001:db8::3"));
    BOOST_CHECK(!CONTAINS(found, "10.20.54.11")); // routing disabled
}

BOOST_AUTO_TEST_SUITE(AdvertManager_test)

BOOST_FIXTURE_TEST_CASE(endpointAdvertGU, EpAdvertFixtureGU) {
    testEpAdvert(AdvertManager::EPADV_GRATUITOUS_UNICAST);
}

BOOST_FIXTURE_TEST_CASE(endpointAdvertRR, EpAdvertFixtureRR) {
    testEpAdvert(AdvertManager::EPADV_ROUTER_REQUEST);
}

BOOST_FIXTURE_TEST_CASE(endpointAdvertGB, EpAdvertFixtureGB) {
    testEpAdvert(AdvertManager::EPADV_GRATUITOUS_BROADCAST);
}

BOOST_FIXTURE_TEST_CASE(routerAdvert, RouterAdvertFixture) {
    WAIT_FOR(conn->sentMsgs.size() == 1, 1000);
    BOOST_CHECK_EQUAL(1, conn->sentMsgs.size());

    unordered_set<string> found;
    BOOST_FOREACH(ofpbuf* msg, conn->sentMsgs) {
        struct ofputil_packet_out po;
        uint64_t ofpacts_stub[1024 / 8];
        struct ofpbuf ofpact;
        ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
        ofputil_decode_packet_out(&po,
                                  (ofp_header*)msg->data,
                                  &ofpact);
        DpPacketP pkt;
        struct flow flow;
        dp_packet_use_const(pkt.get(), po.packet, po.packet_len);
        flow_extract(pkt.get(), &flow);

        BOOST_REQUIRE_EQUAL(eth::type::IPV6, ntohs(flow.dl_type));
        size_t l4_size = dpp_l4_size(pkt.get());
        BOOST_REQUIRE(l4_size > sizeof(struct icmp6_hdr));
        struct ip6_hdr* ip6 = (struct ip6_hdr*) dpp_l3(pkt.get());
        address_v6::bytes_type bytes;
        memcpy(bytes.data(), &ip6->ip6_src, sizeof(struct in6_addr));
        LOG(INFO) << address_v6(bytes).to_string();
        found.insert(address_v6(bytes).to_string());
        struct icmp6_hdr* icmp = (struct icmp6_hdr*) dpp_l4(pkt.get());
        BOOST_CHECK_EQUAL(ND_ROUTER_ADVERT, icmp->icmp6_type);
    }

    BOOST_CHECK(CONTAINS(found, "fe80::a8bb:ccff:fedd:eeff"));
}

BOOST_AUTO_TEST_SUITE_END()
