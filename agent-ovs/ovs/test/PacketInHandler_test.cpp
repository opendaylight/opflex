/*
 * Test suite for class PacketInHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <netinet/ip.h>
#include <linux/icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <boost/test/unit_test.hpp>

#include "CtZoneManager.h"
#include "PacketInHandler.h"
#include <opflexagent/test/ModbFixture.h>
#include "MockSwitchManager.h"
#include "IntFlowManager.h"
#include "FlowConstants.h"
#include "udp.h"
#include "dhcp.h"
#include <opflexagent/logging.h>
#include "ovs-shim.h"

BOOST_AUTO_TEST_SUITE(PacketInHandler_test)

using boost::asio::ip::address_v4;
using std::unordered_set;
using namespace opflexagent;

class PacketInHandlerFixture : public ModbFixture {
public:
    PacketInHandlerFixture()
        : ModbFixture(), ctZoneManager(idGen),
          switchManager(agent, flowExecutor, flowReader, intPortMapper),
          intFlowManager(agent, switchManager, idGen,
                         ctZoneManager, pktInHandler, tunnelEpManager),
          pktInHandler(agent, intFlowManager),
          proto(ofputil_protocol_from_ofp_version
                ((ofp_version)intConn.GetProtocolVersion())) {
        createObjects();

        intFlowManager.setEncapIface("br0_vxlan0");
        intFlowManager.setTunnel("10.11.12.13", 4789);
        intFlowManager.setVirtualRouter(true, true, "aa:bb:cc:dd:ee:ff");
        intFlowManager.setVirtualDHCP(true, "00:22:bd:f8:19:ff");

        intPortMapper.ports[ep0->getInterfaceName().get()] = 80;
        intPortMapper.RPortMap[80] = ep0->getInterfaceName().get();
        accPortMapper.ports["access"] = 500;
        accPortMapper.RPortMap[500] = "access";
        accPortMapper.ports["accessUplink"] = 501;
        accPortMapper.RPortMap[501] = "accessUplink";
        pktInHandler.setPortMapper(&intPortMapper, &accPortMapper);
    }

    void setDhcpv4Config() {
        Endpoint::DHCPv4Config c;
        c.setIpAddress("10.20.44.2");
        c.setPrefixLen(24);
        c.addRouter("10.20.44.1");
        c.addRouter("1.2.3.4");
        c.addDnsServer("8.8.8.8");
        c.addDnsServer("4.3.2.1");
        c.setDomain("example.com");
        c.addStaticRoute("169.254.169.254", 32, "4.3.2.1");
        c.setInterfaceMtu(1234);
        ep0->setDHCPv4Config(c);
        epSrc.updateEndpoint(*ep0);
        WAIT_FOR(agent.getPolicyManager().getGroupForVnid(0xA0A), 500);
    }

    void setDhcpv6Config() {
        Endpoint::DHCPv6Config c;
        c.addDnsServer("2001:4860:4860::8888");
        c.addDnsServer("2001:4860:4860::8844");
        c.addSearchListEntry("test1.example.com");
        c.addSearchListEntry("example.com");
        ep0->setDHCPv6Config(c);
        epSrc.updateEndpoint(*ep0);
        WAIT_FOR(agent.getPolicyManager().getGroupForVnid(0xA0A), 500);
    }

    void testDhcpv4Discover(MockSwitchConnection& tconn);
    void testIcmpv4Error(MockSwitchConnection& tconn);
    void testIcmpEcho(bool v4);

    IdGenerator idGen;
    CtZoneManager ctZoneManager;
    MockSwitchConnection intConn;
    MockSwitchConnection accConn;
    MockFlowReader flowReader;
    MockFlowExecutor flowExecutor;
    MockPortMapper intPortMapper;
    MockPortMapper accPortMapper;
    MockSwitchManager switchManager;
    IntFlowManager intFlowManager;
    PacketInHandler pktInHandler;
    ofputil_protocol proto;
};

static const uint8_t pkt_dhcpv4_discover[] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x08, 0x00, 0x45, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x40, 0x11,
     0x79, 0xae, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x44,
     0x00, 0x43, 0x01, 0x2c, 0x5a, 0xa7, 0x01, 0x01, 0x06, 0x00, 0xe0, 0xe2,
     0x52, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x63, 0x82, 0x53, 0x63, 0x35, 0x01, 0x01, 0x3d, 0x07, 0x01,
     0xfa, 0x16, 0x3e, 0x63, 0x13, 0x8b, 0x39, 0x02, 0x02, 0x40, 0x37, 0x09,
     0x01, 0x03, 0x06, 0x0c, 0x0f, 0x1a, 0x1c, 0x2a, 0x79, 0x3c, 0x0c, 0x75,
     0x64, 0x68, 0x63, 0x70, 0x20, 0x31, 0x2e, 0x32, 0x30, 0x2e, 0x31, 0x0c,
     0x08, 0x77, 0x65, 0x62, 0x2d, 0x76, 0x6d, 0x2d, 0x31, 0xff};

static const uint8_t pkt_dhcpv4_request[] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x08, 0x00, 0x45, 0x00, 0x01, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x40, 0x11,
     0x79, 0xa2, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x44,
     0x00, 0x43, 0x01, 0x38, 0x6f, 0x30, 0x01, 0x01, 0x06, 0x00, 0xe0, 0xe2,
     0x52, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x63, 0x82, 0x53, 0x63, 0x35, 0x01, 0x03, 0x3d, 0x07, 0x01,
     0xfa, 0x16, 0x3e, 0x63, 0x13, 0x8b, 0x32, 0x04, 0x0a, 0x14, 0x2c, 0x02,
     0x36, 0x04, 0x09, 0xfe, 0x20, 0x20, 0x39, 0x02, 0x02, 0x40, 0x37, 0x09,
     0x01, 0x03, 0x06, 0x0c, 0x0f, 0x1a, 0x1c, 0x2a, 0x79, 0x3c, 0x0c, 0x75,
     0x64, 0x68, 0x63, 0x70, 0x20, 0x31, 0x2e, 0x32, 0x30, 0x2e, 0x31, 0x0c,
     0x08, 0x77, 0x65, 0x62, 0x2d, 0x76, 0x6d, 0x2d, 0x31, 0xff};

static const uint8_t opt_subnet_mask[] =
    {opflexagent::dhcp::option::SUBNET_MASK,
     4, 0xff, 0xff, 0xff, 0x00};

static const uint8_t opt_router[] =
    {opflexagent::dhcp::option::ROUTER,
     8, 10, 20, 44, 1, 1, 2, 3, 4};

static const uint8_t opt_dns[] =
    {opflexagent::dhcp::option::DNS,
     8, 8, 8, 8, 8, 4, 3, 2, 1};

static const uint8_t opt_domain[] =
    {opflexagent::dhcp::option::DOMAIN_NAME,
     11, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'};

static const uint8_t opt_lease_time[] =
    {opflexagent::dhcp::option::LEASE_TIME,
     4, 0x00, 0x01, 0x51, 0x80};

static const uint8_t opt_server_id[] =
    {opflexagent::dhcp::option::SERVER_IDENTIFIER,
     4, 169, 254, 32, 32};

static const uint8_t opt_route[] =
    {opflexagent::dhcp::option::CLASSLESS_STATIC_ROUTE,
     9, 32, 169, 254, 169, 254, 4, 3, 2, 1};

static const uint8_t opt_iface_mtu[] =
    {opflexagent::dhcp::option::INTERFACE_MTU, 2, 0x4, 0xd2};

static const uint8_t pkt_dhcpv6_solicit[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x40, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x40,
     0x97, 0xd7, 0x01, 0x33, 0xb9, 0xa0, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27, 0x00, 0x1f,
     0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x0c, 0x00, 0x00,
     0x80, 0x00, 0x00, 0x00, 0x0e, 0x10, 0x00, 0x00, 0x15, 0x18};

static const uint8_t pkt_dhcpv6_solicit_rapid[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x44, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x44,
     0xa4, 0x9b, 0x01, 0x13, 0xac, 0xe6, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27, 0x00, 0x1f,
     0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x03,
     0x00, 0x0c, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0e, 0x10, 0x00, 0x00,
     0x15, 0x18};

static const uint8_t pkt_dhcpv6_request[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x86, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x86,
     0x4b, 0x6c, 0x03, 0x5b, 0x5c, 0x6c, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x02, 0x00, 0x0a, 0x00, 0x03, 0x00, 0x01, 0x00, 0x22, 0xbd, 0xf8,
     0x19, 0xff, 0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27,
     0x00, 0x1f, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x44,
     0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0e, 0x10, 0x00, 0x00, 0x15, 0x18,
     0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x1c, 0x20,
     0x00, 0x00, 0x1d, 0x4c, 0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
     0x00, 0x00, 0x1c, 0x20, 0x00, 0x00, 0x1d, 0x4c};

static const uint8_t pkt_dhcpv6_request_tmp[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x7e,
     0x6d, 0xbc, 0x03, 0x77, 0x5d, 0x3f, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x02, 0x00, 0x0a, 0x00, 0x03, 0x00, 0x01, 0x00, 0x22, 0xbd, 0xf8,
     0x19, 0xff, 0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27,
     0x00, 0x1f, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x3c,
     0x00, 0x00, 0x80, 0x00, 0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
     0x00, 0x00, 0x1c, 0x20, 0x00, 0x00, 0x1d, 0x4c, 0x00, 0x05, 0x00, 0x18,
     0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1c, 0x20, 0x00, 0x00, 0x1d, 0x4c};

static const uint8_t pkt_dhcpv6_confirm[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x78, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x78,
     0x75, 0x60, 0x04, 0xd0, 0x9f, 0x49, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27, 0x00, 0x1f,
     0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x44, 0x00, 0x00,
     0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
     0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t pkt_dhcpv6_renew[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x86, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x86,
     0x77, 0xed, 0x05, 0xe5, 0x2d, 0x61, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x02, 0x00, 0x0a, 0x00, 0x03, 0x00, 0x01, 0x00, 0x22, 0xbd, 0xf8,
     0x19, 0xff, 0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27,
     0x00, 0x1f, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x44,
     0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0e, 0x10, 0x00, 0x00, 0x15, 0x18,
     0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x1c, 0x20,
     0x00, 0x00, 0x1d, 0x4c, 0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
     0x00, 0x00, 0x1c, 0x20, 0x00, 0x00, 0x1d, 0x4c};

static const uint8_t pkt_dhcpv6_info_req[] =
    {0x33, 0x33, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x86, 0xdd, 0x60, 0x00, 0x00, 0x00, 0x00, 0x30, 0x11, 0x01, 0xfe, 0x80,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xff, 0xfe, 0x00,
     0x80, 0x00, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x22, 0x02, 0x23, 0x00, 0x30,
     0xb4, 0x95, 0x0b, 0xef, 0x35, 0x7e, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01,
     0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
     0x00, 0x06, 0x00, 0x08, 0x00, 0x17, 0x00, 0x18, 0x00, 0x27, 0x00, 0x1f,
     0x00, 0x08, 0x00, 0x02, 0x00, 0x00};

static const uint8_t opt6_server_id[] =
    {0, 2, 0, 10, 0, 3, 0, 1, 0x00, 0x22, 0xbd, 0xf8, 0x19, 0xff};

static const uint8_t opt6_client_id[] =
    {0x00, 0x01, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x1c, 0x6b, 0xe9, 0xb5,
     0x00, 0x00, 0x00, 0x00, 0x80, 0x00};

static const uint8_t opt6_ia_na[] =
    {0x00, 0x03, 0x00, 0x44, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0e, 0x10,
     0x00, 0x00, 0x15, 0x18};

static const uint8_t opt6_ia_ta[] =
    {0x00, 0x04, 0x00, 0x3c, 0x00, 0x00, 0x80, 0x00};

static const uint8_t iaaddr1[] =
    {0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x1c, 0x20,
     0x00, 0x00, 0x1d, 0x4c};
static const uint8_t iaaddr2[] =
    {0x00, 0x05, 0x00, 0x18, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1c, 0x20,
     0x00, 0x00, 0x1d, 0x4c};

static const uint8_t opt6_dns[] =
    {0x00, 0x17, 0x00, 0x20, 0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x20, 0x01, 0x48, 0x60,
     0x48, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x44};

static const uint8_t opt6_search_list[] =
    {0x00, 0x18, 0x00, 0x20, 0x05, 0x74, 0x65, 0x73, 0x74, 0x31, 0x07, 0x65,
     0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x07,
     0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00};

static const uint8_t pkt_icmp4_ttl_exc[] =
    {0xfa, 0x16, 0x3e, 0x35, 0x19, 0x7f, 0x00, 0x22, 0xbd, 0xf8, 0x19, 0xff,
     0x08, 0x00, 0x45, 0x00, 0x00, 0x38, 0xe0, 0x52, 0x00, 0x00, 0xfd, 0x01,
     0xd6, 0x02, 0x02, 0x69, 0x02, 0x01, 0x01, 0x01, 0x02, 0x05, 0x0b, 0x00,
     0xb6, 0xf7, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x3c, 0x52, 0xe0,
     0x00, 0x00, 0x01, 0x11, 0x9a, 0x18, 0x67, 0x67, 0x01, 0x05, 0xac, 0x1c,
     0xb8, 0x30, 0x8c, 0x98, 0x82, 0x9e, 0x00, 0x28, 0x2e, 0xa9};

static const uint8_t pkt_icmp4_echo_req[] =
    "\x00\x00\x00\x10\x00\x10\x32\x59\x3a\xce\x77\x36\x08\x00\x45\x00" \
    "\x00\x54\x55\x25\x40\x00\x40\x01\x6d\x14\x0a\x05\x64\x64\x0a\x05" \
    "\x00\x02\x08\x00\x5d\x29\x97\x5a\x00\x01\xd3\xd3\x85\x5a\x00\x00" \
    "\x00\x00\xe5\x79\x06\x00\x00\x00\x00\x00\x10\x11\x12\x13\x14\x15" \
    "\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25" \
    "\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35" \
    "\x36\x37";

static const uint8_t pkt_icmp4_echo_rep[] =
    "\x32\x59\x3a\xce\x77\x36\x00\x00\x00\x10\x00\x10\x08\x00\x45\x00" \
    "\x00\x54\x55\x25\x40\x00\x40\x01\x6d\x14\x0a\x05\x00\x02\x0a\x05" \
    "\x64\x64\x00\x00\x65\x29\x97\x5a\x00\x01\xd3\xd3\x85\x5a\x00\x00" \
    "\x00\x00\xe5\x79\x06\x00\x00\x00\x00\x00\x10\x11\x12\x13\x14\x15" \
    "\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25" \
    "\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35" \
    "\x36\x37";

static const uint8_t pkt_icmp6_echo_req[] =
    "\x00\x00\x00\x10\x00\x10\x32\x59\x3a\xce\x77\x36\x86\xdd\x60\x09" \
    "\x28\x1b\x00\x40\x3a\x40\xfd\xd6\x6e\x3c\xd8\x91\xff\x49\x00\x00" \
    "\x00\x00\x00\x00\x01\x00\xfd\xd6\x6e\x3c\xd8\x91\xff\x49\x00\x00" \
    "\x00\x00\x00\x00\x00\x03\x80\x00\xa3\xd9\x97\x6d\x00\x01\xdc\xd3" \
    "\x85\x5a\x00\x00\x00\x00\x99\x5a\x01\x00\x00\x00\x00\x00\x10\x11" \
    "\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21" \
    "\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31" \
    "\x32\x33\x34\x35\x36\x37";

static const uint8_t pkt_icmp6_echo_rep[] =
    "\x32\x59\x3a\xce\x77\x36\x00\x00\x00\x10\x00\x10\x86\xdd\x60\x09" \
    "\x28\x1b\x00\x40\x3a\x40\xfd\xd6\x6e\x3c\xd8\x91\xff\x49\x00\x00" \
    "\x00\x00\x00\x00\x00\x03\xfd\xd6\x6e\x3c\xd8\x91\xff\x49\x00\x00" \
    "\x00\x00\x00\x00\x01\x00\x81\x00\xa2\xd9\x97\x6d\x00\x01\xdc\xd3" \
    "\x85\x5a\x00\x00\x00\x00\x99\x5a\x01\x00\x00\x00\x00\x00\x10\x11" \
    "\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21" \
    "\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31" \
    "\x32\x33\x34\x35\x36\x37";

static void init_packet_in(ofputil_packet_in_private& pin,
                           const void* packet_buf, size_t len,
                           uint64_t cookie = 0,
                           uint8_t table_id = IntFlowManager::ROUTE_TABLE_ID,
                           uint32_t in_port = 42,
                           uint32_t dstReg = 0,
                           uint64_t metadata = 0) {
    memset(&pin, 0, sizeof(pin));
    pin.publ.reason = OFPR_ACTION;
    pin.publ.cookie = cookie;
    pin.publ.packet = (void*)packet_buf;
    pin.publ.packet_len = len;
    pin.publ.table_id = table_id;
    match_set_in_port(&pin.publ.flow_metadata, in_port);
    match_set_reg(&pin.publ.flow_metadata, 0, 0xA0A);
    match_set_reg(&pin.publ.flow_metadata, 5, 10);
    match_set_reg(&pin.publ.flow_metadata, 7, dstReg);
    match_set_metadata(&pin.publ.flow_metadata, metadata);
}

static void verify_dhcpv4(OfpBuf& msg, uint8_t message_type) {
    using namespace dhcp;
    using namespace udp;

    struct ofputil_packet_out po;
    uint64_t ofpacts_stub[1024 / 8];
    struct ofpbuf ofpact;
    ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
    ofputil_decode_packet_out(&po,
                              (ofp_header*)msg.data(),
                              &ofpact);
    DpPacketP pkt;
    struct flow flow;
    dp_packet_use_const(pkt.get(), po.packet, po.packet_len);
    flow_extract(pkt.get(), &flow);

    size_t l4_size = dpp_l4_size(pkt.get());
    BOOST_REQUIRE(l4_size > (sizeof(struct udp_hdr) +
                             sizeof(struct dhcp_hdr) + 1));

    struct dhcp_hdr* dhcp_pkt =
        (struct dhcp_hdr*) ((char*)dpp_l4(pkt.get()) + sizeof(struct udp_hdr));
    BOOST_CHECK_EQUAL(2, dhcp_pkt->op);
    BOOST_CHECK_EQUAL(99, dhcp_pkt->cookie[0]);
    BOOST_CHECK_EQUAL(130, dhcp_pkt->cookie[1]);
    BOOST_CHECK_EQUAL(83, dhcp_pkt->cookie[2]);
    BOOST_CHECK_EQUAL(99, dhcp_pkt->cookie[3]);

    char* cur = (char*)dhcp_pkt + sizeof(struct dhcp_hdr);
    size_t remaining =
        l4_size - sizeof(struct dhcp_hdr) - sizeof(struct udp_hdr);

    unordered_set<uint8_t> foundOptions;

    while (remaining > 0) {
        struct dhcp_option_hdr* hdr = (struct dhcp_option_hdr*)cur;
        foundOptions.insert(hdr->code);

        if (hdr->code == option::END)
            break;
        if (hdr->code == option::PAD) {
            cur += 1;
            remaining -= 1;
            continue;
        }

        if (remaining <= ((size_t)hdr->len + 2))
            break;

        switch (hdr->code) {
        case option::MESSAGE_TYPE:
            {
                uint8_t mt = ((uint8_t*)cur)[2];
                BOOST_CHECK_EQUAL(message_type, mt);
            }
            break;
        case option::SUBNET_MASK:
            BOOST_CHECK(0 == memcmp(opt_subnet_mask, cur,
                                    sizeof(opt_subnet_mask)));
            break;
        case option::ROUTER:
            BOOST_CHECK(0 == memcmp(opt_router, cur, sizeof(opt_router)));
            break;
        case option::DNS:
            BOOST_CHECK(0 == memcmp(opt_dns, cur, sizeof(opt_dns)));
            break;
        case option::DOMAIN_NAME:
            BOOST_CHECK(0 == memcmp(opt_domain, cur, sizeof(opt_domain)));
            break;
        case option::LEASE_TIME:
            BOOST_CHECK(0 == memcmp(opt_lease_time, cur,
                                    sizeof(opt_lease_time)));
            break;
        case option::SERVER_IDENTIFIER:
            BOOST_CHECK(0 == memcmp(opt_server_id, cur,
                                    sizeof(opt_server_id)));
            break;
        case option::CLASSLESS_STATIC_ROUTE:
            BOOST_CHECK(0 == memcmp(opt_route, cur, sizeof(opt_route)));
            break;
        case option::INTERFACE_MTU:
            BOOST_CHECK(0 == memcmp(opt_iface_mtu, cur, sizeof(opt_iface_mtu)));
            break;
        }

        cur += hdr->len + 2;
        remaining -= hdr->len + 2;
    }

#define CONTAINS(x, y) (x.find(y) != x.end())
    BOOST_CHECK(CONTAINS(foundOptions, option::MESSAGE_TYPE));
    BOOST_CHECK(CONTAINS(foundOptions, option::ROUTER));
    BOOST_CHECK(CONTAINS(foundOptions, option::DOMAIN_NAME));
    BOOST_CHECK(CONTAINS(foundOptions, option::LEASE_TIME));
    BOOST_CHECK(CONTAINS(foundOptions, option::SERVER_IDENTIFIER));
    BOOST_CHECK(CONTAINS(foundOptions, option::CLASSLESS_STATIC_ROUTE));
    BOOST_CHECK(CONTAINS(foundOptions, option::INTERFACE_MTU));

#undef CONTAINS
}

enum ReplyType {
    PERM,
    TEMP,
    INFO,
    RAPID_PERM
};

static void verify_dhcpv6(OfpBuf& msg, uint8_t message_type,
                          ReplyType rt = PERM) {
    using namespace dhcp6;
    using namespace udp;

    struct ofputil_packet_out po;
    uint64_t ofpacts_stub[1024 / 8];
    struct ofpbuf ofpact;
    ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
    ofputil_decode_packet_out(&po,
                              (ofp_header*)msg.data(),
                              &ofpact);
    DpPacketP pkt;
    struct flow flow;
    dp_packet_use_const(pkt.get(), po.packet, po.packet_len);
    flow_extract(pkt.get(), &flow);

    size_t l4_size = dpp_l4_size(pkt.get());
    BOOST_REQUIRE(l4_size > (sizeof(struct udp_hdr) +
                             sizeof(struct dhcp6_hdr)));

    struct dhcp6_hdr* dhcp_pkt =
        (struct dhcp6_hdr*)((char*)dpp_l4(pkt.get()) + sizeof(struct udp_hdr));

    BOOST_CHECK_EQUAL(message_type, dhcp_pkt->msg_type);

    unordered_set<uint16_t> foundOptions;

    const size_t opt_hdr_len = sizeof(struct dhcp6_opt_hdr);
    char* cur = (char*)dhcp_pkt + sizeof(struct dhcp6_hdr);
    size_t remaining =
        l4_size - sizeof(struct dhcp6_hdr) - sizeof(struct udp_hdr);
    while (remaining >= opt_hdr_len) {
        struct dhcp6_opt_hdr* hdr = (struct dhcp6_opt_hdr*)cur;
        uint16_t opt_len = ntohs(hdr->option_len);
        uint16_t opt_code = ntohs(hdr->option_code);
        foundOptions.insert(opt_code);

        if (remaining < ((size_t)opt_len + opt_hdr_len))
            break;
        switch (opt_code) {
        case option::CLIENT_IDENTIFIER:
            BOOST_CHECK(0 == memcmp(opt6_client_id, cur,
                                    sizeof(opt6_client_id)));
            break;
        case option::SERVER_IDENTIFIER:
            BOOST_CHECK(0 == memcmp(opt6_server_id, cur,
                                    sizeof(opt6_server_id)));
            break;
        case option::IA_TA:
        case option::IA_NA:
            {
                char* iaaddrs = cur;
                if (opt_code == option::IA_TA) {
                    BOOST_REQUIRE(0 == memcmp(opt6_ia_ta, cur,
                                              sizeof(opt6_ia_ta)));
                    iaaddrs += sizeof(opt6_ia_ta);
                } else {
                    BOOST_REQUIRE(0 == memcmp(opt6_ia_na, cur,
                                              sizeof(opt6_ia_na)));
                    iaaddrs += sizeof(opt6_ia_na);
                }

                // addresses can be in either order
                BOOST_CHECK((0 == memcmp(iaaddr1, iaaddrs, 28) &&
                             0 == memcmp(iaaddr2, iaaddrs + 28, 28)) ||
                            (0 == memcmp(iaaddr2, iaaddrs, 28) &&
                             0 == memcmp(iaaddr1, iaaddrs + 28, 28)));
            }

            break;
        case option::DNS_SERVERS:
            BOOST_CHECK(0 == memcmp(opt6_dns, cur, sizeof(opt6_dns)));
            break;
        case option::DOMAIN_LIST:
            BOOST_CHECK(0 == memcmp(opt6_search_list, cur,
                                    sizeof(opt6_search_list)));
            break;
        }

        cur += opt_len + opt_hdr_len;
        remaining -= opt_len + opt_hdr_len;
    }

#define CONTAINS(x, y) (x.find(y) != x.end())
    BOOST_CHECK(CONTAINS(foundOptions, option::CLIENT_IDENTIFIER));
    BOOST_CHECK(CONTAINS(foundOptions, option::SERVER_IDENTIFIER));
    BOOST_CHECK(CONTAINS(foundOptions, option::DNS_SERVERS));
    BOOST_CHECK(CONTAINS(foundOptions, option::DOMAIN_LIST));
    if (rt == TEMP) {
        BOOST_CHECK(!CONTAINS(foundOptions, option::IA_NA));
        BOOST_CHECK(CONTAINS(foundOptions, option::IA_TA));
    } else if (rt == PERM || rt == RAPID_PERM) {
        BOOST_CHECK(CONTAINS(foundOptions, option::IA_NA));
        BOOST_CHECK(!CONTAINS(foundOptions, option::IA_TA));
    } else {
        BOOST_CHECK(!CONTAINS(foundOptions, option::IA_NA));
        BOOST_CHECK(!CONTAINS(foundOptions, option::IA_TA));
    }
    if (rt == RAPID_PERM) {
        BOOST_CHECK(CONTAINS(foundOptions, option::RAPID_COMMIT));
    }
#undef CONTAINS
}

static void verify_icmpv4_err(OfpBuf& msg) {
    struct ofputil_packet_out po;
    uint64_t ofpacts_stub[1024 / 8];
    struct ofpbuf ofpact;
    ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
    ofputil_decode_packet_out(&po,
                              (ofp_header*)msg.data(),
                              &ofpact);
    DpPacketP pkt;
    struct flow flow;
    dp_packet_use_const(pkt.get(), po.packet, po.packet_len);
    flow_extract(pkt.get(), &flow);

    size_t l4_size = dpp_l4_size(pkt.get());
    BOOST_REQUIRE(l4_size >= (sizeof(struct icmphdr) +
                              sizeof(struct iphdr)));

    struct iphdr* outer_ip_pkt =
        (struct iphdr*)((char*)dpp_l3(pkt.get()));
    struct icmphdr* icmp_pkt =
        (struct icmphdr*)((char*)dpp_l4(pkt.get()));
    struct iphdr* ip_pkt =
        (struct iphdr*)((char*)icmp_pkt + sizeof(icmphdr));

    uint32_t saddr;
    uint32_t daddr;
    memcpy(&saddr, &ip_pkt->saddr, sizeof(saddr));
    memcpy(&daddr, &outer_ip_pkt->daddr, sizeof(daddr));
    BOOST_CHECK_EQUAL(address_v4(saddr), address_v4(daddr));
}

BOOST_FIXTURE_TEST_CASE(dhcpv4_noconfig, PacketInHandlerFixture) {
    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv4_discover, sizeof(pkt_dhcpv4_discover),
                   flow::cookie::DHCP_V4, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));
    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());

    BOOST_CHECK_EQUAL(0, intConn.sentMsgs.size());
}

void PacketInHandlerFixture::testDhcpv4Discover(MockSwitchConnection& tconn) {
    setDhcpv4Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv4_discover, sizeof(pkt_dhcpv4_discover),
                   flow::cookie::DHCP_V4, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, tconn.sentMsgs.size());

    verify_dhcpv4(tconn.sentMsgs[0], opflexagent::dhcp::message_type::OFFER);
}

BOOST_FIXTURE_TEST_CASE(dhcpv4_discover_acc, PacketInHandlerFixture) {
    ep0->setAccessInterface("access");
    ep0->setAccessUplinkInterface("accessUplink");
    pktInHandler.registerConnection(&intConn, &accConn);

    testDhcpv4Discover(accConn);
}

BOOST_FIXTURE_TEST_CASE(dhcpv4_discover, PacketInHandlerFixture) {
    testDhcpv4Discover(intConn);
}

BOOST_FIXTURE_TEST_CASE(dhcpv4_request, PacketInHandlerFixture) {
    setDhcpv4Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv4_request, sizeof(pkt_dhcpv4_request),
                   flow::cookie::DHCP_V4, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));
    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());

    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv4(intConn.sentMsgs[0], opflexagent::dhcp::message_type::ACK);
}

BOOST_FIXTURE_TEST_CASE(dhcpv4_request_inv, PacketInHandlerFixture) {
    setDhcpv4Config();

    char* buf = (char*)malloc(sizeof(pkt_dhcpv4_request));
    memcpy(buf, &pkt_dhcpv4_request, sizeof(pkt_dhcpv4_request));

    // request invalid IP
    buf[0x12a] = 5;

    ofputil_packet_in_private pin;
    init_packet_in(pin, buf, sizeof(pkt_dhcpv4_request),
                   flow::cookie::DHCP_V4, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));
    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    free(buf);

    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv4(intConn.sentMsgs[0], opflexagent::dhcp::message_type::NAK);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_noconfig, PacketInHandlerFixture) {
    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_solicit, sizeof(pkt_dhcpv6_solicit),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));
    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());

    BOOST_CHECK_EQUAL(0, intConn.sentMsgs.size());
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_solicit, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_solicit, sizeof(pkt_dhcpv6_solicit),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::ADVERTISE);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_solicit_rapid, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_solicit_rapid,
                   sizeof(pkt_dhcpv6_solicit_rapid),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::REPLY,
                  RAPID_PERM);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_request, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_request, sizeof(pkt_dhcpv6_request),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::REPLY);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_request_tmp, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_request_tmp, sizeof(pkt_dhcpv6_request_tmp),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::REPLY,
                  TEMP);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_confirm, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_confirm, sizeof(pkt_dhcpv6_confirm),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::REPLY);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_renew, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_renew, sizeof(pkt_dhcpv6_renew),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::REPLY);
}

BOOST_FIXTURE_TEST_CASE(dhcpv6_info_req, PacketInHandlerFixture) {
    setDhcpv6Config();

    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_dhcpv6_info_req, sizeof(pkt_dhcpv6_info_req),
                   flow::cookie::DHCP_V6, IntFlowManager::SEC_TABLE_ID,
                   80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    verify_dhcpv6(intConn.sentMsgs[0], opflexagent::dhcp6::message_type::REPLY,
                  INFO);
}

void PacketInHandlerFixture::testIcmpv4Error(MockSwitchConnection& tconn) {
    ofputil_packet_in_private pin;
    init_packet_in(pin, &pkt_icmp4_ttl_exc, sizeof(pkt_icmp4_ttl_exc),
                   flow::cookie::ICMP_ERROR_V4,
                   IntFlowManager::OUT_TABLE_ID, 5, 80);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, tconn.sentMsgs.size());

    verify_icmpv4_err(tconn.sentMsgs[0]);
}

BOOST_FIXTURE_TEST_CASE(icmpv4_error, PacketInHandlerFixture) {
    testIcmpv4Error(intConn);
}

BOOST_FIXTURE_TEST_CASE(icmpv4_error_acc, PacketInHandlerFixture) {
    ep0->setAccessInterface("access");
    ep0->setAccessUplinkInterface("accessUplink");
    epSrc.updateEndpoint(*ep0);
    pktInHandler.registerConnection(&intConn, &accConn);

    testIcmpv4Error(accConn);
}

void PacketInHandlerFixture::testIcmpEcho(bool v4) {
    ofputil_packet_in_private pin;
    auto req = v4 ? pkt_icmp4_echo_req : pkt_icmp6_echo_req;
    auto rep = v4 ? pkt_icmp4_echo_rep : pkt_icmp6_echo_rep;
    auto rsize = v4 ? sizeof(pkt_icmp4_echo_req) : sizeof(pkt_icmp6_echo_req);
    init_packet_in(pin, req, rsize,
                   v4 ? flow::cookie::ICMP_ECHO_V4 : flow::cookie::ICMP_ECHO_V6,
                   IntFlowManager::ROUTE_TABLE_ID, 5, 101, 0x100);

    OfpBuf b(ofputil_encode_packet_in_private(&pin,
                                              OFPUTIL_P_OF13_OXM,
                                              NXPIF_NXT_PACKET_IN,
                                              0xffff, NULL));

    pktInHandler.Handle(&intConn, OFPTYPE_PACKET_IN, b.get());
    BOOST_REQUIRE_EQUAL(1, intConn.sentMsgs.size());

    struct ofputil_packet_out po;
    uint64_t ofpacts_stub[1024 / 8];
    struct ofpbuf ofpact;
    ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
    ofputil_decode_packet_out(&po,
                              (ofp_header*)intConn.sentMsgs[0]->data,
                              &ofpact);

    BOOST_CHECK(0 == memcmp(rep, po.packet, po.packet_len));
}

BOOST_FIXTURE_TEST_CASE(icmpv4_echo, PacketInHandlerFixture) {
    testIcmpEcho(true);
}

BOOST_FIXTURE_TEST_CASE(icmpv6_echo, PacketInHandlerFixture) {
    testIcmpEcho(false);
}

BOOST_AUTO_TEST_SUITE_END()
