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

#include <thread>
#include <chrono>

using namespace std;
using namespace boost;
using namespace ovsagent;

#define VPP_TESTS

class VppInit {
public:
    int pauseMS;

    VppInit()
    {
        pauseMS=500;
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

    auto makeVeth (string name) -> void
    {
        std::system(("sudo ip link add " + name + " type veth peer name " + name + "-peer").c_str());
    }

    auto delVeth (string name) -> void
    {
        std::system(("sudo ip link del " + name).c_str());
    }
};

BOOST_AUTO_TEST_SUITE(VppApi_test)

// Putting configure var to disable VPP UT until Jenkins sorted out.

#ifndef VPP_TESTS
BOOST_AUTO_TEST_CASE(disable) {
    LOG(INFO) << "VPP tests temporarily disabled. uncomment #define VPP_TESTS";
}
#endif //not VPP_TESTS

#ifdef VPP_TESTS

BOOST_FIXTURE_TEST_CASE(cb, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(populateMessageIds, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    auto rv = vppApi.populateMessageIds();
    vppApi.disconnect();
}


BOOST_FIXTURE_TEST_CASE(barrier, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    auto rv = vppApi.barrierSend();
    BOOST_CHECK( rv == 0 );
    auto rvp = vppApi.barrierReceived();
    BOOST_CHECK ( rvp.first == true );
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    vppApi.disconnect();

}

BOOST_FIXTURE_TEST_CASE(getMsgIndex, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::string name{"acl_add_replace_3c317936"};
    BOOST_CHECK(vppApi.getMsgIndex(name) != -1);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(getMsgTableSize, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    BOOST_CHECK (vppApi.getMsgTableSize() > 0);
    vppApi.disconnect();
}


BOOST_FIXTURE_TEST_CASE(getVersion, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    auto vppVersion = vppApi.getVersion();
    BOOST_CHECK( vppVersion.program.empty() == false );
    BOOST_CHECK( vppVersion.version.empty() == false );
    BOOST_CHECK( vppVersion.buildDate.empty() == false );
    BOOST_CHECK( vppVersion.buildDirectory.empty() == false );
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(printVersion, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    vppApi.printVersion(vppApi.getVersion());
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(checkVersion, VppInit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::vector<string> good{"17.04"};
    std::vector<string> bad{"17.01", "17.07"};

    BOOST_CHECK(vppApi.checkVersion(good) == true);
    BOOST_CHECK(vppApi.checkVersion(bad) == false);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(createBridge, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    string name{"br1"};
    BOOST_CHECK( vppApi.createBridge(name) == 0 );
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(deleteBridge, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    string name{"br1"};
    BOOST_CHECK( vppApi.createBridge(name) == 0 );
    BOOST_CHECK( vppApi.deleteBridge(name) == 0 );
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(createAfPacketIntf, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    std::string const name{"veth1-test"};
    makeVeth(name);

    int rv = vppApi.createAfPacketIntf(name);
    BOOST_CHECK(rv == 0);
    delVeth(name);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(deleteAfPacketIntf, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    std::string const name{"veth1-test"};
    makeVeth(name);

    BOOST_CHECK( vppApi.createAfPacketIntf(name) == 0);
    BOOST_CHECK( vppApi.deleteAfPacketIntf(name) == 0);
    delVeth(name);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(setIntfIpAddr, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    u8 is_add = 1;
    u8 del_all = 0;
    u8 is_ipv6 = 0;
    ip46_address address = vppApi.boostToVppIp46(
		boost::asio::ip::address::from_string("192.168.10.10"));
    u8 mask = 24;

    string name{"veth1-test"};
    //Test for non-existant port
    BOOST_CHECK(vppApi.setIntfIpAddr(name, is_add, del_all, is_ipv6, address, mask) == -201);
    makeVeth(name);
    if (vppApi.createAfPacketIntf(name) == 0) {
        BOOST_CHECK(vppApi.setIntfIpAddr(name, is_add, del_all, is_ipv6, address, mask) == 0);
    } else {
        BOOST_TEST_MESSAGE("Failed to create afpacket interface required for test.");
    }

    delVeth(name);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(setIntfFlags, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    u32 flags = 1;
    string name{"veth1-test"};

    //Test for non-existant port
    BOOST_CHECK(vppApi.setIntfFlags(name, flags) == -2);
    makeVeth(name);
    if (vppApi.createAfPacketIntf(name) == 0) {
        BOOST_CHECK(vppApi.setIntfFlags(name, flags) == 0);
    } else {
        BOOST_TEST_MESSAGE("Failed to create afpacket interface required for test.");
    }

    delVeth(name);
    vppApi.disconnect();
}

BOOST_FIXTURE_TEST_CASE(setIntfL2Bridge, VppInit) {
    VppApi vppApi(string{"agent-vpp-ut"});
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::string const intf{"veth1-test"};
    std::string const bridge{"b1-test"};

    //Create Interface
    makeVeth(intf);
    BOOST_CHECK(vppApi.createAfPacketIntf(intf) == 0);

    //Try to add to non-existant bridge
    BOOST_CHECK(vppApi.setIntfL2Bridge(bridge, intf, 0) == -202);

    //Create bridge
    BOOST_CHECK(vppApi.createBridge(bridge) == 0);

    //Try to add to existing bridge
    BOOST_CHECK(vppApi.setIntfL2Bridge(bridge, intf, 0) == 0);
    delVeth(intf);
    vppApi.disconnect();
}

#endif //VPP_TESTS

BOOST_AUTO_TEST_SUITE_END()
