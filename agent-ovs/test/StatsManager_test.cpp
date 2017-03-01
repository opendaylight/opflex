/*
 * Test suite for class StatsManager.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>

#include "logging.h"
#include "ModbFixture.h"
#include "StatsManager.h"
#include "MockPortMapper.h"
#include "SwitchConnection.h"

using boost::optional;
using std::shared_ptr;
using modelgbp::observer::EpStatUniverse;

namespace ovsagent {

enum {
    TEST_CONN_TYPE_INT=0,
    TEST_CONN_TYPE_ACC,
};

class MockConnection : public SwitchConnection {
public:
    MockConnection(int conn_type) : SwitchConnection((conn_type==
              TEST_CONN_TYPE_INT)? "int_conn": "acc_conn"), lastSentMsg(NULL) {
        nType = conn_type;
        lastSentMsg = NULL;
    }
    ~MockConnection() {
        if (lastSentMsg) ofpbuf_delete(lastSentMsg);
    }
    int SendMessage(struct ofpbuf *msg) {
        switch(nType) {
        case TEST_CONN_TYPE_ACC:
        case TEST_CONN_TYPE_INT:
        default:
            BOOST_CHECK(msg != NULL);
            ofp_header *hdr = (ofp_header *)msg->data;
            ofptype typ;
            ofptype_decode(&typ, hdr);
            BOOST_CHECK(typ == OFPTYPE_PORT_STATS_REQUEST);
            if (msg) ofpbuf_delete(msg);
            break;
        }
        return 0;
    }

    ofpbuf *lastSentMsg;
private:
    int nType;
};

class StatsManagerFixture : public ModbFixture {

public:
    StatsManagerFixture() : statsManager(&agent, intPortMapper,
                    accessPortMapper, 10) {
        createObjects();

        // set a couple of access bridge end-points in EP manager
        ep0.reset(new Endpoint("0-0-0-0"));
        ep0->setAccessInterface("ep0-acc");
        ep1.reset(new Endpoint("0-0-0-1"));
        ep1->setAccessInterface("ep1-acc");

        accessPortMapper.ports[ep0->getAccessInterface().get()] = 1;
        accessPortMapper.RPortMap[1] = ep0->getAccessInterface().get();
        accessPortMapper.ports[ep1->getAccessInterface().get()] = 2;
        accessPortMapper.RPortMap[2] = ep1->getAccessInterface().get();
        epSrc.updateEndpoint(*ep0);
        epSrc.updateEndpoint(*ep1);

        // set a couple of internal bridge end-points in EP manager
        ep2.reset(new Endpoint("0-0-0-2"));
        ep2->setInterfaceName("ep0-int");
        ep3.reset(new Endpoint("0-0-0-3"));
        ep3->setInterfaceName("ep1-int");

        // add couple of ports to intPortMapper
        intPortMapper.ports[ep2->getInterfaceName().get()] = 1;
        intPortMapper.RPortMap[1] = ep2->getInterfaceName().get();
        intPortMapper.ports[ep3->getInterfaceName().get()] = 2;
        intPortMapper.RPortMap[2] = ep3->getInterfaceName().get();
        epSrc.updateEndpoint(*ep2);
        epSrc.updateEndpoint(*ep3);
      }
    virtual ~StatsManagerFixture() {}
    void verifyCounters(int *dummy, int port_num);

    StatsManager statsManager;
    MockPortMapper intPortMapper;
    MockPortMapper accessPortMapper;

private:
};

BOOST_AUTO_TEST_SUITE(StatsManager_test)

BOOST_FIXTURE_TEST_CASE(startAndStopBeforeInitialization, StatsManagerFixture) {
    statsManager.stop(); // let it throw exception if there is a problem
    statsManager.start();
    statsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(registerWithoutIntConnection, StatsManagerFixture) {
    statsManager.registerConnection(NULL,NULL);

    MockConnection accessPortConn(TEST_CONN_TYPE_ACC);
    statsManager.registerConnection(NULL, &accessPortConn);
}

struct ofpbuf *makeStatResponseMessage(MockConnection *pConn,
                     int stats[], ofp_port_t port_num) {
    struct ofpbuf *req_msg = ofputil_encode_dump_ports_request(
        (ofp_version)OFP13_VERSION, port_num);

    struct ofp_header *req_hdr = (ofp_header *)req_msg->data;

    ovs_list ovs_replies;
    struct ofpbuf *reply = 0;

    ofputil_decode_port_stats_request(req_hdr, &port_num);

    ofpmp_init(&ovs_replies, req_hdr);

    {
        struct ofputil_port_stats ops = { .port_no = port_num };
        ops.stats.rx_packets = stats[0];
        ops.stats.tx_packets = stats[1];
        ops.stats.rx_bytes   = stats[2];
        ops.stats.tx_bytes   = stats[3];
        ops.stats.rx_dropped = stats[4];
        ops.stats.tx_dropped = stats[5];

        ofputil_append_port_stat(&ovs_replies, &ops);
    }

    {
        struct ofpbuf *reply;
        LIST_FOR_EACH_POP (reply, list_node, &ovs_replies) {
            ofpmsg_update_length(reply);
            return reply;
        }
    }

   return reply;
}


BOOST_FIXTURE_TEST_CASE(useIntConnectionAlone, StatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    statsManager.registerConnection(&integrationPortConn, NULL);
    statsManager.start();

    statsManager.Handle(NULL, OFPTYPE_PORT_STATS_REPLY, NULL);
    statsManager.Handle(&integrationPortConn, OFPTYPE_PORT_STATS_REPLY, NULL);

    int dummy_stats[6] = { 1, 2, 3, 4, 5, 6 };

    ofp_port_t port_num=1;
    struct ofpbuf *res_msg = makeStatResponseMessage(&integrationPortConn,
                            dummy_stats, port_num);
    BOOST_REQUIRE(res_msg!=0);

    statsManager.Handle(&integrationPortConn,
                          OFPTYPE_PORT_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);
    verifyCounters(dummy_stats, port_num);
    statsManager.stop();
}


void StatsManagerFixture::verifyCounters(int *dummy_stats, int port_num) {
    EndpointManager& epMgr = agent.getEndpointManager();
    std::unordered_set<std::string> endpoints;
    const std::string& intPortName = intPortMapper.FindPort(port_num);
    epMgr.getEndpointsByIface(intPortName, endpoints);
    optional<shared_ptr<EpStatUniverse> > su =
                                EpStatUniverse::resolve(framework);
    std::vector<OF_SHARED_PTR<modelgbp::gbpe::EpCounter> > epCounters;

    for (const std::string& uuid : endpoints) { // assuming only one entry

        if (su) {
            optional<std::shared_ptr<modelgbp::gbpe::EpCounter> > myCounter =
                        su.get()->resolveGbpeEpCounter(uuid);

            if (myCounter) {
                BOOST_CHECK_EQUAL(myCounter.get()->getRxPackets().get(),
                       dummy_stats[0]);
                BOOST_CHECK_EQUAL(myCounter.get()->getTxPackets().get(),
                       dummy_stats[1]);
                BOOST_CHECK_EQUAL(myCounter.get()->getRxBytes().get(),
                       dummy_stats[2]);
                BOOST_CHECK_EQUAL(myCounter.get()->getTxBytes().get(),
                       dummy_stats[3]);
                BOOST_CHECK_EQUAL(myCounter.get()->getRxDrop().get(),
                       dummy_stats[4]);
                BOOST_CHECK_EQUAL(myCounter.get()->getTxDrop().get(),
                       dummy_stats[5]);
            }
            break;
        }
    }
}

BOOST_FIXTURE_TEST_CASE(useBothConnections, StatsManagerFixture) {

    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    MockConnection accessPortConn(TEST_CONN_TYPE_ACC);
    statsManager.registerConnection(&integrationPortConn, &accessPortConn);

    // see how it behaves if started and stopped.
    statsManager.start();
    statsManager.stop();

    statsManager.start();

    ofp_port_t port_num = 1;
    int dummy_stats_1[6] = { 1, 2, 3, 4, 5, 6 };
    struct ofpbuf *res_msg_1 = makeStatResponseMessage(&integrationPortConn,
                              dummy_stats_1, port_num);
    BOOST_REQUIRE(res_msg_1 != 0);
    statsManager.Handle(&integrationPortConn, OFPTYPE_PORT_STATS_REPLY,
                            res_msg_1);
    ofpbuf_delete(res_msg_1);

    int dummy_stats_2[6] = { 10, 20, 30, 40, 50, 60 };
    struct ofpbuf *res_msg_2 = makeStatResponseMessage(&accessPortConn,
                              dummy_stats_2, port_num);
    BOOST_REQUIRE(res_msg_2 != 0);
    statsManager.Handle(&accessPortConn, OFPTYPE_PORT_STATS_REPLY, res_msg_2);
    ofpbuf_delete(res_msg_2);
    // add drop counters of both the bridges before verifying them
    dummy_stats_2[4] += dummy_stats_1[4];
    dummy_stats_2[5] += dummy_stats_1[5];
    verifyCounters(dummy_stats_2, port_num);
    statsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}

