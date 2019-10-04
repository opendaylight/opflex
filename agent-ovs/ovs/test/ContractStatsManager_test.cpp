/*
 * Test suite for class ContractStatsManager.
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

#include <opflexagent/logging.h>
#include <opflexagent/test/ModbFixture.h>
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "IntFlowManager.h"
#include "ContractStatsManager.h"
#include "SwitchConnection.h"
#include "MockSwitchManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "FlowConstants.h"
#include "FlowUtils.h"
#include "PolicyStatsManagerFixture.h"
#include "FlowBuilder.h"
#include <opflex/modb/Mutator.h>
#include <modelgbp/gbp/Contract.hpp>
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
using opflex::modb::Mutator;

namespace opflexagent {

static const uint32_t LAST_PACKET_COUNT = 379; // for removed flow entry

class ContractStatsManagerFixture : public PolicyStatsManagerFixture {

public:
    ContractStatsManagerFixture() : PolicyStatsManagerFixture(),
                                    contractStatsManager(&agent, idGen,
                                                         switchManager, 10),
                                    policyManager(agent.getPolicyManager()) {
        createObjects();
        createPolicyObjects();
        idGen.initNamespace("l24classifierRule");
        idGen.initNamespace("routingDomain");
        switchManager.setMaxFlowTables(IntFlowManager::NUM_FLOW_TABLES);
    }
    virtual ~ContractStatsManagerFixture() {
        stop();
    }
    void verifyRoutingDomainDropStats(shared_ptr<RoutingDomain> rd,
                                      uint32_t packet_count,
                                      uint32_t byte_count);
    ContractStatsManager contractStatsManager;
    PolicyManager& policyManager;
private:
};


void ContractStatsManagerFixture::
verifyRoutingDomainDropStats(shared_ptr<RoutingDomain> rd,
                             uint32_t packet_count,
                             uint32_t byte_count) {

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    auto uuid =
        boost::lexical_cast<string>(contractStatsManager.getAgentUUID());
    optional<shared_ptr<RoutingDomainDropCounter> > myCounter =
        su.get()->resolveGbpeRoutingDomainDropCounter(uuid,
                                                      contractStatsManager
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

BOOST_AUTO_TEST_SUITE(ContractStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testFlowMatchStats, ContractStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    contractStatsManager.registerConnection(&integrationPortConn);
    contractStatsManager.start();

    contractStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    contractStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_STATS_REPLY, NULL);

    testOneFlow(integrationPortConn,classifier3,
                IntFlowManager::POL_TABLE_ID,
                1,
                &contractStatsManager,
                epg1,
                epg2,
                &policyManager);

    contractStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testRdDropStats, ContractStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    contractStatsManager.registerConnection(&integrationPortConn);
    contractStatsManager.start();

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

    contractStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);
    LOG(DEBUG) << "testRd:FlowStatsReplyMessage handling successful";

    verifyRoutingDomainDropStats(rd0, packet_count, byte_count);
    LOG(DEBUG) << "testRd:FlowStatsReplyMessage verification successful";
    contractStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemoved, ContractStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    contractStatsManager.registerConnection(&integrationPortConn);
    contractStatsManager.start();

    // Add flows in switchManager
    FlowEntryList entryList;
    writeClassifierFlows(entryList,
                         IntFlowManager::POL_TABLE_ID,
                         1,
                         classifier3,
                         epg1,
                         epg2,
                         &policyManager);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    contractStatsManager.on_timer(ec);

    struct ofpbuf *res_msg =
        makeFlowRemovedMessage_2(&integrationPortConn,
                                 LAST_PACKET_COUNT,
                                 IntFlowManager::POL_TABLE_ID,
                                 entryList);
    BOOST_REQUIRE(res_msg!=0);

    contractStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    contractStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    IntFlowManager::POL_TABLE_ID,
                    &contractStatsManager,
                    epg1,epg2);
    contractStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testCircularBuffer, ContractStatsManagerFixture) {
    MockConnection intPortConn(TEST_CONN_TYPE_INT);
    contractStatsManager.registerConnection(&intPortConn);
    contractStatsManager.start();
    // Add flows in switchManager
    testCircBuffer(intPortConn,classifier3,
                   IntFlowManager::POL_TABLE_ID,2,&contractStatsManager,
                   epg1,epg2,&policyManager);
    contractStatsManager.stop();

}

BOOST_FIXTURE_TEST_CASE(testContractDelete, ContractStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    contractStatsManager.registerConnection(&integrationPortConn);
    contractStatsManager.start();

    contractStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_STATS_REPLY, NULL);

    testOneFlow(integrationPortConn,
                classifier3,
                IntFlowManager::POL_TABLE_ID,
                1,
                &contractStatsManager,
                epg1,
                epg2,
                &policyManager);
    Mutator mutator(agent.getFramework(), "policyreg");
    modelgbp::gbp::Contract::remove(agent.getFramework(),"tenant0","contract1");
    mutator.commit();
    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());
    auto uuid =
        boost::lexical_cast<std::string>(contractStatsManager.getAgentUUID());
    optional<shared_ptr<L24ClassifierCounter> > myCounter =
        su.get()-> resolveGbpeL24ClassifierCounter(uuid,
                                                   contractStatsManager.
                                                   getCurrClsfrGenId(),
                                                   epg1->getURI().toString(),
                                                   epg2->getURI().toString(),
                                                   classifier3->getURI()
                                                   .toString());
    WAIT_FOR(!myCounter,500);
    contractStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
