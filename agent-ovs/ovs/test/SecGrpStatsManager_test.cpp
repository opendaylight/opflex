/*
 * Test suite for class SecGrpStatsManager.
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

#include <opflexagent/test/ModbFixture.h>
#include "ovs-ofputil.h"
#include "AccessFlowManager.h"
#include "IntFlowManager.h"
#include "SecGrpStatsManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "FlowConstants.h"
#include "PolicyStatsManagerFixture.h"

#include "ovs-ofputil.h"
#include <modelgbp/gbpe/SecGrpClassifierCounter.hpp>
#include <modelgbp/gbpe/L24ClassifierCounter.hpp>
#include <modelgbp/observer/PolicyStatUniverse.hpp>

#include <opflex/modb/Mutator.h>
#include <modelgbp/gbp/SecGroup.hpp>

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
using opflex::modb::Mutator;

typedef opflexagent::EndpointListener::uri_set_t uri_set_t;

namespace opflexagent {

static const uint32_t LAST_PACKET_COUNT = 379; // for removed flow entry

class MockSecGrpStatsManager : public SecGrpStatsManager {
public:
    MockSecGrpStatsManager(Agent *agent_,
                           IdGenerator& idGen_,
                           SwitchManager& switchManager_,
                           long timer_interval_)
        : SecGrpStatsManager(agent_, idGen_, switchManager_, timer_interval_) {};

    void testInjectTxnId (uint32_t txn_id) {
        std::lock_guard<mutex> lock(txnMtx);
        txns.insert(txn_id);
    }
};

class SecGrpStatsManagerFixture : public PolicyStatsManagerFixture {

public:
    SecGrpStatsManagerFixture() : PolicyStatsManagerFixture(),
                                  secGrpStatsManager(&agent, idGen,
                                                     switchManager, 100000) {
        idGen.initNamespace("l24classifierRule");
        idGen.initNamespace("secGroupSet");
        idGen.initNamespace("secGroup");
        createObjects();
        createPolicyObjects();
        switchManager.setMaxFlowTables(IntFlowManager::NUM_FLOW_TABLES);
    }
    virtual ~SecGrpStatsManagerFixture() {
        stop();
    }
#ifdef HAVE_PROMETHEUS_SUPPORT
    virtual void verifyPromMetrics(shared_ptr<L24Classifier> classifier,
                            uint32_t pkts,
                            uint32_t bytes,
                            bool isTx=false) override;
#endif
    MockSecGrpStatsManager secGrpStatsManager;
};

#ifdef HAVE_PROMETHEUS_SUPPORT
void SecGrpStatsManagerFixture::
verifyPromMetrics (shared_ptr<L24Classifier> classifier,
                   uint32_t pkts,
                   uint32_t bytes,
                   bool isTx)
{
    std::string s_rx_bytes, s_rx_pkts, s_tx_bytes, s_tx_pkts;
    if (classifier == classifier1) {
        s_tx_bytes = "opflex_sg_tx_bytes{classifier=\"tenant:tenant0,policy:"\
                     "classifier1,[etype:2048,proto:6,dport:80]\"} "\
                     + boost::lexical_cast<std::string>(bytes) + ".000000";
        s_rx_bytes = "opflex_sg_rx_bytes{classifier=\"tenant:tenant0,policy:"\
                     "classifier1,[etype:2048,proto:6,dport:80]\"} "\
                     + boost::lexical_cast<std::string>(bytes) + ".000000";
        s_tx_pkts = "opflex_sg_tx_packets{classifier=\"tenant:tenant0,policy:"\
                    "classifier1,[etype:2048,proto:6,dport:80]\"} "\
                    + boost::lexical_cast<std::string>(pkts) + ".000000";
        s_rx_pkts = "opflex_sg_rx_packets{classifier=\"tenant:tenant0,policy:"\
                    "classifier1,[etype:2048,proto:6,dport:80]\"} "\
                    + boost::lexical_cast<std::string>(pkts) + ".000000";
    } else if (classifier == classifier2) {
        s_tx_bytes = "opflex_sg_tx_bytes{classifier=\"tenant:tenant0,policy:"\
                     "classifier2,[etype:2054,]\"} "\
                     + boost::lexical_cast<std::string>(bytes) + ".000000";
        s_rx_bytes = "opflex_sg_rx_bytes{classifier=\"tenant:tenant0,policy:"\
                     "classifier2,[etype:2054,]\"} "\
                     + boost::lexical_cast<std::string>(bytes) + ".000000";
        s_tx_pkts = "opflex_sg_tx_packets{classifier=\"tenant:tenant0,policy:"\
                    "classifier2,[etype:2054,]\"} "\
                    + boost::lexical_cast<std::string>(pkts) + ".000000";
        s_rx_pkts = "opflex_sg_rx_packets{classifier=\"tenant:tenant0,policy:"\
                    "classifier2,[etype:2054,]\"} "\
                    + boost::lexical_cast<std::string>(pkts) + ".000000";
    } else {
        s_tx_bytes = "opflex_sg_tx_bytes{classifier=\"tenant:tenant0,policy:"\
                     "classifier3,[etype:2048,proto:6,dport:80-85,]\"} "\
                     + boost::lexical_cast<std::string>(bytes) + ".000000";
        s_rx_bytes = "opflex_sg_rx_bytes{classifier=\"tenant:tenant0,policy:"\
                     "classifier3,[etype:2048,proto:6,dport:80-85,]\"} "\
                     + boost::lexical_cast<std::string>(bytes) + ".000000";
        s_tx_pkts = "opflex_sg_tx_packets{classifier=\"tenant:tenant0,policy:"\
                    "classifier3,[etype:2048,proto:6,dport:80-85,]\"} "\
                    + boost::lexical_cast<std::string>(pkts) + ".000000";
        s_rx_pkts = "opflex_sg_rx_packets{classifier=\"tenant:tenant0,policy:"\
                    "classifier3,[etype:2048,proto:6,dport:80-85,]\"} "\
                    + boost::lexical_cast<std::string>(pkts) + ".000000";
    }

    const std::string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    if (isTx) {
        pos = output.find(s_tx_pkts);
        BOOST_CHECK_NE(pos, std::string::npos);
        pos = output.find(s_tx_bytes);
        BOOST_CHECK_NE(pos, std::string::npos);
    } else {
        pos = output.find(s_rx_pkts);
        BOOST_CHECK_NE(pos, std::string::npos);
        pos = output.find(s_rx_bytes);
        BOOST_CHECK_NE(pos, std::string::npos);
    }
}
#endif

BOOST_AUTO_TEST_SUITE(SecGrpStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testFlowMatchStats, SecGrpStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    secGrpStatsManager.registerConnection(&accPortConn);
    secGrpStatsManager.start();
    LOG(DEBUG) << "### SecGrpClassifierCounter flow stats start";
    secGrpStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, NULL);
    LOG(DEBUG) << "### SecGrpClassifierCounter flow stats in start";
    // testing one flow only
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier1,
                AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                1, false, &secGrpStatsManager);
    // 2 entries in flow table now - testing second flow
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier2,
                AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                2, false,
                &secGrpStatsManager);
    // changing flow table entry
    // Note: If the portNum is set as 2, then it clashes with classifier2
    // entry. So first classifier1 entry will get deleted. No new counter
    // objeects will get generated and verifyflowstats will fail.
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier1,
                AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                3, true,
                &secGrpStatsManager);
    LOG(DEBUG) << "### SecGrpClassifierCounter flow stats out start";
    // same 3 steps above for OUT table
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier1,
                AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                1, false,
                &secGrpStatsManager);
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier2,
                AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                2, false,
                &secGrpStatsManager);
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier1,
                AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                3, true,
                &secGrpStatsManager);
    LOG(DEBUG) << "### SecGrpClassifierCounter flow stats stop";
    secGrpStatsManager.stop();

}



BOOST_FIXTURE_TEST_CASE(testFlowRemoved, SecGrpStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    secGrpStatsManager.registerConnection(&accPortConn);
    secGrpStatsManager.start();
    LOG(DEBUG) << "### SecGrpClassifierCounter flow removed start";

    // Add flows in switchManager
    FlowEntryList entryList;
    writeClassifierFlows(entryList,
                         AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                         1,
                         classifier3);
    FlowEntryList entryList1;
    writeClassifierFlows(entryList1,
                         AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                         1,
                         classifier3);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    secGrpStatsManager.on_timer(ec);

    struct ofpbuf *res_msg =
        makeFlowRemovedMessage_2(&accPortConn,
                                 LAST_PACKET_COUNT,
                                 AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                 entryList);
    BOOST_REQUIRE(res_msg!=0);
    struct ofputil_flow_removed fentry;
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg, &fentry);
    ofpbuf_delete(res_msg);

    // Collect counts related to Rx
    secGrpStatsManager.on_timer(ec);

    // 2nd delete received from ovs for same flow, ideally this is a no-op.
    // But since on-timer is called table state's flow would have created
    // an entry in newCounterMap. So the flow removed message will lead
    // to accumulation of stats to prom metric.
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                       LAST_PACKET_COUNT,
                                       AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                       entryList);
    BOOST_REQUIRE(res_msg!=0);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg, &fentry);
    ofpbuf_delete(res_msg);

    res_msg =
        makeFlowRemovedMessage_2(&accPortConn,
                                 LAST_PACKET_COUNT,
                                 AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                 entryList1);
    BOOST_REQUIRE(res_msg!=0);
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg, &fentry);
    ofpbuf_delete(res_msg);
    res_msg =
        makeFlowRemovedMessage_2(&accPortConn,
                                 LAST_PACKET_COUNT,
                                 AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                 entryList1);
    BOOST_REQUIRE(res_msg!=0);
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg, &fentry);
    ofpbuf_delete(res_msg);
    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    // Collect counts relateed to Tx
    secGrpStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    true,
                    AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                    &secGrpStatsManager);
    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    false,
                    AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                    &secGrpStatsManager);
    LOG(DEBUG) << "### SecGrpClassifierCounter flow removed stop";
    secGrpStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testCircularBuffer, SecGrpStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    secGrpStatsManager.registerConnection(&accPortConn);
    secGrpStatsManager.start();

    LOG(DEBUG) << "### SecGrpClassifierCounter circbuffer start";
    // Add flows in switchManager

    testCircBuffer<MockSecGrpStatsManager>(accPortConn,
                   classifier3,
                   AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                   2,
                   &secGrpStatsManager);
    LOG(DEBUG) << "### SecGrpClassifierCounter circbuffer stop";
    secGrpStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testSecGrpDelete, SecGrpStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    secGrpStatsManager.registerConnection(&accPortConn);
    secGrpStatsManager.start();

    LOG(DEBUG) << "### SecGrpClassifierCounter Delete Start";
    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, NULL);
    // testing one flow only
    testOneFlow<MockSecGrpStatsManager>(accPortConn,classifier1,
                AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                1, false, &secGrpStatsManager);
    Mutator mutator(agent.getFramework(), "policyreg");

    // Note: In UTs, deleting the sg doesnt trigger classifier delete
    //modelgbp::gbp::SecGroup::remove(agent.getFramework(),"tenant0","secgrp1");
    modelgbp::gbpe::L24Classifier::remove(agent.getFramework(),"tenant0","classifier1");
    mutator.commit();
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());
    auto uuid =
        boost::lexical_cast<std::string>(secGrpStatsManager.getAgentUUID());
    optional<shared_ptr<SecGrpClassifierCounter> > myCounter;
    WAIT_FOR_DO_ONFAIL(!(su.get()->resolveGbpeSecGrpClassifierCounter(uuid,
                        secGrpStatsManager.getCurrClsfrGenId(),
                        classifier1->getURI().toString()))
                        ,500
                        ,
                        ,LOG(ERROR) << "Obj still present";);
    LOG(DEBUG) << "### SecGrpClassifierCounter Delete End";
    secGrpStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
