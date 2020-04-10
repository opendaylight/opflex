/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TEST_POLSTATSMANAGERFIXTURE_H_
#define OPFLEXAGENT_TEST_POLSTATSMANAGERFIXTURE_H_

#include <openvswitch/ofp-msgs.h>
#include <openvswitch/ofp-monitor.h>

#include <opflexagent/test/ModbFixture.h>
#include "SwitchManager.h"
#include "MockSwitchManager.h"
#include "TableState.h"
#include "PolicyStatsManager.h"
#include "CtZoneManager.h"
#include <opflexagent/logging.h>
#include "FlowBuilder.h"
#include "IntFlowManager.h"
#include "AccessFlowManager.h"
#include "FlowManagerFixture.h"

#include <boost/lexical_cast.hpp>
#include <boost/asio/io_service.hpp>

#include <vector>

using boost::optional;

using std::shared_ptr;
using std::string;
using opflex::modb::URI;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;

typedef opflexagent::EndpointListener::uri_set_t uri_set_t;

namespace opflexagent {

static const uint32_t INITIAL_PACKET_COUNT = 100;
static const uint32_t FINAL_PACKET_COUNT = 299;
static const uint32_t PACKET_SIZE = 64;

enum {
    TEST_CONN_TYPE_INT=0,
    TEST_CONN_TYPE_ACC,
};

class MockConnection : public SwitchConnection {
public:
    MockConnection(int conn_type) :
        SwitchConnection((conn_type== TEST_CONN_TYPE_INT)
                         ? "int_conn": "acc_conn") {
    }
    virtual int SendMessage(OfpBuf &msg) {
        ofp_header *hdr = (ofp_header *)msg->data;
        ofptype typ;
        ofptype_decode(&typ, hdr);
        BOOST_CHECK(typ == OFPTYPE_FLOW_STATS_REQUEST);
        msg.reset();
        return 0;
    }
};

class PolicyStatsManagerFixture : public FlowManagerFixture {
public:
    PolicyStatsManagerFixture (
            opflex_elem_t mode = opflex_elem_t::INVALID_MODE,
            bool intBridgeTableDesc = true ): FlowManagerFixture (
                    mode, intBridgeTableDesc) {
    };

