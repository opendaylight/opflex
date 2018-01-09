/*
 * Test suite for class SwitchConnection.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <opflexagent/logging.h>

#include "SwitchConnection.h"
#include "ConnectionFixture.h"

#include "ovs-ofputil.h"

using namespace std;
using namespace opflexagent;

class EchoReplyHandler : public MessageHandler {
public:
    EchoReplyHandler() : counter(0) {}
    void Handle(SwitchConnection*, int type, ofpbuf*) {
        BOOST_CHECK(type == OFPTYPE_ECHO_REPLY);
        ++counter;
    }
    int counter;
};

class SimpleConnectListener : public OnConnectListener {
public:
    SimpleConnectListener() : counter(0) {}
    void Connected(SwitchConnection*) {
        LOG(INFO) << "OnConnectListener - connected";
        ++counter;
    }
    int counter;
};

BOOST_AUTO_TEST_SUITE(connection_test)

BOOST_FIXTURE_TEST_CASE(basic, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    BOOST_CHECK(!conn.Connect(OFP13_VERSION));
    BOOST_CHECK(conn.IsConnected());

    conn.Disconnect();
    BOOST_CHECK(!conn.IsConnected());

    BOOST_CHECK(!conn.Connect(OFP10_VERSION));
    conn.Disconnect();
}

BOOST_FIXTURE_TEST_CASE(noswitch, ConnectionFixture) {
    string newSwitch("newTestSwitch");
    SwitchConnection conn(newSwitch);

    BOOST_CHECK(conn.Connect(OFP13_VERSION) != 0);

    AddSwitch(newSwitch);
    WAIT_FOR(conn.IsConnected(), 10);

    conn.Disconnect();
    RemoveSwitch(newSwitch);
}

BOOST_FIXTURE_TEST_CASE(connectListener, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    SimpleConnectListener cl1, cl2;
    conn.RegisterOnConnectListener(&cl1);
    conn.RegisterOnConnectListener(&cl2);

    BOOST_CHECK(!conn.Connect(OFP13_VERSION));

    WAIT_FOR(cl1.counter == 1, 5);
    BOOST_CHECK(cl2.counter == 1);
    conn.Disconnect();

    conn.UnregisterOnConnectListener(&cl2);
    BOOST_CHECK(!conn.Connect(OFP10_VERSION));

    WAIT_FOR(cl1.counter == 2, 5);
    BOOST_CHECK(cl2.counter == 1);
    conn.Disconnect();
}


BOOST_FIXTURE_TEST_CASE(msghandler, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    EchoReplyHandler erh1, erh2;
    conn.RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh1);
    conn.RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh2);

    BOOST_CHECK(!conn.Connect(OFP13_VERSION));

    ofpbuf *echoReq1 = make_echo_request(OFP13_VERSION);
    BOOST_CHECK(conn.SendMessage(echoReq1) == 0);
    WAIT_FOR(erh1.counter == 1, 1);
    BOOST_CHECK(erh2.counter == 1);

    conn.UnregisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh1);
    ofpbuf *echoReq2 = make_echo_request(OFP13_VERSION);
    BOOST_CHECK(conn.SendMessage(echoReq2) == 0);
    WAIT_FOR(erh2.counter == 2, 1);
    BOOST_CHECK(erh1.counter == 1);

    conn.Disconnect();
}

BOOST_FIXTURE_TEST_CASE(sendrecv, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    EchoReplyHandler erh;
    conn.RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh);

    BOOST_CHECK(!conn.Connect(OFP13_VERSION));

    const int numEchos = 5;
    for (int i = 0; i < numEchos; ++i) {
        ofpbuf *echoReq = make_echo_request(OFP13_VERSION);
        BOOST_CHECK(conn.SendMessage(echoReq) == 0);
    }
    WAIT_FOR(erh.counter == numEchos, 5);

    conn.Disconnect();

    /* Send when disconnected, expect error */
    ofpbuf *echoReq = make_echo_request(OFP13_VERSION);
    BOOST_CHECK(conn.SendMessage(echoReq) != 0);
}

BOOST_FIXTURE_TEST_CASE(reconnect, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);
    SimpleConnectListener cl1;
    conn.RegisterOnConnectListener(&cl1);

    BOOST_CHECK(!conn.Connect(OFP13_VERSION));
    WAIT_FOR(cl1.counter == 1, 5);

    /* Break connection and make sure we can reconnect */
    RemoveSwitch(testSwitchName);
    WAIT_FOR(!conn.IsConnected(), 5);

    AddSwitch(testSwitchName);
    WAIT_FOR(conn.IsConnected(), 10);
    BOOST_CHECK(cl1.counter == 2);

    /* Break connection, and make sure we can disconnect quickly */
    RemoveSwitch(testSwitchName);
    WAIT_FOR(!conn.IsConnected(), 5);
    using namespace boost::posix_time;
    ptime start(microsec_clock::local_time());
    conn.Disconnect();
    ptime end(microsec_clock::local_time());
    BOOST_CHECK((end - start).total_seconds() < 3);
    LOG(DEBUG) << "Disconnected after losing connection to switch";
}

BOOST_AUTO_TEST_SUITE_END()
