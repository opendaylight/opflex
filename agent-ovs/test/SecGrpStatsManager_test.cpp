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

#include "logging.h"
#include "ModbFixture.h"
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "AccessFlowManager.h"
#include "IntFlowManager.h"
#include "SecGrpStatsManager.h"
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

static const uint32_t LAST_PACKET_COUNT = 379; // for removed flow entry

class SecGrpStatsManagerFixture : public FlowManagerFixture {

public:
    SecGrpStatsManagerFixture() : FlowManagerFixture(),
                                  secGrpStatsManager(&agent, idGen,
                                                     switchManager, 10) {
        idGen.initNamespace("l24classifierRule");
        idGen.initNamespace("secGroupSet");
        idGen.initNamespace("secGroup");
        createObjects();
        createPolicyObjects();
        switchManager.setMaxFlowTables(10);
    }
    virtual ~SecGrpStatsManagerFixture() {}
    SecGrpStatsManager secGrpStatsManager;

private:
};

BOOST_AUTO_TEST_SUITE(SecGrpStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testFlowMatchStats, SecGrpStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    secGrpStatsManager.registerConnection(&accPortConn);
    secGrpStatsManager.start();
    secGrpStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_STATS_REPLY, NULL);
    // testing one flow only
    testOneFlow(accPortConn,classifier1,
            AccessFlowManager::SEC_GROUP_IN_TABLE_ID,1,&secGrpStatsManager);
    // 2 entries in flow table now - testing second flow
    testOneFlow(accPortConn,classifier2,
            AccessFlowManager::SEC_GROUP_IN_TABLE_ID,2,&secGrpStatsManager);
    // changing flow table entry 
    testOneFlow(accPortConn,classifier1,
            AccessFlowManager::SEC_GROUP_IN_TABLE_ID,2,&secGrpStatsManager);
    // same 3 steps above for OUT table
    testOneFlow(accPortConn,classifier1,
            AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,1,&secGrpStatsManager);
    testOneFlow(accPortConn,classifier2,
            AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,2,&secGrpStatsManager);
    testOneFlow(accPortConn,classifier1,
            AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,2,&secGrpStatsManager);
    secGrpStatsManager.stop();

}



BOOST_FIXTURE_TEST_CASE(testFlowRemoved, SecGrpStatsManagerFixture) {
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    secGrpStatsManager.registerConnection(&accPortConn);
    secGrpStatsManager.start();

    // Add flows in switchManager
    FlowEntryList entryList;
    writeClassifierFlows(entryList,AccessFlowManager::SEC_GROUP_IN_TABLE_ID,1,classifier3);
    FlowEntryList entryList1;
    writeClassifierFlows(entryList1,AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,1,classifier3);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    secGrpStatsManager.on_timer(ec);

    struct ofpbuf *res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                                      LAST_PACKET_COUNT,
                                                      AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                                      entryList);
    BOOST_REQUIRE(res_msg!=0);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    secGrpStatsManager.on_timer(ec);
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                       LAST_PACKET_COUNT,
                                       AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                                       entryList);
    BOOST_REQUIRE(res_msg!=0);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                       LAST_PACKET_COUNT,
                                       AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                       entryList1);
    BOOST_REQUIRE(res_msg!=0);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    res_msg = makeFlowRemovedMessage_2(&accPortConn,
                                       LAST_PACKET_COUNT,
                                       AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                                       entryList1);
    BOOST_REQUIRE(res_msg!=0);

    secGrpStatsManager.Handle(&accPortConn,
                              OFPTYPE_FLOW_REMOVED, res_msg);
    ofpbuf_delete(res_msg);
    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    secGrpStatsManager.on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    uint32_t num_flows = entryList.size();
    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    AccessFlowManager::SEC_GROUP_IN_TABLE_ID,
                    &secGrpStatsManager);
    verifyFlowStats(classifier3,
                    LAST_PACKET_COUNT,
                    LAST_PACKET_COUNT * PACKET_SIZE,
                    AccessFlowManager::SEC_GROUP_OUT_TABLE_ID,
                    &secGrpStatsManager);
    secGrpStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
