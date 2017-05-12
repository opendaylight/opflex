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

#include "VppApi.h"

using namespace std;
using namespace boost;
using namespace ovsagent;

class VppInit {
public:
    VppApi vppApi;

    VppInit()
        : vppApi(string{"agent-vpp-ut"})
    {
    }

    ~VppInit() {
        if (vppApi.isConnected())
            vppApi.disconnect();
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

BOOST_FIXTURE_TEST_CASE(cb, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

}

BOOST_FIXTURE_TEST_CASE(populateMessageIds, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    auto rv = vppApi.populateMessageIds();

}


BOOST_FIXTURE_TEST_CASE(barrier, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    auto rv = vppApi.barrierSend();
    BOOST_CHECK( rv == 0 );
    auto rvp = vppApi.barrierReceived();
    BOOST_CHECK ( rvp.first == true );

}

BOOST_FIXTURE_TEST_CASE(getMsgIndex, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::string name{"acl_add_replace_3c317936"};
    BOOST_CHECK(vppApi.getMsgIndex(name) != -1);

}

BOOST_FIXTURE_TEST_CASE(getMsgTableSize, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    BOOST_CHECK (vppApi.getMsgTableSize() > 0);
}


BOOST_FIXTURE_TEST_CASE(getVersion, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    auto vppVersion = vppApi.getVersion();
    BOOST_CHECK( vppVersion.program.empty() == false );
    BOOST_CHECK( vppVersion.version.empty() == false );
    BOOST_CHECK( vppVersion.buildDate.empty() == false );
    BOOST_CHECK( vppVersion.buildDirectory.empty() == false );
}

BOOST_FIXTURE_TEST_CASE(printVersion, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );
    vppApi.printVersion(vppApi.getVersion());
}

BOOST_FIXTURE_TEST_CASE(checkVersion, VppInit) {
    vppApi.connect();
    BOOST_REQUIRE( vppApi.isConnected() == true );

    std::vector<string> good{"17.04"};
    std::vector<string> bad{"17.01", "17.07"};

    BOOST_CHECK(vppApi.checkVersion(good) == true);
    BOOST_CHECK(vppApi.checkVersion(bad) == false);
}

BOOST_AUTO_TEST_SUITE_END()