    void verifyFlowStats(shared_ptr<L24Classifier> classifier,
                         uint32_t packet_count,
                         uint32_t byte_count,uint32_t table_id,
                         PolicyStatsManager *statsManager,
                         shared_ptr<EpGroup> srcEpg = NULL,
                         shared_ptr<EpGroup> dstEpg = NULL) {
        optional<shared_ptr<PolicyStatUniverse> > su =
            PolicyStatUniverse::resolve(agent.getFramework());
        if (srcEpg.get() && dstEpg.get()) {
            auto uuid =
                boost::lexical_cast<std::string>(statsManager->getAgentUUID());
            LOG(DEBUG) << "verifying stats for src_epg: " << srcEpg->getURI().toString()
                        << " dst_epg: " << dstEpg->getURI().toString()
                        << " classifier: " << classifier->getURI().toString();
            optional<shared_ptr<L24ClassifierCounter> > myCounter =
                boost::make_optional<shared_ptr<L24ClassifierCounter> >(false, nullptr);
            WAIT_FOR_DO_ONFAIL(
               (myCounter && myCounter.get()
                    && (myCounter.get()->getPackets().get() == packet_count)
                    && (myCounter.get()->getBytes().get() == byte_count)),
               500, // usleep(1000) * 500 = 500ms
               (myCounter = su.get()->resolveGbpeL24ClassifierCounter(uuid,
                                    statsManager->getCurrClsfrGenId(),
                                    srcEpg->getURI().toString(),
                                    dstEpg->getURI().toString(),
                                    classifier->getURI().toString())),
               if (myCounter && myCounter.get()) {
                   BOOST_CHECK_EQUAL(myCounter.get()->getPackets().get(), packet_count);
                   BOOST_CHECK_EQUAL(myCounter.get()->getBytes().get(), byte_count);
               } else {
                   LOG(DEBUG) << "L24classifiercounter mo isnt present";
               });
        } else {
            auto uuid =
                boost::lexical_cast<std::string>(statsManager->getAgentUUID());
            optional<shared_ptr<SecGrpClassifierCounter> > myCounter =
                boost::make_optional<shared_ptr<SecGrpClassifierCounter> >(false, nullptr);
            LOG(DEBUG) << "verifying stats for"
                        << " classifier: " << classifier->getURI().toString();
            WAIT_FOR_DO_ONFAIL(
                (myCounter && myCounter.get()
                    && (
                        ((table_id == AccessFlowManager::SEC_GROUP_OUT_TABLE_ID)
                        && (myCounter.get()->getTxpackets()
                                && (myCounter.get()->getTxpackets().get() == packet_count))
                        && (myCounter.get()->getTxbytes()
                                && (myCounter.get()->getTxbytes().get() == byte_count)))
                        ||
                        ((table_id == AccessFlowManager::SEC_GROUP_IN_TABLE_ID)
                        && (myCounter.get()->getRxpackets().get() == packet_count)
                        && (myCounter.get()->getRxbytes().get() == byte_count))
                       )),
                500, // usleep(1000) * 500 = 500ms
                (myCounter = su.get()->resolveGbpeSecGrpClassifierCounter(uuid,
                                  statsManager->getCurrClsfrGenId(),
                                  classifier->getURI().toString())),
                if (myCounter && myCounter.get()) {
                    if (table_id == AccessFlowManager::SEC_GROUP_OUT_TABLE_ID) {
                        if (myCounter.get()->getTxpackets()) {
                            BOOST_CHECK_EQUAL(myCounter.get()->getTxpackets().get(), packet_count);
                        } else {
                            LOG(DEBUG) << "tx pkts invalid";
                        }
                        if (myCounter.get()->getTxbytes()) {
                            BOOST_CHECK_EQUAL(myCounter.get()->getTxbytes().get(), byte_count);
                        } else {
                            LOG(DEBUG) << "tx bytes invalid";
                        }
                    } else if (table_id == AccessFlowManager::SEC_GROUP_IN_TABLE_ID) {
                        BOOST_CHECK_EQUAL(myCounter.get()->getRxpackets().get(), packet_count);
                        BOOST_CHECK_EQUAL(myCounter.get()->getRxbytes().get(), byte_count);
                    }
                } else {
                    LOG(DEBUG) << "SGclassifiercounter mo isnt present";
                });
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
            fs->cookie = fe->entry->cookie;
            fs->packet_count = packet_count;
            fs->byte_count = PACKET_SIZE * (fs->packet_count);
            fs->flags = fe->entry->flags;
            fs->match = fe->entry->match;
            LOG(DEBUG) << "### stats reply for COOKIE: "
                       << ovs_ntohll(fe->entry->cookie)
                       << " pkts: " << packet_count
                       << " bytes: " << fs->byte_count;

            ofputil_append_flow_stats_reply(fs, &ovs_replies, NULL);
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

        return NULL;
    }

    void writeClassifierFlows(FlowEntryList& entryList,
                              uint32_t table_id,
                              uint32_t portNum,
                              shared_ptr<L24Classifier>& classifier,
                              shared_ptr<EpGroup> srcEpg = NULL,
                              shared_ptr<EpGroup> dstEpg = NULL,
                              PolicyManager *policyManager = NULL) {
        uint32_t priority = PolicyManager::MAX_POLICY_RULE_PRIORITY;
        const string& classifierId = classifier->getURI().toString();
        uint32_t cookie =
            idGen.getId(IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID),
                        classifier->getURI().toString());
        LOG(DEBUG) << "Received cookie: " << cookie << " for classifier: "
                    << classifier->getURI().toString();

        if (srcEpg != NULL && dstEpg != NULL)
            {
                WAIT_FOR(policyManager->getVnidForGroup(srcEpg->getURI()), 500);
                WAIT_FOR(policyManager->getVnidForGroup(dstEpg->getURI()), 500);

                uint32_t epg1_vnid =
                    policyManager->getVnidForGroup(srcEpg->getURI()).get();
                uint32_t epg2_vnid =
                    policyManager->getVnidForGroup(dstEpg->getURI()).get();
                FlowBuilder().cookie(ovs_htonll(cookie)).inPort(portNum).table(table_id)
                    .flags(OFPUTIL_FF_SEND_FLOW_REM)
                    .priority(priority).reg(0,epg1_vnid).reg(2,epg2_vnid).
                    build(entryList);
            } else {
            FlowBuilder().cookie(ovs_htonll(cookie)).inPort(portNum).table(table_id)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .priority(priority).reg(0,1).build(entryList);
        }
        FlowEntryList entryListCopy(entryList);
        switchManager.writeFlow(classifierId, table_id,
                                entryListCopy);
    }

