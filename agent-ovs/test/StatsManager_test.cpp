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

// borrowed code from PortMapper_test
class MockConnection : public SwitchConnection {
public:
    MockConnection(int conn_type) : SwitchConnection((conn_type==TEST_CONN_TYPE_INT)? "int_conn": "acc_conn"), lastSentMsg(NULL) {
       nType = conn_type;
       lastSentMsg = NULL;
    }
    ~MockConnection() {
        if (lastSentMsg) ofpbuf_delete(lastSentMsg);
    }
    int SendMessage(struct ofpbuf *msg) { // will be useful in case we decide to do more tests specific to msg generated ...
       switch(nType) {
       case TEST_CONN_TYPE_ACC:
       case TEST_CONN_TYPE_INT:
       default:
          BOOST_CHECK(msg != NULL);
          ofp_header *hdr = (ofp_header *)msg->data;
          ofptype typ;
          ofptype_decode(&typ, hdr);
          BOOST_CHECK(typ == OFPTYPE_PORT_STATS_REQUEST); // or ? OFPTYPE_PORT_DESC_STATS_REQUEST
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
      StatsManagerFixture() : statsManager(&agent, intPortMapper, accessPortMapper, 10) {
      }
   virtual ~StatsManagerFixture() {}

   StatsManager statsManager;
   MockPortMapper intPortMapper;
   MockPortMapper accessPortMapper;

private:
};

BOOST_AUTO_TEST_SUITE(StatsManager_test)

BOOST_FIXTURE_TEST_CASE(startAndStopBeforeInitialization,StatsManagerFixture) {
   BOOST_REQUIRE_NO_THROW(statsManager.stop());
   BOOST_REQUIRE_NO_THROW(statsManager.start());
   BOOST_REQUIRE_NO_THROW(statsManager.stop());
}

BOOST_FIXTURE_TEST_CASE(registerUsingVariousCombinations,StatsManagerFixture) {
   BOOST_REQUIRE_NO_THROW(statsManager.registerConnection(NULL,NULL));

   MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
   BOOST_REQUIRE_NO_THROW(statsManager.registerConnection(&integrationPortConn,NULL));

   MockConnection accessPortConn(TEST_CONN_TYPE_ACC);
   BOOST_REQUIRE_NO_THROW(statsManager.registerConnection(NULL, &accessPortConn));

   BOOST_REQUIRE_NO_THROW(statsManager.registerConnection(&integrationPortConn, &accessPortConn));
   BOOST_REQUIRE_NO_THROW(statsManager.stop()); // stop before starting
}

struct ofpbuf *makeStatResponseMessage(MockConnection *pConn, int stats[], ofp_port_t port_num)
{
   struct ofpbuf *req_msg = ofputil_encode_dump_ports_request(
        (ofp_version)pConn->GetProtocolVersion(), OFPP_ANY);

   struct ofp_header *req_hdr = (ofp_header *)req_msg->data;

   ovs_list ovs_replies;
   struct ofpbuf *reply = 0;

   ofputil_decode_port_stats_request(req_hdr, &port_num);
   ofpmp_init(&ovs_replies, req_hdr);

   { // this is in place of append_port_stat
      struct ofputil_port_stats ops = { .port_no = port_num };
      ops.stats.tx_packets = stats[0];
      ops.stats.rx_packets = stats[1];
      ops.stats.tx_bytes = stats[2];
      ops.stats.rx_bytes = stats[3];
      ops.stats.tx_dropped = stats[4];
      ops.stats.rx_dropped = stats[5];

      ofputil_append_port_stat(&ovs_replies, &ops);
   }

   { // this is in place of ofconn_send_replies
      struct ofpbuf *next;

      LIST_FOR_EACH_SAFE (reply, next, list_node, &ovs_replies) {

/* compilation error on list_remove - going to cause memory leak
        list_remove(&reply->list_node);
 */
        break;
      }
   }

   return reply;
}

BOOST_FIXTURE_TEST_CASE(useIntConnectionAlone,StatsManagerFixture) {

   MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
   BOOST_REQUIRE_NO_THROW(statsManager.registerConnection(&integrationPortConn,NULL));
   BOOST_REQUIRE_NO_THROW(statsManager.start());

   BOOST_REQUIRE_NO_THROW(statsManager.Handle(NULL,OFPTYPE_PORT_STATS_REPLY,NULL));
   BOOST_REQUIRE_NO_THROW(statsManager.Handle(&integrationPortConn,OFPTYPE_PORT_STATS_REPLY,NULL));

   int dummy_stats[8] = { 1, 2, 3, 4, 5, 6, 0, 0 }; // only 6 values are currently used.

   ofp_port_t port_num=1;
   struct ofpbuf *res_msg = makeStatResponseMessage(&integrationPortConn, dummy_stats, port_num);
   BOOST_REQUIRE(res_msg!=0);
   BOOST_REQUIRE_NO_THROW(statsManager.Handle(&integrationPortConn,OFPTYPE_PORT_STATS_REPLY,res_msg));
   BOOST_REQUIRE_NO_THROW(statsManager.stop());
}

BOOST_FIXTURE_TEST_CASE(useBothConnections,StatsManagerFixture) {

   MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
   MockConnection accessPortConn(TEST_CONN_TYPE_ACC);
   statsManager.registerConnection(&integrationPortConn, &accessPortConn);

   BOOST_REQUIRE_NO_THROW(statsManager.start()); // see how it behaves if started and stopped.
   BOOST_REQUIRE_NO_THROW(statsManager.stop());


   BOOST_REQUIRE_NO_THROW(statsManager.start());

   ofp_port_t port_num=1;
   int dummy_stats_1[8] = { 1, 2, 3, 4, 5, 6, 0, 0 }; // only 6 values are used.
   struct ofpbuf *res_msg_1 = makeStatResponseMessage(&integrationPortConn, dummy_stats_1,port_num);
   BOOST_REQUIRE(res_msg_1!=0);
   BOOST_REQUIRE_NO_THROW(statsManager.Handle(&integrationPortConn,OFPTYPE_PORT_STATS_REPLY,res_msg_1));

   int dummy_stats_2[8] = { 10, 20, 30, 40, 50, 60, 0, 0 }; // only 6 values are used.
   struct ofpbuf *res_msg_2 = makeStatResponseMessage(&accessPortConn, dummy_stats_2,port_num);
   BOOST_REQUIRE(res_msg_2!=0);
   BOOST_REQUIRE_NO_THROW(statsManager.Handle(&accessPortConn,OFPTYPE_PORT_STATS_REPLY,res_msg_2));
   BOOST_REQUIRE_NO_THROW(statsManager.stop());
}

BOOST_AUTO_TEST_SUITE_END()

}

