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

#include "logging.h"
#include "ovs.h"
#include "SwitchConnection.h"
#include "FlowExecutor.h"
#include "ActionBuilder.h"

using namespace std;
using namespace boost;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;


class MockExecutorConnection : public SwitchConnection {
public:
    MockExecutorConnection() : SwitchConnection("mockBridge"),
        errReply(ofperr(0)), reconnectReply(false) {
    }
    ~MockExecutorConnection() {
    }

    ofp_version GetProtocolVersion() { return OFP13_VERSION; }
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
        for (int i = 0; i < flows.size(); ++i) {
            delete flows[i];
        }
    }

    void createTestFlows();

    MockExecutorConnection conn;
    FlowExecutor fexec;
    vector<FlowEntry *> flows;
};

BOOST_AUTO_TEST_SUITE(FlowExecutor_test)

BOOST_FIXTURE_TEST_CASE(multiedit, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::add, flows[0])
            (FlowEdit::mod, flows[1])(FlowEdit::del, flows[0]);
    conn.Expect(fe);
    BOOST_CHECK(fexec.Execute(fe));
}

BOOST_FIXTURE_TEST_CASE(noblock, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::add, flows[0])
            (FlowEdit::mod, flows[1]);
    conn.Expect(fe);
    BOOST_CHECK(fexec.ExecuteNoBlock(fe));
    BOOST_CHECK(conn.expectedEdits.edits.empty());
}

BOOST_FIXTURE_TEST_CASE(moderror, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::mod, flows[0]);
    conn.Expect(fe);
    conn.ReplyWithError(OFPERR_OFPFMFC_TABLE_FULL);
    BOOST_CHECK(fexec.Execute(fe) == false);
}

BOOST_FIXTURE_TEST_CASE(reconnect, FlowExecutorFixture) {
    FlowEdit fe;
    assign::push_back(fe.edits)(FlowEdit::mod, flows[0]);
    conn.Expect(fe);
    conn.reconnectReply = true;
    BOOST_CHECK(fexec.Execute(fe) == false);
}

BOOST_AUTO_TEST_SUITE_END()

int MockExecutorConnection::SendMessage(ofpbuf *msg) {
    uint16_t COMM[] = {OFPFC_ADD, OFPFC_MODIFY_STRICT, OFPFC_DELETE_STRICT};
    ofp_header *msgHdr = (ofp_header *)ofpbuf_data(msg);
    ofptype type;
    ofptype_decode(&type, msgHdr);
    BOOST_CHECK(type == OFPTYPE_FLOW_MOD ||
            type == OFPTYPE_BARRIER_REQUEST);
    if (type == OFPTYPE_FLOW_MOD) {
        ofputil_flow_mod fm;
        ofpbuf ofpacts;
        ofpbuf_init(&ofpacts, 64);
        int err = ofputil_decode_flow_mod(&fm, msgHdr,
                ofputil_protocol_from_ofp_version(GetProtocolVersion()),
                &ofpacts, OFPP_MAX, 255);
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
                (fm.command == OFPFC_ADD ? htonll(0) : ~htonll(0)));
        BOOST_CHECK(match_equal(&ee.match, &fm.match));
        BOOST_CHECK(ofpacts_equal(ee.ofpacts, ee.ofpacts_len,
                                  fm.ofpacts, fm.ofpacts_len));
        ofpbuf_uninit(&ofpacts);
     } else if (type == OFPTYPE_BARRIER_REQUEST) {
         BOOST_CHECK(expectedEdits.edits.empty());

         if (reconnectReply) {
             executor->Connected(this);
             return 0;
         }
         ofpbuf *barrRep = ofpraw_alloc_reply(OFPRAW_OFPT11_BARRIER_REPLY,
                 msgHdr, 0);
         if (errReply != 0) {
             msgHdr->xid = lastXid;
             ofpbuf *reply = ofperr_encode_reply(errReply, msgHdr);
             executor->Handle(this, OFPTYPE_ERROR, reply);
             ofpbuf_uninit(reply);
         }
         executor->Handle(this, OFPTYPE_BARRIER_REPLY, barrRep);
         ofpbuf_uninit(barrRep);
     }

    return 0;
}

void FlowExecutorFixture::createTestFlows() {
    flows.push_back(new FlowEntry());
    FlowEntry& e0 = *(flows.back());
    e0.entry->table_id = 0;
    e0.entry->priority = 100;
    e0.entry->cookie = 0xabcd;
    match_set_reg(&e0.entry->match, 3, 42);
    match_set_dl_type(&e0.entry->match, htons(ETH_TYPE_IP));
    match_set_nw_dst(&e0.entry->match, 0x01020304);
    ActionBuilder ab0;
    ab0.SetRegLoad(MFF_REG0, 100);
    ab0.SetOutputToPort(OFPP_IN_PORT);
    ab0.Build(e0.entry);

    flows.push_back(new FlowEntry());
    FlowEntry& e1 = *(flows.back());
    e1.entry->table_id = 5;
    e1.entry->priority = 50;
    match_set_in_port(&e1.entry->match, 2168);
    ActionBuilder ab1;
    ab1.SetGotoTable(10);
    ab1.Build(e1.entry);
}

