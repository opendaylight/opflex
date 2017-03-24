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
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "IntFlowManager.h"
#include "PolicyStatsManager.h"
#include "SwitchConnection.h"

using boost::optional;
using std::shared_ptr;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;
using opflex::modb::class_id_t;

extern int oxm_put_match(struct ofpbuf *b, const struct match *match,
              enum ofp_version version);

namespace ovsagent {

enum {
    TEST_CONN_TYPE_INT=0,
    TEST_CONN_TYPE_ACC,
};


static const char* ID_NAMESPACES[] =
    {"floodDomain", "bridgeDomain", "routingDomain",
     "contract", "externalNetwork", "service", "l24classifierRule"};

static const char* ID_NMSPC_FD            = ID_NAMESPACES[0];
static const char* ID_NMSPC_BD            = ID_NAMESPACES[1];
static const char* ID_NMSPC_RD            = ID_NAMESPACES[2];
static const char* ID_NMSPC_CON           = ID_NAMESPACES[3];
static const char* ID_NMSPC_EXTNET        = ID_NAMESPACES[4];
static const char* ID_NMSPC_SERVICE       = ID_NAMESPACES[5];
static const char* ID_NMSPC_L24CLASS_RULE = ID_NAMESPACES[6];


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

class PolicyStatsManagerFixture : public ModbFixture {

public:
    PolicyStatsManagerFixture() : policyStatsManager(&agent, idGen, 10),
                                  policyManager(agent.getPolicyManager()) {
        createObjects();
        createPolicyObjects();
        for (size_t i = 0; i < sizeof(ID_NAMESPACES)/sizeof(char*); i++) {
            idGen.initNamespace(ID_NAMESPACES[i]);
        }


      }
    virtual ~PolicyStatsManagerFixture() {}
    void verifyFlowStats(
                     std::shared_ptr<modelgbp::gbp::EpGroup> srcEpg,
                     std::shared_ptr<modelgbp::gbp::EpGroup> dstEpg,
                     std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier,
                     uint32_t packet_count,
                     uint32_t byte_count);
    void verifyRoutingDomainDropStats(
                         std::shared_ptr<modelgbp::gbp::RoutingDomain> rd,
                         uint32_t packet_count,
                         uint32_t byte_count);

    IdGenerator idGen;
    PolicyStatsManager policyStatsManager;
    PolicyManager& policyManager;

private:
};



const char * getIdNamespace(class_id_t cid) {
    const char *nmspc = NULL;
    switch (cid) {
    case RoutingDomain::CLASS_ID:   nmspc = ID_NMSPC_RD; break;
    case BridgeDomain::CLASS_ID:    nmspc = ID_NMSPC_BD; break;
    case FloodDomain::CLASS_ID:     nmspc = ID_NMSPC_FD; break;
    case Contract::CLASS_ID:        nmspc = ID_NMSPC_CON; break;
    case L3ExternalNetwork::CLASS_ID: nmspc = ID_NMSPC_EXTNET; break;
    case L24Classifier::CLASS_ID: nmspc = ID_NMSPC_L24CLASS_RULE; break;
    default:
        assert(false);
    }
    return nmspc;
}

struct ofpbuf *makeFlowStatReplyMessage(MockConnection *pConn,
                                        uint32_t priority, uint32_t cookie,
                                        uint32_t packet_count,
                                        uint32_t byte_count,
                                        uint32_t reg0, uint32_t reg2,
                                        uint32_t reg6) {

    struct ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(struct ofputil_flow_stats_request));
    fsr.table_id = IntFlowManager::POL_TABLE_ID;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;
    enum ofputil_protocol proto = ofputil_protocol_from_ofp_version(
                                     (ofp_version)OFP13_VERSION);
    struct ofpbuf *req_msg = ofputil_encode_flow_stats_request(&fsr, proto);

    struct ofp_header *req_hdr = (ofp_header *)req_msg->data;

