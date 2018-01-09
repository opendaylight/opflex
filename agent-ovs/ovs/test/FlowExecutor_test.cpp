/*
 * Test suite for class FlowExecutor.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_inserter.hpp>

#include <opflexagent/logging.h>

#include "SwitchConnection.h"
#include "FlowExecutor.h"
#include "FlowBuilder.h"

#include "ovs-shim.h"
#include "ovs-ofputil.h"

using namespace std;
using namespace boost;
using namespace opflexagent;

class MockExecutorConnection : public SwitchConnection {
public:
    MockExecutorConnection() : SwitchConnection("mockBridge"),
        errReply(ofperr(0)), reconnectReply(false) {
    }
    ~MockExecutorConnection() {
    }

    int GetProtocolVersion() { return OFP13_VERSION; }
    int SendMessage(ofpbuf *msg);

    void Expect(const FlowEdit& fe) {
        expectedEdits = fe;
    }
    void ReplyWithError(ofperr err) {
        errReply = err;
    }

    FlowEdit expectedEdits;
    ovs_be32 lastXid;
    ofperr errReply;
    bool reconnectReply;
    FlowExecutor *executor;
};

class FlowExecutorFixture {
public:
    FlowExecutorFixture() {
        conn.executor = &fexec;
        fexec.InstallListenersForConnection(&conn);
        createTestFlows();
    }
    ~FlowExecutorFixture() {
        fexec.UninstallListenersForConnection(&conn);
        flows.clear();
    }

    void createTestFlows();

    MockExecutorConnection conn;
    FlowExecutor fexec;
    FlowEntryList flows;
};

BOOST_AUTO_TEST_SUITE(FlowExecutor_test)

BOOST_FIXTURE_TEST_CASE(multiedit, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::ADD, flows[0])
            (FlowEdit::MOD, flows[1])(FlowEdit::DEL, flows[0]);
    conn.Expect(fe);
    BOOST_CHECK(fexec.Execute(fe));
}

BOOST_FIXTURE_TEST_CASE(noblock, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::ADD, flows[0])
            (FlowEdit::MOD, flows[1]);
    conn.Expect(fe);
    BOOST_CHECK(fexec.ExecuteNoBlock(fe));
    BOOST_CHECK(conn.expectedEdits.edits.empty());
}

BOOST_FIXTURE_TEST_CASE(moderror, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::MOD, flows[0]);
    conn.Expect(fe);
    conn.ReplyWithError(OFPERR_OFPFMFC_TABLE_FULL);
    BOOST_CHECK(fexec.Execute(fe) == false);
}

BOOST_FIXTURE_TEST_CASE(reconnect, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::MOD, flows[0]);
    conn.Expect(fe);
    conn.reconnectReply = true;
    BOOST_CHECK(fexec.Execute(fe) == false);
}

BOOST_AUTO_TEST_SUITE_END()

int MockExecutorConnection::SendMessage(ofpbuf *msg) {
    uint16_t COMM[] = {OFPFC_ADD, OFPFC_MODIFY_STRICT, OFPFC_DELETE_STRICT};
    ofp_header *msgHdr = (ofp_header *)msg->data;
    ofptype type;
    ofptype_decode(&type, msgHdr);
    BOOST_CHECK(type == OFPTYPE_FLOW_MOD ||
            type == OFPTYPE_BARRIER_REQUEST);
    if (type == OFPTYPE_FLOW_MOD) {
        ofputil_flow_mod fm;
        ofpbuf ofpacts;
        ofpbuf_init(&ofpacts, 64);
        int err = ofputil_decode_flow_mod
            (&fm, msgHdr, ofputil_protocol_from_ofp_version
             ((ofp_version)GetProtocolVersion()),
                &ofpacts, OFPP_MAX, 255);
        fm.ofpacts = ActionBuilder::getActionsFromBuffer(&ofpacts,
                fm.ofpacts_len);
        ofpbuf_uninit(&ofpacts);
        BOOST_CHECK_EQUAL(err, 0);
        BOOST_CHECK(!expectedEdits.edits.empty());
        lastXid = msgHdr->xid;

        FlowEdit::Entry edit = expectedEdits.edits.front();
        ofputil_flow_stats &ee = *(edit.second->entry);
        expectedEdits.edits.erase(expectedEdits.edits.begin());
        BOOST_CHECK(COMM[edit.first] == fm.command);
        BOOST_CHECK(ee.table_id == fm.table_id);
        BOOST_CHECK(ee.priority == fm.priority);
        BOOST_CHECK(ee.cookie ==
                (fm.command == OFPFC_ADD ? fm.new_cookie : fm.cookie));
        BOOST_CHECK(fm.cookie_mask ==
                    (fm.command == OFPFC_ADD ? 0 : ~((uint64_t)0)));
        BOOST_CHECK(match_equal(&ee.match, &fm.match));
        if (fm.command == OFPFC_DELETE_STRICT) {
            BOOST_CHECK_EQUAL(fm.ofpacts_len, 0);
        } else {
            BOOST_CHECK(action_equal(ee.ofpacts, ee.ofpacts_len,
                                     fm.ofpacts, fm.ofpacts_len));
        }
        free((void *)fm.ofpacts);
    } else if (type == OFPTYPE_BARRIER_REQUEST) {
         BOOST_CHECK(expectedEdits.edits.empty());

         if (reconnectReply) {
             ofpbuf_delete(msg);
             executor->Connected(this);
             return 0;
         }
         struct ofpbuf *barrRep =
             ofpraw_alloc_reply(OFPRAW_OFPT11_BARRIER_REPLY, msgHdr, 0);
         if (errReply != 0) {
             msgHdr->xid = lastXid;
             struct ofpbuf *reply = ofperr_encode_reply(errReply, msgHdr);
             executor->Handle(this, OFPTYPE_ERROR, reply);
             ofpbuf_delete(reply);
         }
         executor->Handle(this, OFPTYPE_BARRIER_REPLY, barrRep);
         ofpbuf_delete(barrRep);
    }

    ofpbuf_delete(msg);
    return 0;
}

void FlowExecutorFixture::createTestFlows() {
    FlowBuilder e0;
    e0.priority(100)
        .cookie(0xabcd)
        .reg(3, 42)
        .ipDst(boost::asio::ip::address::from_string("1.2.3.4"))
        .action()
        .reg(MFF_REG0, 100)
        .output(OFPP_IN_PORT);
    flows.push_back(e0.build());

    FlowBuilder e1;
    e1.priority(50)
        .inPort(2168)
        .action().go(10);
    flows.push_back(e1.build());

    flows[0]->entry->table_id = 0;
    flows[1]->entry->table_id = 5;
}

