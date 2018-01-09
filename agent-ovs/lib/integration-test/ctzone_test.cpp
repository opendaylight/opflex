/*
 * Test suite for class CtZoneManager.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <opflexagent/logging.h>
#include "stdlib.h"

#include <opflexagent/IdGenerator.h>
#include "CtZoneManager.h"

BOOST_AUTO_TEST_SUITE(ctzone_test)

class CtZoneFixture {
public:
    CtZoneFixture() : zm(idGen) {
        zm.enableNetLink(true);
        zm.init("test");
    }

    opflexagent::IdGenerator idGen;
    opflexagent::CtZoneManager zm;
};

/*
We expect this test to clear connection tracking zone 1 of any
connection state.  Can prepopulate conntrack zone 1 with:

sudo conntrack -I --zone 1 --src 1.1.1.1 --dst 2.2.2.2 --protonum 6   \
    --sport 42 --dport 24 --state ESTABLISHED --timeout 1000
sudo conntrack -I --zone 2 --src 1.1.1.1 --dst 2.2.2.2 --protonum 6   \
    --sport 42 --dport 24 --state ESTABLISHED --timeout 1000

Then run the test below as root.  Verify that zone 1 has been cleared with:
 conntrack -L --zone 1

And that zone 2 has NOT been cleared with:
sudo conntrack -L --zone 2

In this test we simply verify that the command succeeds and not that
the connection in the zone are actually deleted.
*/
BOOST_FIXTURE_TEST_CASE(allocZone, CtZoneFixture) {
    uint16_t zoneId = zm.getId("id1");
    BOOST_CHECK_EQUAL(1, zoneId);
}

BOOST_AUTO_TEST_SUITE_END()
