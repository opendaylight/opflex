/*
 * Test suite for class PortMapper.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include "SwitchConnection.h"
#include "PortMapper.h"
#include <opflexagent/logging.h>
#include "ovs-ofputil.h"

extern "C" {
#include <openvswitch/ofp-msgs.h>
#include <openvswitch/list.h>
}

using namespace std;
using namespace opflexagent;

class MockConnection : public SwitchConnection {
public:
    MockConnection() : SwitchConnection("mockBridge"), lastSentMsg(NULL) {
    }
    ~MockConnection() {
        ofpbuf_delete(lastSentMsg);
    }

    int GetProtocolVersion() { return OFP13_VERSION; }

    int SendMessage(ofpbuf *msg) {
        ofp_header *hdr = (ofp_header *)msg->data;
        ofptype typ;
        ofptype_decode(&typ, hdr);
        BOOST_CHECK(typ == OFPTYPE_PORT_DESC_STATS_REQUEST);

        ofpbuf_delete(lastSentMsg);
        lastSentMsg = msg;
        return 0;
    }

    ofpbuf *lastSentMsg;
};

// for some reason this type has been moved out of OVS headers
struct ofp11_stats_msg {
    struct ofp_header header;
    ovs_be16 type;              /* One of the OFPST_* constants. */
    ovs_be16 flags;             /* OFPSF_REQ_* flags (none yet defined). */
    uint8_t pad[4];
    /* Followed by the body of the request. */
};

class MockListener : public PortStatusListener {
public:
    void portStatusUpdate(const string& portName, uint32_t portNo, bool) {
        lastPortName = portName;
        lastPortNo = portNo;
    }
    string lastPortName;
    uint16_t lastPortNo;
};

class PortMapperFixture {
public:
    PortMapperFixture() {
        pm.InstallListenersForConnection(&conn);
        for (int i = 0; i < 10; ++i) {
            ofputil_phy_port p;
            MakePort(5*(i+1), &p);
            ports.push_back(p);
        }
    }
    ~PortMapperFixture() {
        pm.UninstallListenersForConnection(&conn);
    }

    void MakePort(uint32_t portNo, ofputil_phy_port *port) {
        memset(port, 0, sizeof(*port));
        port->port_no = portNo;
        sprintf(port->name, "test-port-%d", portNo);
    }

    void SetReplyFlags(ofp_header *reply, uint16_t flags) {
        if ((ofp_version)reply->version > OFP10_VERSION) {
            ((struct ofp11_stats_msg *) reply)->flags = htons(flags);
        }
    }

    ofpbuf *MakeReplyMsg(size_t startIdx, size_t endIdx, bool more) {
        ovs_list replies;
        ofpmp_init(&replies, (ofp_header *)conn.lastSentMsg->data);
        for (size_t i = startIdx; i < endIdx && i < ports.size(); ++i) {
            ofputil_append_port_desc_stats_reply(&ports[i], &replies);
        }
        assert(ovs_list_size(&replies) == 1);

        ofpbuf *reply;
        LIST_FOR_EACH (reply, list_node, &replies) {
            ovs_list_remove(&reply->list_node);

            ofpmsg_update_length(reply);
            ofp_header *replyHdr = (ofp_header *)reply->data;
            ofptype typ;
            ofptype_decode(&typ, replyHdr);
            BOOST_CHECK(typ == OFPTYPE_PORT_DESC_STATS_REPLY);

            if (more) {
                SetReplyFlags(replyHdr, ofpmp_flags(replyHdr) | OFPSF_REPLY_MORE);
            }
            return reply;
        }
        return NULL;
    }

    ofpbuf *MakePortStatusMsg(int portIdx, ofp_port_reason reas) {
        ofputil_port_status ps = {reas, ports[portIdx]};
        return ofputil_encode_port_status
            (&ps, ofputil_protocol_from_ofp_version
             ((ofp_version)conn.GetProtocolVersion()));
    }

    void Received(PortMapper& pm, ofpbuf *msg) {
        ofptype t;
        ofptype_decode(&t, (ofp_header *)msg->data);
        pm.Handle(&conn, t, msg);
        ofpbuf_delete(msg);
    }

