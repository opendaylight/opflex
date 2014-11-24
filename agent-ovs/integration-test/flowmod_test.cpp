/*
 * Integration test suite for flow modifications.
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
#include "ConnectionFixture.h"
#include "FlowExecutor.h"
#include "ActionBuilder.h"

using namespace std;
using namespace boost;
using namespace ovsagent;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;


class FlowReader : public MessageHandler {
public:
    FlowReader(SwitchConnection *c) : conn(c), replyDone(false) {
        conn->RegisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    }
    ~FlowReader() {
        conn->UnregisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    }

    void Handle(SwitchConnection *c, ofptype msgType, ofpbuf *msg) {
        do {
            recvFlows.push_back(new FlowEntry());
            FlowEntry& e = *(recvFlows.back());

            ofpbuf actsBuf;
            ofpbuf_init(&actsBuf, 32);
            int ret = ofputil_decode_flow_stats_reply(e.entry, msg, false,
                    &actsBuf);
            e.entry->ofpacts = (ofpact*)ofpbuf_steal_data(&actsBuf);
            ofpbuf_uninit(&actsBuf);
            LOG(DEBUG) << "FLOW: " << e;

            BOOST_CHECK(ret == 0 || ret == EOF);
            if (ret != 0) {
                replyDone = !ofpmp_more((ofp_header*)msg->frame);
                break;
            }
        } while (true);
    }

    void GetFlows(uint8_t tableId, vector<FlowEntry *>& flows) {
        recvFlows.clear();
        replyDone = false;
        ofp_version ofVer = conn->GetProtocolVersion();
        ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVer);

        ofputil_flow_stats_request fsr1;
        fsr1.aggregate = false;
        match_init_catchall(&fsr1.match);
        fsr1.table_id = tableId;
        fsr1.out_port = OFPP_ANY;
        fsr1.out_group = OFPG11_ANY;
        fsr1.cookie = fsr1.cookie_mask = htonll(0);

        ofpbuf *req = ofputil_encode_flow_stats_request(&fsr1, proto);
        BOOST_REQUIRE(conn->SendMessage(req) == 0);

        while (replyDone == false) {
            sleep(1);
        }
        flows.swap(recvFlows);
    }

    SwitchConnection *conn;
    vector<FlowEntry *> recvFlows;
    bool replyDone;
};

class FlowModFixture : public ConnectionFixture {
public:
    FlowModFixture() : conn(testSwitchName), rdr(&conn) {
        fexec.InstallListenersForConnection(&conn);
        createTestFlows();
    }
    ~FlowModFixture() {
        fexec.UninstallListenersForConnection(&conn);

    }

    void createTestFlows();

    void destroyFlows(vector<FlowEntry *>& flows) {
        for (int i = 0; i < flows.size(); ++i) {
            delete flows[i];
        }
    }

    void compareFlows(const FlowEntry& lhs, const FlowEntry& rhs);
    void removeDefaultFlows(vector<FlowEntry *>& newFlows);

    SwitchConnection conn;
    FlowReader rdr;
    FlowExecutor fexec;
    vector<FlowEntry *> testFlows;
};

BOOST_AUTO_TEST_SUITE(flowmod_test)

BOOST_FIXTURE_TEST_CASE(simple_mod, FlowModFixture) {
    BOOST_REQUIRE(!conn.Connect(OFP13_VERSION));

    FlowEdit fe;
    vector<FlowEntry *> flows;

    /* add */
    assign::push_back(fe.edits)(FlowEdit::add, testFlows[0]);
    fexec.Execute(fe);
    rdr.GetFlows(0, flows);
    removeDefaultFlows(flows);
    BOOST_CHECK(flows.size() == 1);
    compareFlows(*testFlows[0], *flows[0]);
    destroyFlows(flows);

    /* modify */
    fe.edits.clear();
    assign::push_back(fe.edits)(FlowEdit::mod, testFlows[1]);
    fexec.Execute(fe);
    rdr.GetFlows(0, flows);
    removeDefaultFlows(flows);
    BOOST_CHECK(flows.size() == 1);
    compareFlows(*testFlows[1], *flows[0]);
    destroyFlows(flows);

    /* delete */
    fe.edits.clear();
    assign::push_back(fe.edits)(FlowEdit::del, testFlows[1]);
    fexec.Execute(fe);
    rdr.GetFlows(0, flows);
    removeDefaultFlows(flows);
    BOOST_CHECK(flows.size() == 0);
    destroyFlows(flows);
}

BOOST_AUTO_TEST_SUITE_END()

void FlowModFixture::createTestFlows() {
    testFlows.push_back(new FlowEntry());
    FlowEntry& e0 = *(testFlows.back());
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

    testFlows.push_back(new FlowEntry());
    FlowEntry& e1 = *(testFlows.back());
    memcpy(e1.entry, e0.entry, sizeof(*e0.entry));
    ActionBuilder ab1;
    ab1.SetGotoTable(10);
    ab1.Build(e1.entry);
}

void FlowModFixture::compareFlows(const FlowEntry& lhs,
        const FlowEntry& rhs) {
    const ofputil_flow_stats& le = *(lhs.entry);
    const ofputil_flow_stats& re = *(rhs.entry);
    BOOST_CHECK(le.table_id == re.table_id);
    BOOST_CHECK(le.priority == re.priority);
    BOOST_CHECK(le.cookie == re.cookie);
    BOOST_CHECK(match_equal(&le.match, &re.match));
    BOOST_CHECK(ofpacts_equal(le.ofpacts, le.ofpacts_len,
                              re.ofpacts, re.ofpacts_len));
}

void FlowModFixture::removeDefaultFlows(
        vector<FlowEntry *>& newFlows) {
    match def;
    match_init_catchall(&def);

    vector<FlowEntry *>::iterator itr = newFlows.begin();
    while (itr != newFlows.end()) {
        if (match_equal(&def, &((*itr)->entry->match))) {
            FlowEntry *e = *itr;
            itr = newFlows.erase(itr);
            delete e;
        } else {
            ++itr;
        }
    }
}


