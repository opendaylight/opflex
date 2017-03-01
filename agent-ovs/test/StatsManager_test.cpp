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
        (ofp_version)OFP13_VERSION, OFPP_ANY);
//        (ofp_version)pConn->GetProtocolVersion(), OFPP_ANY);

   struct ofp_header *req_hdr = (ofp_header *)req_msg->data;

   ovs_list ovs_replies;
   struct ofpbuf *reply = 0;

   ofputil_decode_port_stats_request(req_hdr, &port_num);
   port_num=1;
   ofpmp_init(&ovs_replies, req_hdr);

   { // this is in place of append_port_stat
      struct ofputil_port_stats ops = { .port_no = port_num };
      ops.stats.rx_packets = stats[0];
      ops.stats.tx_packets = stats[1];
      ops.stats.rx_bytes   = stats[2];
      ops.stats.tx_bytes   = stats[3];
      ops.stats.rx_dropped = stats[4];
      ops.stats.tx_dropped = stats[5];

      ofputil_append_port_stat(&ovs_replies, &ops);
   }

   { // this is in place of ofconn_send_replies
      struct ofpbuf *next;

      LIST_FOR_EACH_SAFE (reply, next, list_node, &ovs_replies) {

/* compilation error on list_remove - going to cause memory leak
        list_remove(&reply->list_node);
 */
        return reply;
      }
   }

   return reply;
}
/*
int
ofputil_decode_port_stats1(struct ofputil_port_stats *ps, struct ofpbuf *msg)
{
    enum ofperr error;
    enum ofpraw raw;

    memset(&(ps->stats), 0xFF, sizeof (ps->stats));

    error = (msg->header ? ofpraw_decode(&raw, msg->header)
             : ofpraw_pull(&raw, msg));
    if (error) {
        return error;
    }

    if (!msg->size) {
        return EOF;
    } else if (raw == OFPRAW_OFPST14_PORT_REPLY) {
        return ofputil_pull_ofp14_port_stats(ps, msg);
    } else if (raw == OFPRAW_OFPST13_PORT_REPLY) {
        const struct ofp13_port_stats *ps13;
        ps13 = ofpbuf_try_pull(msg, sizeof *ps13);
        if (!ps13) {
            goto bad_len;
        }
        return ofputil_port_stats_from_ofp13(ps, ps13);
    } else if (raw == OFPRAW_OFPST11_PORT_REPLY) {
        const struct ofp11_port_stats *ps11;

        ps11 = ofpbuf_try_pull(msg, sizeof *ps11);
        if (!ps11) {
            goto bad_len;
        }
        return ofputil_port_stats_from_ofp11(ps, ps11);
    } else if (raw == OFPRAW_OFPST10_PORT_REPLY) {
        const struct ofp10_port_stats *ps10;

        ps10 = ofpbuf_try_pull(msg, sizeof *ps10);
        if (!ps10) {
            goto bad_len;
        }
        return ofputil_port_stats_from_ofp10(ps, ps10);
    } else {
        OVS_NOT_REACHED();
    }

 bad_len:
    VLOG_WARN_RL(&bad_ofmsg_rl, "OFPST_PORT reply has %"PRIu32" leftover "
                 "bytes at end", msg->size);
    return OFPERR_OFPBRC_BAD_LEN;
}
*/

static inline uint64_t
ntohll(ovs_be64 n)
{
    return htonl(1) == 1 ? n : ((uint64_t) ntohl(n) << 32) | ntohl(n >> 32);
}


// below code is // adopted from ofp_print_ofpst_port_reply ofp-print.c
int verifyStatsResponseMsg(struct ofpbuf *msg) {
    int retval= 0;
    if (!msg) {
        retval= -100;
        return retval;
    }
    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_port_stats ps;
    memset(&ps, '\0', sizeof(struct ofputil_port_stats));
    struct ofpbuf b;
    memset(&b, '\0', sizeof(struct ofpbuf));

    b = ofpbuf_const_initializer(oh, ntohs(oh->length));
//    retval = ofputil_decode_port_stats(&ps, &b);
//    retval = ofputil_decode_port_stats1(&ps, msg);

const struct ofp13_port_stats *ps13;
ps13 = ofpbuf_try_pull(msg, sizeof *ps13);
if (!ps13) {
    retval= -200;
    return retval;
}
//ps13->ps holds the stat
//ofputil_port_stats_from_ofp13(ps, ps13);
    ps.stats.rx_packets = ntohll(ps13->ps.rx_packets);
    ps.stats.tx_packets = ntohll(ps13->ps.tx_packets);
    ps.stats.rx_bytes = ntohll(ps13->ps.rx_bytes);
    ps.stats.tx_bytes = ntohll(ps13->ps.tx_bytes);
    ps.stats.rx_dropped = ntohll(ps13->ps.rx_dropped);
    ps.stats.tx_dropped = ntohll(ps13->ps.tx_dropped);
    ps.stats.rx_errors = ntohll(ps13->ps.rx_errors);
    ps.stats.tx_errors = ntohll(ps13->ps.tx_errors);
    ps.stats.rx_frame_errors = ntohll(ps13->ps.rx_frame_err);
    ps.stats.rx_over_errors = ntohll(ps13->ps.rx_over_err);
    ps.stats.rx_crc_errors = ntohll(ps13->ps.rx_crc_err);
    ps.stats.collisions = ntohll(ps13->ps.collisions);




    if (retval) {
        if (retval != EOF) {
            retval= -200; // parse error
        }
        return retval;
    }
    return retval;
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
//   BOOST_REQUIRE(verifyStatsResponseMsg(res_msg)==0);

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