    ovs_list ovs_replies;
    struct ofpbuf *reply = 0;

    ofpmp_init(&ovs_replies, req_hdr);
    ofpbuf_delete(req_msg);
    {
        struct ofputil_flow_stats *fs, fstat;

        fs = &fstat;
        bzero(fs, sizeof(struct ofputil_flow_stats));
        fs->table_id = IntFlowManager::POL_TABLE_ID;
        fs->priority = priority;
        fs->cookie = ovs_htonll((uint64_t)cookie);
        fs->packet_count = packet_count;
        fs->byte_count = byte_count;
        // set match registers reg0, reg2
        match_set_reg(&(fs->match), 0 /* REG0 */, reg0);
        match_set_reg(&(fs->match), 2 /* REG2 */, reg2);
        match_set_reg(&(fs->match), 6 /* REG6 */, reg6);

        ofputil_append_flow_stats_reply(fs, &ovs_replies);
        reply = ofpbuf_from_list(ovs_list_back(&ovs_replies));
        ofpmsg_update_length(reply);
        // set it to be OFPRAW_ type of openflow message by setting header to be
        // null.
        reply->header = NULL;
        return reply;
    }

}


struct ofpbuf *makeFlowRemovedMessage(MockConnection *pConn,
                                        uint32_t priority, uint32_t cookie,
                                        uint32_t packet_count,
                                        uint32_t byte_count,
                                        uint32_t reg0, uint32_t reg2) {

    struct ofpbuf *bufp;
    struct ofputil_flow_removed fs;

    bzero(&fs, sizeof(struct ofputil_flow_removed));
    fs.table_id = IntFlowManager::POL_TABLE_ID;
    fs.priority = priority,
    fs.cookie = ovs_htonll((uint64_t)cookie);
    fs.packet_count = packet_count;
    fs.byte_count = byte_count;
    // set match registers reg0, reg2 
    match_set_reg(&(fs.match), 0 /* REG0 */, reg0);
    match_set_reg(&(fs.match), 2 /* REG2 */, reg2);

    enum ofputil_protocol proto = ofputil_protocol_from_ofp_version(
                                     (ofp_version)OFP13_VERSION);
    bufp = ofputil_encode_flow_removed(&fs, proto);
    ofpmsg_update_length(bufp);

    return bufp;
}