    void testCircBuffer(MockConnection& portConn,
                        shared_ptr<L24Classifier>& classifier,
                        uint32_t table_id,
                        uint32_t portNum,
                        PolicyStatsManager *statsManager,
                        shared_ptr<EpGroup> srcEpg = NULL,
                        shared_ptr<EpGroup> dstEpg = NULL,
                        PolicyManager *policyManager = NULL) {
        FlowEntryList entryList;
        writeClassifierFlows(entryList,table_id,portNum,classifier);
        boost::system::error_code ec;
        ec = make_error_code(boost::system::errc::success);
        // Call on_timer function to process the flow entries received from
        // switchManager.
        statsManager->on_timer(ec);
        // create first flow stats reply message
        struct ofpbuf *res_msg =
            makeFlowStatReplyMessage_2(&portConn,
                                       INITIAL_PACKET_COUNT,
                                       table_id,
                                       entryList);
        LOG(DEBUG) << "1 makeFlowStatReplyMessage created";
        BOOST_REQUIRE(res_msg!=0);
        ofp_header *msgHdr = (ofp_header *)res_msg->data;
        statsManager->testInjectTxnId(msgHdr->xid);

        // send first flow stats reply message
        statsManager->Handle(&portConn,
                             OFPTYPE_FLOW_STATS_REPLY, res_msg);
        LOG(DEBUG) << "1 FlowStatsReplyMessage handled";
        ofpbuf_delete(res_msg);
        uint64_t firstId = 0;
        for (int statCount = 0;
             statCount < PolicyStatsManager::MAX_COUNTER_LIMIT+1; statCount++) {

            // create subsequent flow stats reply message
            res_msg = makeFlowStatReplyMessage_2(&portConn,
                                                 FINAL_PACKET_COUNT *
                                                 (statCount+1),
                                                 table_id,
                                                 entryList);
            msgHdr = (ofp_header *)res_msg->data;
            statsManager->testInjectTxnId(msgHdr->xid);
            // send second flow stats reply message
            statsManager->Handle(&portConn,
                                 OFPTYPE_FLOW_STATS_REPLY, res_msg);
            ofpbuf_delete(res_msg);
            // Call on_timer function to process the stats collected
            // and generate Genie objects for stats

            statsManager->on_timer(ec);
            if (statCount == 0) {
                firstId = statsManager->getCurrClsfrGenId();
            }
        }
        optional<shared_ptr<PolicyStatUniverse> > su =
            PolicyStatUniverse::resolve(agent.getFramework());
        auto uuid =
            boost::lexical_cast<std::string>(statsManager->getAgentUUID());
        if (srcEpg.get() && dstEpg.get()) {
            optional<shared_ptr<L24ClassifierCounter> > myCounter =
                su.get()->resolveGbpeL24ClassifierCounter(uuid,firstId,
                                                          srcEpg->getURI()
                                                          .toString(),
                                                          dstEpg->getURI()
                                                          .toString(),
                                                          classifier->getURI()
                                                          .toString());
            WAIT_FOR(!myCounter,500);
        } else {
            optional<shared_ptr<SecGrpClassifierCounter> > myCounter =
                su.get()->resolveGbpeSecGrpClassifierCounter(uuid,firstId,
                                                             classifier->
                                                             getURI().
                                                             toString());
            WAIT_FOR(!myCounter,500);
        }
    }

