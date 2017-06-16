/*
 * Test suite for class AccStatsManager.
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
#include <boost/lexical_cast.hpp>

#include "logging.h"
#include "ModbFixture.h"
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "AccessFlowManager.h"
#include "IntFlowManager.h"
#include "AccStatsManager.h"
#include "SwitchConnection.h"
#include "MockSwitchManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "FlowConstants.h"
#include "FlowUtils.h"
#include "FlowManagerFixture.h"
#include "FlowBuilder.h"

#include "ovs-ofputil.h"
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <modelgbp/gbpe/HPPClassifierCounter.hpp>
#include <modelgbp/observer/PolicyStatUniverse.hpp>

extern "C" {
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>
}

using namespace boost::assign;
using boost::optional;
using std::shared_ptr;
using std::string;
using opflex::modb::URI;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;
using opflex::modb::class_id_t;

typedef ovsagent::EndpointListener::uri_set_t uri_set_t;

namespace ovsagent {

enum {
    TEST_CONN_TYPE_INT=0,
    TEST_CONN_TYPE_ACC,
};

static const uint32_t PACKET_SIZE = 64;
static const uint32_t INITIAL_PACKET_COUNT = 100;
static const uint32_t FINAL_PACKET_COUNT = 299;
static const uint32_t LAST_PACKET_COUNT = 379; // for removed flow entry
static string getSecGrpSetId(const uri_set_t& secGrps) {
   std::stringstream ss;
   bool notfirst = false;
   for (const URI& uri : secGrps) {
       if (notfirst) ss << ",";
       notfirst = true;
       ss << uri.toString();
   }
   return ss.str();
}

class MockConnection : public SwitchConnection {
public:
    MockConnection(int conn_type) :
        SwitchConnection((conn_type== TEST_CONN_TYPE_INT)
                         ? "int_conn": "acc_conn"), lastSentMsg(NULL) {
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
            BOOST_CHECK(typ == OFPTYPE_FLOW_STATS_REQUEST);
            if (msg) ofpbuf_delete(msg);
            break;
        }
        return 0;
    }

    ofpbuf *lastSentMsg;
private:
    int nType;
};

class AccStatsManagerFixture : public FlowManagerFixture {

public:
    AccStatsManagerFixture() : FlowManagerFixture(),
                                  accStatsManager(&agent, idGen,
                                                     switchManager, 10),
                                  policyManager(agent.getPolicyManager()) {
        createObjects();
        createPolicyObjects();
        idGen.initNamespace("l24classifierRule");
        idGen.initNamespace("secGroupSet");
        idGen.initNamespace("secGroup");
        switchManager.setMaxFlowTables(10);
    }
    virtual ~AccStatsManagerFixture() {}
    void verifyFlowStats(shared_ptr<L24Classifier> classifier,
                         uint32_t packet_count,
                         uint32_t byte_count,uint32_t table_id);
    void writeClassifierFlows(FlowEntryList& entryList,uint32_t table_id,uint32_t cookie);

    IdGenerator idGen;
    AccStatsManager accStatsManager;
    PolicyManager& policyManager;

private:
};

void AccStatsManagerFixture::
verifyFlowStats(shared_ptr<L24Classifier> classifier,
                uint32_t packet_count,
                uint32_t byte_count,uint32_t table_id) {

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    auto uuid =
        boost::lexical_cast<std::string>(accStatsManager.getAgentUUID());
    optional<shared_ptr<HPPClassifierCounter> > myCounter =
        su.get()-> resolveGbpeHPPClassifierCounter(uuid,
                                                   accStatsManager
                                                   .getCurrClsfrGenId(),
                                                   classifier->getURI()
                                                   .toString());
    if (myCounter) {
        if (table_id == AccessFlowManager::SEC_GROUP_IN_TABLE_ID) {
            BOOST_CHECK_EQUAL(myCounter.get()->getTxpackets().get(),
                          packet_count);
            BOOST_CHECK_EQUAL(myCounter.get()->getTxbytes().get(),
                          byte_count);
        } else {
            BOOST_CHECK_EQUAL(myCounter.get()->getRxpackets().get(),
                          packet_count);
            BOOST_CHECK_EQUAL(myCounter.get()->getRxbytes().get(),
                          byte_count);
        }
    }
}


struct ofpbuf *makeFlowStatReplyMessage(MockConnection *pConn,
                                        uint32_t priority, uint32_t cookie,
                                        uint32_t packet_count,
                                        uint32_t byte_count,
                                        uint32_t reg0,
                                        uint32_t table_id) {

    struct ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(struct ofputil_flow_stats_request));
    fsr.table_id = table_id;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;
    enum ofputil_protocol proto =
        ofputil_protocol_from_ofp_version((ofp_version)OFP13_VERSION);
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
        fs->table_id = table_id;
        fs->priority = priority;
        fs->cookie = ovs_htonll((uint64_t)cookie);
        fs->packet_count = packet_count;
        fs->byte_count = byte_count;
        fs->flags = OFPUTIL_FF_SEND_FLOW_REM;
        // set match registers reg0, reg2
        match_set_reg(&(fs->match), 0 /* REG0 */, reg0);

        ofputil_append_flow_stats_reply(fs, &ovs_replies);
        reply = ofpbuf_from_list(ovs_list_back(&ovs_replies));
        ofpmsg_update_length(reply);
        // set it to be OFPRAW_ type of openflow message by setting
        // header to be null.
        reply->header = NULL;
        return reply;
    }

}

