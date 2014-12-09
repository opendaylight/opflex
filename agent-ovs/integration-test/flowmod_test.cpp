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
#include <boost/bind.hpp>
#include "logging.h"

#include "ovs.h"
#include "ConnectionFixture.h"
#include "FlowReader.h"
#include "FlowExecutor.h"
#include "ActionBuilder.h"

using namespace std;
using namespace boost;
using namespace ovsagent;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;

class BlockingFlowReader {
public:
    BlockingFlowReader(SwitchConnection *c) {
        conn = c;
        reader.installListenersForConnection(conn);
        cb = boost::bind(&BlockingFlowReader::gotFlow, this, _1, _2);
        gcb = boost::bind(&BlockingFlowReader::gotGroup, this, _1, _2);
    }
    ~BlockingFlowReader() {
        reader.uninstallListenersForConnection(conn);
    }

    void gotFlow(const FlowEntryList& flows, bool done) {
        recvFlows.insert(recvFlows.end(), flows.begin(), flows.end());
        replyDone = done;
    }
    void gotGroup(const GroupEdit::EntryList& groups, bool done) {
        recvGroups.insert(recvGroups.end(), groups.begin(), groups.end());
        replyDone = done;
    }

    void GetFlows(uint8_t tableId, FlowEntryList& flows) {
        recvFlows.clear();
        replyDone = false;
        reader.getFlows(tableId, cb);
        while (replyDone == false) {
            sleep(1);
        }
        flows.swap(recvFlows);
    }

    void GetGroups(GroupEdit::EntryList& groups) {
        recvGroups.clear();
        replyDone = false;
        reader.getGroups(gcb);
        while (replyDone == false) {
            sleep(1);
        }
        groups.swap(recvGroups);
    }

    SwitchConnection *conn;
    FlowReader reader;
    FlowReader::FlowCb cb;
    FlowEntryList recvFlows;
    FlowReader::GroupCb gcb;
    GroupEdit::EntryList recvGroups;
    bool replyDone;
};

class FlowModFixture : public ConnectionFixture,
                       public OnConnectListener {
public:
    FlowModFixture() : conn(testSwitchName), rdr(&conn), connectDone(false) {
        fexec.InstallListenersForConnection(&conn);
        conn.RegisterOnConnectListener(this);
        createTestFlows();
    }
    ~FlowModFixture() {
        conn.UnregisterOnConnectListener(this);
        fexec.UninstallListenersForConnection(&conn);
    }

    /** Interface: OnConnectListener */
    void Connected(SwitchConnection *swConn) {
        connectDone = true;
    }

    void createTestFlows();

    void compareFlows(const FlowEntry& lhs, const FlowEntry& rhs);
    void removeDefaultFlows(FlowEntryList& newFlows);

    SwitchConnection conn;
    BlockingFlowReader rdr;
    FlowExecutor fexec;
    FlowEntryList testFlows;
    bool connectDone;
};

BOOST_AUTO_TEST_SUITE(flowmod_test)

BOOST_FIXTURE_TEST_CASE(simple_mod, FlowModFixture) {
    BOOST_REQUIRE(!conn.Connect(OFP13_VERSION));
    WAIT_FOR(connectDone == true, 5);

    FlowEdit fe;
    FlowEntryList flows;

    /* add */
    assign::push_back(fe.edits)(FlowEdit::add, testFlows[0]);
    BOOST_CHECK(fexec.Execute(fe));
    rdr.GetFlows(0, flows);
    removeDefaultFlows(flows);
    BOOST_CHECK(flows.size() == 1);
    compareFlows(*testFlows[0], *flows[0]);
    flows.clear();

    /* modify */
    fe.edits.clear();
    assign::push_back(fe.edits)(FlowEdit::mod, testFlows[1]);
    BOOST_CHECK(fexec.Execute(fe));
    rdr.GetFlows(0, flows);
    removeDefaultFlows(flows);
    BOOST_CHECK(flows.size() == 1);
    compareFlows(*testFlows[1], *flows[0]);
    flows.clear();

    /* delete */
    fe.edits.clear();
    assign::push_back(fe.edits)(FlowEdit::del, testFlows[1]);
    BOOST_CHECK(fexec.Execute(fe));
    rdr.GetFlows(0, flows);
    removeDefaultFlows(flows);
    BOOST_CHECK(flows.size() == 0);
    flows.clear();
}

BOOST_FIXTURE_TEST_CASE(group_mod, FlowModFixture) {
    BOOST_REQUIRE(!conn.Connect(OFP13_VERSION));
    WAIT_FOR(connectDone == true, 5);

    GroupEdit::Entry entryIn1(new GroupEdit::GroupMod());
    entryIn1->mod->command = OFPGC11_ADD;
    entryIn1->mod->group_id = 0;
    GroupEdit::Entry entryIn2(new GroupEdit::GroupMod());
    entryIn2->mod->command = OFPGC11_ADD;
    entryIn2->mod->group_id = 1;
    GroupEdit gedit;
    gedit.edits.push_back(entryIn1);
    gedit.edits.push_back(entryIn2);
    BOOST_CHECK(fexec.Execute(gedit));

    GroupEdit::EntryList gl;
    rdr.GetGroups(gl);
    BOOST_CHECK(gl.size() == 2);
    BOOST_CHECK_EQUAL(gl[0]->mod->group_id, entryIn1->mod->group_id);
    BOOST_CHECK_EQUAL(gl[1]->mod->group_id, entryIn2->mod->group_id);

    gedit.edits.clear();
    entryIn1->mod->command = OFPGC11_DELETE;
    gedit.edits.push_back(entryIn1);
    BOOST_CHECK(fexec.Execute(gedit));

    gl.clear();
    rdr.GetGroups(gl);
    BOOST_CHECK(gl.size() == 1);
    BOOST_CHECK_EQUAL(gl[0]->mod->group_id, entryIn2->mod->group_id);
}

BOOST_AUTO_TEST_SUITE_END()

void FlowModFixture::createTestFlows() {
    testFlows.push_back(FlowEntryPtr(new FlowEntry()));
    FlowEntry& e0 = *(testFlows.back());
    e0.entry->table_id = 0;
    e0.entry->priority = 100;
    e0.entry->cookie = 0xabcd;
    match_set_reg(&e0.entry->match, 3, 42);
    match_set_dl_type(&e0.entry->match, htons(ETH_TYPE_IP));
    match_set_nw_dst(&e0.entry->match, 0x01020304);
    ActionBuilder ab0;
    ab0.SetRegLoad(MFF_REG0, 100);
    uint8_t mac[6] = {0xab, 0xcd, 0xef, 0xef, 0xcd, 0xab};
    ab0.SetEthSrcDst(mac, NULL);
    ab0.SetOutputReg(MFF_REG7);
    ab0.Build(e0.entry);

    testFlows.push_back(FlowEntryPtr(new FlowEntry()));
    FlowEntry& e1 = *(testFlows.back());
    memcpy(e1.entry, e0.entry, sizeof(*e0.entry));
    ActionBuilder ab1;
    ab1.SetRegMove(MFF_REG0, MFF_TUN_ID);
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

void FlowModFixture::removeDefaultFlows(FlowEntryList& newFlows) {
    match def;
    match_init_catchall(&def);

    FlowEntryList::iterator itr = newFlows.begin();
    while (itr != newFlows.end()) {
        if (match_equal(&def, &((*itr)->entry->match))) {
            itr = newFlows.erase(itr);
        } else {
            ++itr;
        }
    }
}


