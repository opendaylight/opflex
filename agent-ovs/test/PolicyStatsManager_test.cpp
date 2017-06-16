/*
 * Test suite for class PolicyStatsManager.
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
#include "IntFlowManager.h"
#include "PolicyStatsManager.h"
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

extern "C" {
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>
}

using namespace boost::assign;
using boost::optional;
using std::shared_ptr;
using std::string;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;
using opflex::modb::class_id_t;

namespace ovsagent {

enum {
    TEST_CONN_TYPE_INT=0,
    TEST_CONN_TYPE_ACC,
};

static const uint32_t PACKET_SIZE = 64;
static const uint32_t INITIAL_PACKET_COUNT = 100;
static const uint32_t FINAL_PACKET_COUNT = 299;
static const uint32_t LAST_PACKET_COUNT = 379; // for removed flow entry

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

class PolicyStatsManagerFixture : public FlowManagerFixture {

public:
    PolicyStatsManagerFixture() : FlowManagerFixture(),
                                  policyStatsManager(&agent, idGen,
                                                     switchManager, 10),
                                  policyManager(agent.getPolicyManager()) {
        createObjects();
        createPolicyObjects();
        idGen.initNamespace("l24classifierRule");
        idGen.initNamespace("routingDomain");
        switchManager.setMaxFlowTables(10);
    }
    virtual ~PolicyStatsManagerFixture() {}
    void verifyFlowStats(shared_ptr<EpGroup> srcEpg,
                         shared_ptr<EpGroup> dstEpg,
                         shared_ptr<L24Classifier> classifier,
                         uint32_t packet_count,
                         uint32_t byte_count);
    void verifyRoutingDomainDropStats(shared_ptr<RoutingDomain> rd,
                                      uint32_t packet_count,
                                      uint32_t byte_count);
    void writeClassifierFlows(FlowEntryList& entryList);

    IdGenerator idGen;
    PolicyStatsManager policyStatsManager;
    PolicyManager& policyManager;

private:
};

void PolicyStatsManagerFixture::
verifyFlowStats(shared_ptr<EpGroup> srcEpg,
                shared_ptr<EpGroup> dstEpg,
                shared_ptr<L24Classifier> classifier,
                uint32_t packet_count,
                uint32_t byte_count) {

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    auto uuid =
        boost::lexical_cast<std::string>(policyStatsManager.getAgentUUID());
    optional<shared_ptr<L24ClassifierCounter> > myCounter =
        su.get()-> resolveGbpeL24ClassifierCounter(uuid,
                                                   policyStatsManager
                                                   .getCurrClsfrGenId(),
                                                   srcEpg->getURI().toString(),
                                                   dstEpg->getURI().toString(),
                                                   classifier->getURI()
                                                   .toString());
    if (myCounter) {
        BOOST_CHECK_EQUAL(myCounter.get()->getPackets().get(),
                          packet_count);
        BOOST_CHECK_EQUAL(myCounter.get()->getBytes().get(),
                          byte_count);
    }
}

void PolicyStatsManagerFixture::
verifyRoutingDomainDropStats(shared_ptr<RoutingDomain> rd,
                             uint32_t packet_count,
                             uint32_t byte_count) {

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    auto uuid = boost::lexical_cast<string>(policyStatsManager.getAgentUUID());
    optional<shared_ptr<RoutingDomainDropCounter> > myCounter =
        su.get()->resolveGbpeRoutingDomainDropCounter(uuid,
                                                      policyStatsManager
                                                      .getCurrDropGenId(),
                                                      rd->getURI().toString());
    if (myCounter) {
        BOOST_CHECK_EQUAL(myCounter.get()->getPackets().get(),
                          packet_count);
        BOOST_CHECK_EQUAL(myCounter.get()->getBytes().get(),
                          byte_count);
    }
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
        fs->table_id = IntFlowManager::POL_TABLE_ID;
        fs->priority = priority;
        fs->cookie = ovs_htonll((uint64_t)cookie);
        fs->packet_count = packet_count;
        fs->byte_count = byte_count;
        fs->flags = OFPUTIL_FF_SEND_FLOW_REM;
        // set match registers reg0, reg2
        match_set_reg(&(fs->match), 0 /* REG0 */, reg0);
        match_set_reg(&(fs->match), 2 /* REG2 */, reg2);
        match_set_reg(&(fs->match), 6 /* REG6 */, reg6);

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
                                          FlowEntryList& entryList) {

    struct ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(struct ofputil_flow_stats_request));
    fsr.table_id = IntFlowManager::POL_TABLE_ID;
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
        fs->table_id = IntFlowManager::POL_TABLE_ID;
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
                                        FlowEntryList& entryList) {

    struct ofpbuf *bufp;
    struct ofputil_flow_removed *fs, fstat;

    // generate flow removed message for the first flow
    // found in the flow entry list
    for (const FlowEntryPtr& fe : entryList) {

        fs = &fstat;
        bzero(fs, sizeof(struct ofputil_flow_removed));
        fs->table_id = IntFlowManager::POL_TABLE_ID;
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

void PolicyStatsManagerFixture::
writeClassifierFlows(FlowEntryList& entryList) {
    WAIT_FOR(policyManager.getVnidForGroup(epg1->getURI()), 500);
    WAIT_FOR(policyManager.getVnidForGroup(epg2->getURI()), 500);

    //FlowEntryList entryList;
    struct ofputil_flow_mod fm;
    enum ofputil_protocol prots;
    const string& classifier3Id = classifier3->getURI().toString();
    uint32_t cookie =
        idGen.getId(IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID),
                    classifier3->getURI().toString());
    uint32_t priority = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    uint32_t epg1_vnid = policyManager.getVnidForGroup(epg1->getURI()).get();
    uint32_t epg2_vnid = policyManager.getVnidForGroup(epg2->getURI()).get();

    MaskList ml_80_85 = list_of<Mask>(0x0050, 0xfffc)(0x0054, 0xfffe);
    MaskList ml_66_69 = list_of<Mask>(0x0042, 0xfffe)(0x0044, 0xfffe);
    MaskList ml_94_95 = list_of<Mask>(0x005e, 0xfffe);

    for (const Mask& mk : ml_80_85) {
        const string &flowMod =
            Bldr("", SEND_FLOW_REM).table(IntFlowManager::POL_TABLE_ID)
            .priority(priority)
            .cookie(cookie).tcp()
            .reg(SEPG, epg1_vnid).reg(DEPG, epg2_vnid)
            .isTpDst(mk.first, mk.second).actions().done();
        //.isTpDst(mk.first, mk.second).actions().drop().done();
        char* error =
            parse_ofp_flow_mod_str(&fm, flowMod.c_str(), OFPFC_ADD, &prots);
        if (error) {
            LOG(ERROR) << "Could not parse: " << flowMod << ": " << error;
            return;
        }
        //LOG(ERROR) << "writeClassifierFlows" << flowMod.c_str();
        FlowEntryPtr e(new FlowEntry());
        e->entry->match = fm.match;
        e->entry->cookie = fm.new_cookie;
        e->entry->table_id = fm.table_id;
        e->entry->priority = fm.priority;
        e->entry->idle_timeout = fm.idle_timeout;
        e->entry->hard_timeout = fm.hard_timeout;
        e->entry->ofpacts = fm.ofpacts;
        fm.ofpacts = NULL;
        e->entry->ofpacts_len = fm.ofpacts_len;
        e->entry->flags = fm.flags;
        entryList.push_back(e);
    }
    // make a copy of entryList since writeFlow will clear it
    FlowEntryList entryListCopy(entryList);
    switchManager.writeFlow(classifier3Id,
                            IntFlowManager::POL_TABLE_ID,
                            entryListCopy);
}

BOOST_AUTO_TEST_SUITE(PolicyStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testFlowMatchStats, PolicyStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    policyStatsManager.BaseStatsManager::registerConnection(&integrationPortConn);
    policyStatsManager.start();

    policyStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    policyStatsManager.Handle(&integrationPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, NULL);

    // add flows in switchManager
    FlowEntryList entryList;
    PolicyStatsManagerFixture::writeClassifierFlows(entryList);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    policyStatsManager.on_timer(ec);

    // create first flow stats reply message
    struct ofpbuf *res_msg = makeFlowStatReplyMessage_2(&integrationPortConn,
                                                        INITIAL_PACKET_COUNT,
                                                        entryList);
    LOG(DEBUG) << "1 makeFlowStatReplyMessage successful";
    BOOST_REQUIRE(res_msg!=0);

    // send first flow stats reply message
    policyStatsManager.Handle(&integrationPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handling successful";
    ofpbuf_delete(res_msg);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&integrationPortConn,
                                         FINAL_PACKET_COUNT,
                                         entryList);
    // send second flow stats reply message
    policyStatsManager.Handle(&integrationPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    policyStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    uint32_t num_flows = entryList.size();
    uint32_t exp_classifier_packet_count =
        (FINAL_PACKET_COUNT - INITIAL_PACKET_COUNT) * num_flows;

    // Verify per classifier/per epg pair packet and byte count

    verifyFlowStats(epg1, epg2, classifier3,
                    exp_classifier_packet_count,
                    exp_classifier_packet_count * PACKET_SIZE);

    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";

    policyStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testRdDropStats, PolicyStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    policyStatsManager.BaseStatsManager::registerConnection(&integrationPortConn);
    policyStatsManager.start();

    // get rdId
    uint32_t rdId =
        idGen.getId(IntFlowManager::getIdNamespace(RoutingDomain::CLASS_ID),
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
    LOG(DEBUG) << "testRd:FlowStatsReplyMessage handling successful";

    verifyRoutingDomainDropStats(rd0, packet_count, byte_count);
    LOG(DEBUG) << "testRd:FlowStatsReplyMessage verification successful";
    policyStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemoved, PolicyStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    policyStatsManager.BaseStatsManager::registerConnection(&integrationPortConn);
    policyStatsManager.start();

    // Add flows in switchManager
    FlowEntryList entryList;
    PolicyStatsManagerFixture::writeClassifierFlows(entryList);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    policyStatsManager.on_timer(ec);

    struct ofpbuf *res_msg = makeFlowRemovedMessage_2(&integrationPortConn,
                                                      LAST_PACKET_COUNT,
                                                      entryList);
    BOOST_REQUIRE(res_msg!=0);

    policyStatsManager.Handle(&integrationPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    policyStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    uint32_t num_flows = entryList.size();
    verifyFlowStats(epg1, epg2, classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE);
    policyStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