struct ofpbuf *makeFlowStatReplyMessage_2(MockConnection *pConn,
                                          uint32_t packet_count,
                                          uint32_t table_id,
                                          FlowEntryList& entryList) {

    struct ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(struct ofputil_flow_stats_request));
    fsr.table_id = table_id;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;
    enum ofputil_protocol proto =
        ofputil_protocol_from_ofp_version((ofp_version)OFP13_VERSION);
    struct ofpbuf *req_msg = ofputil_encode_flow_stats_request(&fsr, proto);

    struct ofp_header *req_hdr = (ofp_header *)req_msg->data;

    ovs_list ovs_replies;
    struct ofpbuf *reply = 0;

    ofpmp_init(&ovs_replies, req_hdr);
    ofpbuf_delete(req_msg);
    for (const FlowEntryPtr& fe : entryList) {
        struct ofputil_flow_stats *fs, fstat;

        fs = &fstat;
        bzero(fs, sizeof(struct ofputil_flow_stats));
        fs->table_id = table_id;
        fs->priority = fe->entry->priority;
#if 0
        LOG(DEBUG) << "COOKIE: " << fe->entry->cookie
                   << " " << ovs_htonll(fe->entry->cookie);
#endif
        fs->cookie = fe->entry->cookie;
        fs->packet_count = packet_count;
        fs->byte_count = PACKET_SIZE * (fs->packet_count);
        fs->flags = fe->entry->flags;
        fs->match = fe->entry->match;

        ofputil_append_flow_stats_reply(fs, &ovs_replies);
    }
    reply = ofpbuf_from_list(ovs_list_back(&ovs_replies));
    ofpmsg_update_length(reply);
    // set it to be OFPRAW_ type of openflow message by setting header
    // to be null.
    reply->header = NULL;
    return reply;
}

struct ofpbuf *makeFlowRemovedMessage_2(MockConnection *pConn,
                                        uint32_t packet_count,
                                        uint32_t table_id,
                                        FlowEntryList& entryList) {

    struct ofpbuf *bufp;
    struct ofputil_flow_removed *fs, fstat;

    // generate flow removed message for the first flow
    // found in the flow entry list
    for (const FlowEntryPtr& fe : entryList) {

        fs = &fstat;
        bzero(fs, sizeof(struct ofputil_flow_removed));
        fs->table_id = table_id;
        fs->priority = fe->entry->priority;
        fs->cookie = fe->entry->cookie;
        fs->packet_count = packet_count;
        fs->byte_count = PACKET_SIZE * (fs->packet_count);
        fs->match = fe->entry->match;
        enum ofputil_protocol proto =
            ofputil_protocol_from_ofp_version((ofp_version)OFP13_VERSION);
        bufp = ofputil_encode_flow_removed(fs, proto);
        ofpmsg_update_length(bufp);

        return bufp;
    }
}

void AccStatsManagerFixture::
writeClassifierFlows(FlowEntryList& entryList,uint32_t table_id,uint32_t cookie) {

    //FlowEntryList entryList;
    struct ofputil_flow_mod fm;
    enum ofputil_protocol prots;
    uint32_t secGrpSetId = idGen.getId("secGroupSet",
                                       getSecGrpSetId(ep0->getSecurityGroups()));
    uint32_t priority = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    {
        FlowBuilder().cookie(cookie).inPort(1).table(table_id).priority(priority).reg(SEPG,secGrpSetId).build(entryList);
        FlowBuilder().cookie(cookie).inPort(2).table(table_id).priority(priority).reg(SEPG, secGrpSetId).build(entryList);
        FlowEntryList entryListCopy(entryList);
        switchManager.writeFlow("test", table_id,
                                entryListCopy);
    }
}