    void testOneFlow(MockConnection& portConn,
                     shared_ptr<L24Classifier>& classifier,uint32_t table_id,
                     uint32_t portNum,PolicyStatsManager *statsManager,
                     PolicyManager *policyManager = NULL,
                     shared_ptr<EpGroup> srcEpg = NULL,
                     shared_ptr<EpGroup> dstEpg = NULL) {
        // add flows in switchManager
        FlowEntryList entryList;
        writeClassifierFlows(entryList,table_id,portNum,classifier,
                             srcEpg,dstEpg,policyManager);

        boost::system::error_code ec;
        ec = make_error_code(boost::system::errc::success);
        // Call on_timer function to process the flow entries received from
        // switchManager.
        statsManager->on_timer(ec);
        LOG(DEBUG) << "1 on_timer called";

        // create first flow stats reply message
        struct ofpbuf *res_msg =
            makeFlowStatReplyMessage_2(&portConn,
                                       INITIAL_PACKET_COUNT,
                                       table_id,
                                       entryList);
        LOG(DEBUG) << "1 makeFlowStatReplyMessage created";
        BOOST_REQUIRE(res_msg!=0);
        ofp_header *msgHdr = (ofp_header *)res_msg->data;
        statsManager->testInjectTxnId(msgHdr->xid);

        // send first flow stats reply message
        statsManager->Handle(&portConn,
                             OFPTYPE_FLOW_STATS_REPLY, res_msg);
        LOG(DEBUG) << "1 FlowStatsReplyMessage handled";
        ofpbuf_delete(res_msg);

        // create second flow stats reply message
        res_msg = makeFlowStatReplyMessage_2(&portConn,
                                             FINAL_PACKET_COUNT,
                                             table_id,
                                             entryList);
        LOG(DEBUG) << "2 makeFlowStatReplyMessage created";
        BOOST_REQUIRE(res_msg!=0);
        msgHdr = (ofp_header *)res_msg->data;
        statsManager->testInjectTxnId(msgHdr->xid);

        // send second flow stats reply message
        statsManager->Handle(&portConn,
                             OFPTYPE_FLOW_STATS_REPLY, res_msg);
        LOG(DEBUG) << "2 FlowStatsReplyMessage handled";
        ofpbuf_delete(res_msg);
        // Call on_timer function to process the stats collected
        // and generate Genie objects for stats

        statsManager->on_timer(ec);
        LOG(DEBUG) << "2 on_timer called";

        // calculate expected packet count and byte count
        // that we should have in Genie object

        uint32_t num_flows = entryList.size();
        uint32_t exp_classifier_packet_count =
            (FINAL_PACKET_COUNT - INITIAL_PACKET_COUNT) * num_flows;
        LOG(DEBUG) << "num_flows: " << num_flows << " exp: " << exp_classifier_packet_count;

        // Verify per classifier/per epg pair packet and byte count

        verifyFlowStats(classifier,
                        exp_classifier_packet_count,
                        exp_classifier_packet_count * PACKET_SIZE,
                        table_id,statsManager,srcEpg,dstEpg);

        LOG(DEBUG) << "FlowStatsReplyMessage verification successful";
    }
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_POLSTATSMANAGERFIXTURE_H_ */