BOOST_AUTO_TEST_SUITE(PolicyStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testFlowMatchStats, PolicyStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    policyStatsManager.registerConnection(&integrationPortConn);
    policyStatsManager.start();

    policyStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    policyStatsManager.Handle(&integrationPortConn, OFPTYPE_FLOW_STATS_REPLY, NULL);

    // get the cookie, reg0 and reg2 
    boost::optional<uint32_t> reg0 = policyManager.getVnidForGroup(epg1->getURI());
    boost::optional<uint32_t> reg2 = policyManager.getVnidForGroup(epg2->getURI());
    // create a cookie in idGen database
    uint32_t cookie = idGen.getId(getIdNamespace(L24Classifier::CLASS_ID),
                        classifier3->getURI().toString());
    uint32_t priority = 100; 
    uint32_t packet_count = 98; 
    uint32_t byte_count = 6400; 

    struct ofpbuf *res_msg = makeFlowStatReplyMessage(&integrationPortConn,
                                                      priority, cookie,
                                                      packet_count, byte_count,
                                                      reg0.get(), reg2.get(), 0);
    LOG(DEBUG) << "makeFlowStatReplyMessage successful";
    BOOST_REQUIRE(res_msg!=0);

    policyStatsManager.Handle(&integrationPortConn,
                          OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);
    verifyFlowStats(epg1, epg2, classifier3,
                    packet_count, byte_count);
    policyStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testRdDropStats, PolicyStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    policyStatsManager.registerConnection(&integrationPortConn);
    policyStatsManager.start();

    // get rdId 
    uint32_t rdId = idGen.getId(getIdNamespace(RoutingDomain::CLASS_ID),
                                rd0->getURI().toString());
    uint32_t priority = 1; 
    uint32_t packet_count = 39; 
    uint32_t byte_count = 6994; 

    /* create  per RD flow drop stats  */
    struct ofpbuf *res_msg = makeFlowStatReplyMessage(&integrationPortConn,
                                                      priority, 0,
                                                      packet_count, byte_count,
                                                      0, 0, rdId);
    BOOST_REQUIRE(res_msg!=0);

    policyStatsManager.Handle(&integrationPortConn,
                          OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);

    verifyRoutingDomainDropStats(rd0, packet_count, byte_count);
    policyStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemoved, PolicyStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    policyStatsManager.registerConnection(&integrationPortConn);
    policyStatsManager.start();

    // get the cookie, reg0 and reg2 
    boost::optional<uint32_t> reg0 = policyManager.getVnidForGroup(epg1->getURI());
    boost::optional<uint32_t> reg2 = policyManager.getVnidForGroup(epg2->getURI());
    // create a cookie in idGen database
    uint32_t cookie = idGen.getId(getIdNamespace(L24Classifier::CLASS_ID),
                        classifier3->getURI().toString());
    uint32_t priority = 100; 
    uint32_t packet_count = 98; 
    uint32_t byte_count = 6400; 

    struct ofpbuf *res_msg = makeFlowStatReplyMessage(&integrationPortConn,
                                                      priority, cookie,
                                                      packet_count, byte_count,
                                                      reg0.get(), reg2.get(), 0);
    BOOST_REQUIRE(res_msg!=0);

    policyStatsManager.Handle(&integrationPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);

    uint32_t last_packet_count = 98*3; 
    uint32_t last_byte_count = 6400*3; 

    res_msg = NULL;
    res_msg = makeFlowRemovedMessage(&integrationPortConn,
                                        priority, cookie,
                                        last_packet_count, last_byte_count,
                                        reg0.get(), reg2.get());
    BOOST_REQUIRE(res_msg!=0);

    policyStatsManager.Handle(&integrationPortConn,
                               OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    verifyFlowStats(epg1, epg2, classifier3,
                    last_packet_count - packet_count,
                    last_byte_count -  byte_count);
    policyStatsManager.stop();
}


void PolicyStatsManagerFixture::verifyFlowStats(
                     std::shared_ptr<modelgbp::gbp::EpGroup> srcEpg,
                     std::shared_ptr<modelgbp::gbp::EpGroup> dstEpg,
                     std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier,
                     uint32_t packet_count,
                     uint32_t byte_count) {

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    optional<std::shared_ptr<modelgbp::gbpe::L24ClassifierCounter> > myCounter =
                        su.get()->resolveGbpeL24ClassifierCounter(
                                  srcEpg->getURI().toString(),
                                  dstEpg->getURI().toString(),
                                  classifier->getURI().toString());
    if (myCounter) {
        BOOST_CHECK_EQUAL(myCounter.get()->getPackets().get(),
                          packet_count);
        BOOST_CHECK_EQUAL(myCounter.get()->getBytes().get(),
                          byte_count);
    }
}

void PolicyStatsManagerFixture::verifyRoutingDomainDropStats(
                     std::shared_ptr<modelgbp::gbp::RoutingDomain> rd,
                     uint32_t packet_count,
                     uint32_t byte_count) {

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    optional<std::shared_ptr<modelgbp::gbpe::RoutingDomainDropCounter> > myCounter =
                        su.get()->resolveGbpeRoutingDomainDropCounter(
                                  rd->getURI().toString());
    if (myCounter) {
        BOOST_CHECK_EQUAL(myCounter.get()->getPackets().get(),
                          packet_count);
        BOOST_CHECK_EQUAL(myCounter.get()->getBytes().get(),
                          byte_count);
    }
}

BOOST_AUTO_TEST_SUITE_END()

}

