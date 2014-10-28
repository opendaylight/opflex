/*
 * Test suite for class Inventory.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <glog/logging.h>
#include <ovs.h>
#include <switchConnection.h>

using namespace std;
using namespace opflex::enforcer;

/*
 * Assumes that an OVS switch named "br0" already exists.
 */

class ConnectionFixture {
public:
    ConnectionFixture() {
        google::InitGoogleLogging("");
        FLAGS_logtostderr = 0;      // set to 1 to enable logging
    }
};

BOOST_AUTO_TEST_SUITE(connection_test)

BOOST_FIXTURE_TEST_CASE(basic, ConnectionFixture) {
    SwitchConnection conn("br0");

    BOOST_CHECK(conn.Connect(OFP13_VERSION));
    BOOST_CHECK(conn.IsConnected());

    conn.Disconnect();
    BOOST_CHECK(!conn.IsConnected());

    BOOST_CHECK(conn.Connect(OFP10_VERSION));
    conn.Disconnect();

    // check invalid bridge
    SwitchConnection conn2("bad_bad_br0");
    BOOST_CHECK(conn2.Connect(OFP10_VERSION) == false);
}

BOOST_AUTO_TEST_SUITE_END()
