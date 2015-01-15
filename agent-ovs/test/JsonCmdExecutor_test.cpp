/*
 * Test suite for class JsonCmdExecutor.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>

#include "logging.h"
#include "ovs.h"
#include "SwitchConnection.h"
#include "JsonCmdExecutor.h"

using namespace std;
using namespace boost;
using namespace ovsagent;

BOOST_AUTO_TEST_SUITE (JsonCmdExecutor_test)

enum ReplyType { SEND_ERROR, RECONNECT, RESULT, ERROR };

class MockExecutorConnection : public SwitchConnection {
public:
    MockExecutorConnection() : SwitchConnection("mockBridge"),
        replyStatus(RESULT), jsexec(NULL) {
    }
    ~MockExecutorConnection() {
    }

    int sendJsonMessage(jsonrpc_msg *msg);

    void expect(const string& cmd, const vector<string>& params,
                ReplyType status, const string& reply = "") {
        expectedCommand = cmd;
        expectedParams = params;
        replyStatus = status;
        replyWith = reply;
    }

    string expectedCommand;
    vector<string> expectedParams;
    ReplyType replyStatus;
    string replyWith;
    JsonCmdExecutor *jsexec;
};

class JsonCmdExecutorFixture {
public:
    JsonCmdExecutorFixture() {
        conn.jsexec = &jsexec;
        jsexec.installListenersForConnection(&conn);
        testCommand = "tunl/mcast-dump";
        testParams = assign::list_of("br0")("4789");
        testResult = "224.1.1.1\n224.2.3.5";
        testError = ovs_strerror(EINVAL);
    }
    ~JsonCmdExecutorFixture() {
        jsexec.uninstallListenersForConnection(&conn);
    }

    MockExecutorConnection conn;
    JsonCmdExecutor jsexec;
    string testCommand;
    vector<string> testParams;
    string testResult, testError;
};

BOOST_FIXTURE_TEST_CASE(exec_block, JsonCmdExecutorFixture) {
    conn.expect(testCommand, testParams, RESULT, testResult);
    string result;
    BOOST_CHECK(jsexec.execute(testCommand, testParams, result));
    BOOST_CHECK_EQUAL(result, testResult);

    conn.expect(testCommand, testParams, ERROR, testError);
    BOOST_CHECK(false == jsexec.execute(testCommand, testParams, result));
    BOOST_CHECK_EQUAL(result, testError);
}

BOOST_FIXTURE_TEST_CASE(exec_noblock, JsonCmdExecutorFixture) {
    conn.expect(testCommand, testParams, RESULT, testResult);
    BOOST_CHECK(jsexec.executeNoBlock(testCommand, testParams));

    conn.expect(testCommand, testParams, SEND_ERROR);
    BOOST_CHECK(false == jsexec.executeNoBlock(testCommand, testParams));
}

BOOST_FIXTURE_TEST_CASE(reconnect, JsonCmdExecutorFixture) {
    conn.expect(testCommand, testParams, RECONNECT);
    string result;
    BOOST_CHECK(false == jsexec.execute(testCommand, testParams, result));
    BOOST_CHECK_EQUAL(result, ovs_strerror(ENOTCONN));
}

int MockExecutorConnection::sendJsonMessage(jsonrpc_msg *msg) {
    if (replyStatus == RECONNECT) {
        jsonrpc_msg_destroy(msg);
        jsexec->Connected(this);
        return 0;
    } else if (replyStatus == SEND_ERROR) {
        jsonrpc_msg_destroy(msg);
        return ENOTCONN;
    }
    BOOST_CHECK_EQUAL(msg->type, JSONRPC_REQUEST);
    BOOST_CHECK_EQUAL(msg->method, expectedCommand);
    if (expectedParams.empty()) {
        BOOST_CHECK_EQUAL(msg->params, NULL);
    } else {
        struct json_array *jarr = json_array(msg->params);
        BOOST_CHECK_EQUAL(jarr->n,  expectedParams.size());
        for (int i = 0; i < jarr->n && i < expectedParams.size(); ++i) {
            BOOST_CHECK_EQUAL(json_string(jarr->elems[i]), expectedParams[i]);
        }
    }
    json *res = json_string_create(replyWith.c_str());
    jsonrpc_msg *reply = replyStatus == ERROR ?
                         jsonrpc_create_error(res, msg->id) :
                         jsonrpc_create_reply(res, msg->id);

    jsexec->Handle(this, reply);
    jsonrpc_msg_destroy(reply);
    jsonrpc_msg_destroy(msg);
    return 0;
}

BOOST_AUTO_TEST_SUITE_END()
