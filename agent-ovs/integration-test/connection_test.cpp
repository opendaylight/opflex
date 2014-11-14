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
#include "logging.h"

#include "ovs.h"
#include "SwitchConnection.h"

using namespace std;
using namespace opflex::enforcer;
using namespace ovsagent;

class ConnectionFixture {
public:
    ConnectionFixture() : testSwitchName("myTestBridge") {
        AddSwitch(testSwitchName);
    }

    ~ConnectionFixture() {
        RemoveSwitch(testSwitchName);
    }

    int AddSwitch(const string& swName) {
        string comm("ovs-vsctl add-br " + swName);
        return system(comm.c_str());
    }

    int RemoveSwitch(const string& swName) {
        string comm("ovs-vsctl del-br " + swName);
        return system(comm.c_str());
    }

    string testSwitchName;
};


class EchoReplyHandler : public MessageHandler {
public:
    EchoReplyHandler() : counter(0) {}
    void Handle(SwitchConnection *conn, ofptype type, ofpbuf *msg) {
        BOOST_CHECK(type == OFPTYPE_ECHO_REPLY);
        ++counter;
    }
    int counter;
};

class SimpleConnectListener : public OnConnectListener {
public:
    SimpleConnectListener() : counter(0) {}
    void Connected(SwitchConnection *conn) {
        LOG(INFO) << "OnConnectListener - connected";
        ++counter;
    }
    int counter;
};

BOOST_AUTO_TEST_SUITE(connection_test)

BOOST_FIXTURE_TEST_CASE(basic, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    BOOST_CHECK(conn.Connect(OFP13_VERSION));
    BOOST_CHECK(conn.IsConnected());

    conn.Disconnect();
    BOOST_CHECK(!conn.IsConnected());

    BOOST_CHECK(conn.Connect(OFP10_VERSION));
    conn.Disconnect();

    // check invalid bridge
    SwitchConnection conn2("bad_bad_br0");
    BOOST_CHECK(conn2.Connect(OFP10_VERSION) != 0);
}

BOOST_FIXTURE_TEST_CASE(connectListener, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    SimpleConnectListener cl1, cl2;
    conn.RegisterOnConnectListener(&cl1);
    conn.RegisterOnConnectListener(&cl2);

    BOOST_CHECK(conn.Connect(OFP13_VERSION));

    BOOST_CHECK(cl1.counter == 1);
    BOOST_CHECK(cl2.counter == 1);
    conn.Disconnect();

    conn.UnregisterOnConnectListener(&cl2);
    BOOST_CHECK(conn.Connect(OFP10_VERSION));

    BOOST_CHECK(cl1.counter == 2);
    BOOST_CHECK(cl2.counter == 1);
    conn.Disconnect();
}


BOOST_FIXTURE_TEST_CASE(msghandler, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    EchoReplyHandler erh1, erh2;
    conn.RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh1);
    conn.RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh2);

    BOOST_CHECK(conn.Connect(OFP13_VERSION));

    ofpbuf *echoReq1 = make_echo_request(OFP13_VERSION);
    BOOST_CHECK(conn.SendMessage(echoReq1) == 0);
    sleep(1);
    BOOST_CHECK(erh1.counter == 1);
    BOOST_CHECK(erh2.counter == 1);

    conn.UnregisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh1);
    ofpbuf *echoReq2 = make_echo_request(OFP13_VERSION);
    BOOST_CHECK(conn.SendMessage(echoReq2) == 0);
    sleep(1);
    BOOST_CHECK(erh1.counter == 1);
    BOOST_CHECK(erh2.counter == 2);

    conn.Disconnect();
}

BOOST_FIXTURE_TEST_CASE(sendrecv, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);

    EchoReplyHandler erh;
    conn.RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &erh);

    BOOST_CHECK(conn.Connect(OFP13_VERSION));

    const int numEchos = 5;
    for (int i = 0; i < numEchos; ++i) {
        ofpbuf *echoReq = make_echo_request(OFP13_VERSION);
        BOOST_CHECK(conn.SendMessage(echoReq) == 0);
    }
    sleep(2);
    BOOST_CHECK(erh.counter == numEchos);

    conn.Disconnect();

    /* Send when disconnected, expect error */
    ofpbuf *echoReq = make_echo_request(OFP13_VERSION);
    BOOST_CHECK(conn.SendMessage(echoReq) != 0);
}

BOOST_FIXTURE_TEST_CASE(reconnect, ConnectionFixture) {
    SwitchConnection conn(testSwitchName);
    SimpleConnectListener cl1;
    conn.RegisterOnConnectListener(&cl1);

    BOOST_CHECK(conn.Connect(OFP13_VERSION));
    BOOST_CHECK(cl1.counter == 1);

    /* Break connection and make sure we can reconnect */
    RemoveSwitch(testSwitchName);
    sleep(3);
    BOOST_CHECK(!conn.IsConnected());

    AddSwitch(testSwitchName);
    int maxIter = 10;
    for (int nIter = 0; nIter < maxIter; ++nIter) {
        if (conn.IsConnected()) {
            break;
        }
        sleep(1);
    }
    BOOST_CHECK(conn.IsConnected());
    BOOST_CHECK(cl1.counter == 2);

    /* Break connection, and make sure we can disconnect */
    RemoveSwitch(testSwitchName);
    sleep(3);
    BOOST_CHECK(!conn.IsConnected());
    conn.Disconnect();

    AddSwitch(testSwitchName);
}

BOOST_AUTO_TEST_SUITE_END()
