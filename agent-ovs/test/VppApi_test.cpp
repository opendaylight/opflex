/*
 * Test suite for class VppApi
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/assign/list_inserter.hpp>

#include <iostream>

#include "logging.h"
#include "VppApi.h"
#include "MockVppConnection.h"

#include <thread>
#include <chrono>

using namespace std;
using namespace boost;
using namespace ovsagent;

class VppInit {
public:
    int pauseMS;
    VppApi vppApi;
    string name;

    VppInit()
        : pauseMS(1000),
          name(string{"vpp-ut"}),
          vppApi(unique_ptr<MockVppConnection>(new MockVppConnection())) {
    }

    ~VppInit() {
        /*
           TODO: put a VPP restart in here. Each UT shouldn't rely on existing state of VPP

           #include <cstdlib>
           std::system("systemctl restart vpp"); //Check for systemd etc

           OR BETTER
           send "cli_exec()" API call with "restart"

          For now its problematic as these UTs are used
          for TDD, so have VPP running in GDB. Each test is meant to be run singularly:
          agent_test --run_test=VppApi_test/<testname> e.g.
          agent_test --run_test=VppApi_test/createBridge or
          sudo gdb --fullname <path>/agent_test
          (gdb) r --run_test=VppApi_test/createBridge

         */
    }

    auto isEqualIp46_address(ip46_address a1, ip46_address a2) -> bool
    {
        if (!(a1.is_ipv6 == a2.is_ipv6)) return false;
        if (a1.is_ipv6) {
            for (int i = 0; i < 8; ++ i) {
                if (!(a1.ip6.as_u16[i] == a2.ip6.as_u16[i])) return false;
            }
        } else {
            if (!(a1.ip4.as_u32 == a2.ip4.as_u32)) return false;
        }
        return true;
    }

};

BOOST_AUTO_TEST_SUITE(VppApi_test)

BOOST_FIXTURE_TEST_CASE(checkVersion1, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::vector<string> good{"17.04"};

    BOOST_CHECK(vppApi.checkVersion(good) == true);

    std::vector<string> bad{"17.01", "17.07"};
    BOOST_CHECK(vppApi.checkVersion(bad) == false);

    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(createBridge, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );
    string name{"br1"};
    BOOST_CHECK( vppApi.createBridge(name) == 0 );
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(deleteBridge, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );
    string name{"br1"};
    BOOST_CHECK( vppApi.createBridge(name) == 0 );
    BOOST_CHECK( vppApi.deleteBridge(name) == 0 );
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(createAfPacketIntf, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );
    std::string const name{"veth1-test"};

    int rv = vppApi.createAfPacketIntf(name);
    BOOST_CHECK(rv == 0);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(deleteAfPacketIntf, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );
    std::string const name{"veth1-test"};

    BOOST_CHECK( vppApi.createAfPacketIntf(name) == 0);
    BOOST_CHECK( vppApi.deleteAfPacketIntf(name) == 0);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(setIntfIpAddr, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );

    u8 isAdd = 1;
    u8 delAll = 0;
    u8 isIpv6 = 0;
    ip46_address address = vppApi.boostToVppIp46(
               boost::asio::ip::address::from_string("192.168.10.10"));
    u8 mask = 24;

    string name{"veth1-test"};
    //Test for non-existant port
    BOOST_CHECK(vppApi.setIntfIpAddr(name, isAdd, delAll, isIpv6, address, mask) == -201);
    if (vppApi.createAfPacketIntf(name) == 0) {
        BOOST_CHECK(vppApi.setIntfIpAddr(name, isAdd, delAll, isIpv6, address, mask) == 0);
    } else {
        BOOST_TEST_MESSAGE("Failed to create afpacket interface required for test.");
    }

    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(setIntfFlags, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );

    u32 flags = 1;
    string name{"veth1-test"};

    //Test for non-existant port
    BOOST_CHECK(vppApi.setIntfFlags(name, flags) == -2);
    if (vppApi.createAfPacketIntf(name) == 0) {
        BOOST_CHECK(vppApi.setIntfFlags(name, flags) == 0);
    } else {
        BOOST_TEST_MESSAGE("Failed to create afpacket interface required for test.");
    }

    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(setIntfL2Bridge, VppInit) {
    vppApi.connect(name);
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::string const intf{"veth1-test"};
    std::string const bridge{"b1-test"};

    //Create Interface
    BOOST_CHECK(vppApi.createAfPacketIntf(intf) == 0);

    //Try to add to non-existant bridge
    BOOST_CHECK(vppApi.setIntfL2Bridge(bridge, intf, 0) == -202);

    //Create bridge
    BOOST_CHECK(vppApi.createBridge(bridge) == 0);

    //Try to add to existing bridge
    BOOST_CHECK(vppApi.setIntfL2Bridge(bridge, intf, 0) == 0);
    vppApi.disconnect();
}

BOOST_AUTO_TEST_SUITE_END()