    MockConnection conn;
    PortMapper pm;
    std::vector<ofputil_phy_port> ports;
};

BOOST_AUTO_TEST_SUITE(PortMapper_test)

BOOST_FIXTURE_TEST_CASE(portdesc_single, PortMapperFixture) {
    pm.Connected(&conn);

    /* Send all ports in one message */
    ofpbuf *reply = MakeReplyMsg(0, 6, false);
    Received(pm, reply);

    BOOST_CHECK(pm.FindPort("test-port-5") == 5);
    BOOST_CHECK(pm.FindPort("test-port-15") == 15);
    BOOST_CHECK(pm.FindPort("test-port-30") == 30);
    BOOST_CHECK(pm.FindPort("bad-test-port-45") == OFPP_NONE);

    /* spurious message */
    reply = MakeReplyMsg(6, ports.size(), false);
    Received(pm, reply);
    BOOST_CHECK(pm.FindPort("test-port-45") == OFPP_NONE);
}

BOOST_FIXTURE_TEST_CASE(portdesc_multi, PortMapperFixture) {
    pm.Connected(&conn);

    /* Send ports in multiple messages */
    ofpbuf *reply3 = MakeReplyMsg(5, ports.size(), true);
    BOOST_CHECK(pm.FindPort("test-port-45") == OFPP_NONE);
    Received(pm, reply3);

    ofpbuf *reply1 = MakeReplyMsg(0, 2, true);
    BOOST_CHECK(pm.FindPort("test-port-5") == OFPP_NONE);
    Received(pm, reply1);

    ofpbuf *reply2 = MakeReplyMsg(2, 5, false);
    Received(pm, reply2);
    BOOST_CHECK(pm.FindPort("test-port-5") == 5);
    BOOST_CHECK(pm.FindPort("test-port-15") == 15);
    BOOST_CHECK(pm.FindPort("test-port-45") == 45);
}

BOOST_FIXTURE_TEST_CASE(portstatus, PortMapperFixture) {
    pm.Connected(&conn);

    ofpbuf *reply1 = MakeReplyMsg(5, ports.size(), false);
    Received(pm, reply1);

    ofpbuf *notif = MakePortStatusMsg(0, OFPPR_ADD);
    Received(pm, notif);
    BOOST_CHECK(pm.FindPort("test-port-5") == 5);

    notif = MakePortStatusMsg(2, OFPPR_ADD);
    Received(pm, notif);
    BOOST_CHECK(pm.FindPort("test-port-15") == 15);

    notif = MakePortStatusMsg(2, OFPPR_DELETE);
    Received(pm, notif);
    BOOST_CHECK(pm.FindPort("test-port-15") == OFPP_NONE);

    notif = MakePortStatusMsg(8, OFPPR_DELETE);
    Received(pm, notif);
    BOOST_CHECK(pm.FindPort("test-port-45") == OFPP_NONE);

    notif = MakePortStatusMsg(2, OFPPR_DELETE); // delete non-existent
    Received(pm, notif);
}

BOOST_FIXTURE_TEST_CASE(portstatus_listener, PortMapperFixture) {
    pm.Connected(&conn);

    MockListener psl;
    pm.registerPortStatusListener(&psl);

    ofpbuf *reply1 = MakeReplyMsg(0, 1, false);
    Received(pm, reply1);
    BOOST_CHECK_EQUAL(psl.lastPortName, "test-port-5");
    BOOST_CHECK_EQUAL(psl.lastPortNo, 5);

    ofpbuf *notif = MakePortStatusMsg(2, OFPPR_ADD);
    Received(pm, notif);
    BOOST_CHECK_EQUAL(psl.lastPortName, "test-port-15");
    BOOST_CHECK_EQUAL(psl.lastPortNo, 15);

    pm.unregisterPortStatusListener(&psl);
    notif = MakePortStatusMsg(3, OFPPR_DELETE);
    Received(pm, notif);
    BOOST_CHECK_EQUAL(psl.lastPortName, "test-port-15");
    BOOST_CHECK_EQUAL(psl.lastPortNo, 15);
}

BOOST_AUTO_TEST_SUITE_END()