BOOST_AUTO_TEST_SUITE(AccStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testFlowMatchStats, AccStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    accStatsManager.BaseStatsManager::registerConnection(&accPortConn);
    accStatsManager.start();

    accStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, NULL);

    // add flows in switchManager
    FlowEntryList entryList;
    AccStatsManagerFixture::writeClassifierFlows(entryList,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,1);
    AccStatsManagerFixture::writeClassifierFlows(entryList,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,2);
    FlowEntryList entryList1;
    AccStatsManagerFixture::writeClassifierFlows(entryList1,AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,1);
    AccStatsManagerFixture::writeClassifierFlows(entryList1,AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,2);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    accStatsManager.on_timer(ec);

    // create first flow stats reply message
    struct ofpbuf *res_msg = makeFlowStatReplyMessage_2(&accPortConn,
                                                        INITIAL_PACKET_COUNT,
                                                        AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                                        entryList);
    LOG(DEBUG) << "1 makeFlowStatReplyMessage successful";
    BOOST_REQUIRE(res_msg!=0);

    // send first flow stats reply message
    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handling successful";
    ofpbuf_delete(res_msg);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&accPortConn,
                                         FINAL_PACKET_COUNT,
                                         AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                         entryList);
    // send second flow stats reply message
    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);
    res_msg = makeFlowStatReplyMessage_2(&accPortConn,
                                                        INITIAL_PACKET_COUNT,
                                                        AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                                        entryList1);
    LOG(DEBUG) << "1 makeFlowStatReplyMessage successful";
    BOOST_REQUIRE(res_msg!=0);

    // send first flow stats reply message
    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handling successful";
    ofpbuf_delete(res_msg);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&accPortConn,
                                         FINAL_PACKET_COUNT,
                                         AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                         entryList1);
    // send second flow stats reply message
    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);
    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    accStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    uint32_t num_flows = entryList.size();
    uint32_t exp_classifier_packet_count =
        (FINAL_PACKET_COUNT - INITIAL_PACKET_COUNT) * num_flows;

    // Verify per classifier/per epg pair packet and byte count

    verifyFlowStats(classifier3,
                    exp_classifier_packet_count,
                    exp_classifier_packet_count * PACKET_SIZE,
                    AccessFlowManager::SEC_GROUP_IN_TABLE_ID);
    verifyFlowStats(classifier3,
                    exp_classifier_packet_count,
                    exp_classifier_packet_count * PACKET_SIZE,
                    AccessFlowManager::SEC_GROUP_OUT_TABLE_ID);

    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";

    accStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemoved, AccStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    accStatsManager.BaseStatsManager::registerConnection(&accPortConn);
    accStatsManager.start();

    // Add flows in switchManager
    FlowEntryList entryList;
    AccStatsManagerFixture::writeClassifierFlows(entryList,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,1);
    FlowEntryList entryList1;
    AccStatsManagerFixture::writeClassifierFlows(entryList1,AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,1);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    accStatsManager.on_timer(ec);

    struct ofpbuf *res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                                      LAST_PACKET_COUNT, 
                                                      AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                                      entryList);
    BOOST_REQUIRE(res_msg!=0);

    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    AccStatsManagerFixture::writeClassifierFlows(entryList,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,2);
    accStatsManager.on_timer(ec);
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                                      LAST_PACKET_COUNT, 
                                                      AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                                      entryList);
    BOOST_REQUIRE(res_msg!=0);

    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                                      LAST_PACKET_COUNT, 
                                                      AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                                      entryList1);
    BOOST_REQUIRE(res_msg!=0);

    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                                      LAST_PACKET_COUNT, 
                                                      AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                                      entryList1);
    BOOST_REQUIRE(res_msg!=0);

    accStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    accStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    uint32_t num_flows = entryList.size();
    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    AccessFlowManager::SEC_GROUP_IN_TABLE_ID);
    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    AccessFlowManager::SEC_GROUP_OUT_TABLE_ID);
    accStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
