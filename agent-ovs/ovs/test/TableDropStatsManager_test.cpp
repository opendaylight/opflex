/*
 * Test suite for class TableDropStatsManager.
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
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
#include "TableDropStatsManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "FlowConstants.h"
#include "PolicyStatsManagerFixture.h"
#include "eth.h"
#include "ovs-ofputil.h"

extern "C" {
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>
}

using boost::optional;

using std::shared_ptr;
using std::string;
using opflex::modb::URI;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;

namespace opflexagent {

class TableDropStatsManagerFixture : public PolicyStatsManagerFixture {

public:
    TableDropStatsManagerFixture() : PolicyStatsManagerFixture(),
                                  accPortConn(TEST_CONN_TYPE_ACC),
                                  intPortConn(TEST_CONN_TYPE_INT),
                                  accBr(agent, exec, reader,
                                        accPortMapper),
                                  intFlowManager(agent, switchManager, idGen,
                                                 ctZoneManager, pktInHandler,
                                                 tunnelEpManager),
                                  accFlowManager(agent, accBr, idGen,
                                                 ctZoneManager),
                                  pktInHandler(agent, intFlowManager),
                                  tableDropStatsManager(&agent, idGen,
                                            switchManager, accBr, 20000000) {
        tableDropStatsManager.setAgentUUID(agent.getUuid());
    }
    virtual ~TableDropStatsManagerFixture() {
        tableDropStatsManager.stop();
        accFlowManager.stop();
        intFlowManager.stop();
        stop();
    }
    void start() {
        FlowManagerFixture::start();
        intFlowManager.setEncapType(IntFlowManager::ENCAP_VXLAN);
        intFlowManager.start();
        accFlowManager.start();
        setConnected();
        tableDropStatsManager.registerConnection(&intPortConn,
                &accPortConn);
        tableDropStatsManager.start();
    }
    PolicyStatsManager &getIntTableDropStatsManager() {
        return tableDropStatsManager.intTableDropStatsMgr;
    }
    PolicyStatsManager &getAccTableDropStatsManager() {
        return tableDropStatsManager.accTableDropStatsMgr;
    }
    MockConnection accPortConn, intPortConn;
    PortMapper accPortMapper;
    MockSwitchManager accBr;
    IntFlowManager intFlowManager;
    AccessFlowManager accFlowManager;
    PacketInHandler pktInHandler;
    TableDropStatsManager tableDropStatsManager;

    void createIntBridgeDropFlowList(uint32_t table_id,
             FlowEntryList& entryList);
    void createAccBridgeDropFlowList(uint32_t table_id,
             FlowEntryList& entryList);
    void testOneStaticDropFlow(MockConnection& portConn,
                               uint32_t table_id,
                               PolicyStatsManager &statsManager,
                               SwitchManager &swMgr,
                               bool refresh_aging);
    void verifyDropFlowStats(uint64_t exp_packet_count,
                             uint64_t exp_byte_count,
                             uint32_t table_id,
                             MockConnection& portConn,
                             PolicyStatsManager &statsManager);

#ifdef HAVE_PROMETHEUS_SUPPORT
    const string cmd = "curl --proxy \"\" --compressed --silent http://127.0.0.1:9612/metrics 2>&1;";
    void checkPrometheusCounters(uint64_t exp_packet_count,
                                 uint64_t exp_byte_count,
                                 const std::string &bridgeName,
                                 const std::string &tableName);
#endif
};

#ifdef HAVE_PROMETHEUS_SUPPORT
void TableDropStatsManagerFixture::checkPrometheusCounters(uint64_t exp_packet_count,
                             uint64_t exp_byte_count,
                             const std::string &bridgeName,
                             const std::string &tableName) {
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    const string& promCtr= bridgeName+ "_" +tableName;
    string packets_key = "opflex_table_drop_packets{table=\"" + promCtr + "\"} "
        + boost::lexical_cast<std::string>(exp_packet_count) + ".000000";
    string bytes_key = "opflex_table_drop_bytes{table=\"" + promCtr + "\"} "
        + boost::lexical_cast<std::string>(exp_byte_count) + ".000000";
    string::size_type pos = output.find(packets_key);
    BOOST_CHECK_NE(pos, string::npos);
    pos = output.find(bytes_key);
    BOOST_CHECK_NE(pos, string::npos);
    return;
}
#endif

void TableDropStatsManagerFixture::verifyDropFlowStats (
                                    uint64_t exp_packet_count,
                                    uint64_t exp_byte_count,
                                    uint32_t table_id,
                                    MockConnection& portConn,
                                    PolicyStatsManager &statsManager) {
    optional<shared_ptr<PolicyStatUniverse>> su =
                PolicyStatUniverse::resolve(agent.getFramework());
    optional<shared_ptr<TableDropCounter>> tableDropCounter;
    if (su) {
        tableDropCounter = su.get()->resolveGbpeTableDropCounter(
                                        agent.getUuid(),
                                        portConn.getSwitchName(),
                                        table_id);
        WAIT_FOR_DO_ONFAIL(
                   (tableDropCounter && tableDropCounter.get()
                        && (tableDropCounter.get()->getPackets().get()
                                == exp_packet_count)
                        && (tableDropCounter.get()->getBytes().get()
                                == exp_byte_count)),
                   500, // usleep(1000) * 500 = 500ms
                   tableDropCounter = su.get()->resolveGbpeTableDropCounter(
                                            agent.getUuid(),
                                            portConn.getSwitchName(),
                                            table_id),
                   if (tableDropCounter && tableDropCounter.get()) {
                       BOOST_CHECK_EQUAL(
                               tableDropCounter.get()->getPackets().get(),
                               exp_packet_count);
                       BOOST_CHECK_EQUAL(
                               tableDropCounter.get()->getBytes().get(),
                               exp_byte_count);
                   } else {
                       LOG(DEBUG) << "TableDropCounter mo for "
                                  << portConn.getSwitchName()
                                  << "-" << table_id
                                  << " isn't present";
                   });
    }
}

void TableDropStatsManagerFixture::createAccBridgeDropFlowList(
        uint32_t table_id,
        FlowEntryList& entryList ) {
    FlowBuilder().priority(0).cookie(flow::cookie::TABLE_DROP_FLOW)
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .action().dropLog(table_id)
            .go(AccessFlowManager::EXP_DROP_TABLE_ID)
            .parent().build(entryList);
}

void TableDropStatsManagerFixture::createIntBridgeDropFlowList(
        uint32_t table_id,
        FlowEntryList& entryList ) {
    if(table_id == IntFlowManager::SEC_TABLE_ID) {
        FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .ethType(eth::type::ARP)
                .action().dropLog(IntFlowManager::SEC_TABLE_ID)
                .go(IntFlowManager::EXP_DROP_TABLE_ID).parent()
                .build(entryList);
        FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .ethType(eth::type::IP)
                .action().dropLog(IntFlowManager::SEC_TABLE_ID)
                .go(IntFlowManager::EXP_DROP_TABLE_ID).parent()
                .build(entryList);
        FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .ethType(eth::type::IPV6)
                .action().dropLog(IntFlowManager::SEC_TABLE_ID)
                .go(IntFlowManager::EXP_DROP_TABLE_ID).parent()
                .build(entryList);
    }
    FlowBuilder().priority(0).cookie(flow::cookie::TABLE_DROP_FLOW)
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .action().dropLog(table_id)
            .go(IntFlowManager::EXP_DROP_TABLE_ID)
            .parent().build(entryList);

}

void TableDropStatsManagerFixture::testOneStaticDropFlow (
        MockConnection& portConn,
        uint32_t table_id,
        PolicyStatsManager &statsManager,
        SwitchManager &swMgr,
        bool refresh_aged_flow=false)
{
    uint64_t expected_pkt_count = INITIAL_PACKET_COUNT*2,
            expected_byte_count = INITIAL_PACKET_COUNT*2*PACKET_SIZE;
    FlowEntryList dropLogFlows;
    if(portConn.getSwitchName()=="int_conn") {
        createIntBridgeDropFlowList(table_id,
                dropLogFlows);
        if(table_id == IntFlowManager::SEC_TABLE_ID){
            expected_pkt_count *= 4;
            expected_byte_count *=4;
        }
    } else {
        createAccBridgeDropFlowList(table_id,
                        dropLogFlows);
    }
    int ctr = 1;
    if(refresh_aged_flow) {
        ctr++;
    }
    // Call on_timer function to process the flow entries received from
    // switchManager.
    boost::system::error_code ec;
    do{
        ec = make_error_code(boost::system::errc::success);
        statsManager.on_timer(ec);
        LOG(DEBUG) << "Called on_timer";
        ctr--;
    } while(ctr>0);
    // create first flow stats reply message
    struct ofpbuf *res_msg =
        makeFlowStatReplyMessage_2(&portConn,
                                   INITIAL_PACKET_COUNT,
                                   table_id,
                                   dropLogFlows);
    LOG(DEBUG) << "1 makeFlowStatReplyMessage created";
    BOOST_REQUIRE(res_msg!=0);
    ofp_header *msgHdr = (ofp_header *)res_msg->data;
    statsManager.testInjectTxnId(msgHdr->xid);

    // send first flow stats reply message
    statsManager.Handle(&portConn,
                         OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handled";
    ofpbuf_delete(res_msg);
    ec = make_error_code(boost::system::errc::success);
    statsManager.on_timer(ec);
    LOG(DEBUG) << "Called on_timer";

    // create second flow stats reply message
    res_msg =
        makeFlowStatReplyMessage_2(&portConn,
                                   INITIAL_PACKET_COUNT*2,
                                   table_id,
                                   dropLogFlows);
    LOG(DEBUG) << "2 makeFlowStatReplyMessage created";
    BOOST_REQUIRE(res_msg!=0);
    msgHdr = (ofp_header *)res_msg->data;
    statsManager.testInjectTxnId(msgHdr->xid);

    // send second flow stats reply message
    statsManager.Handle(&portConn,
                         OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "2 FlowStatsReplyMessage handled";
    ofpbuf_delete(res_msg);

    ec = make_error_code(boost::system::errc::success);
    statsManager.on_timer(ec);
    LOG(DEBUG) << "Called on_timer";

    verifyDropFlowStats(expected_pkt_count,
                        expected_byte_count,
                        table_id, portConn, statsManager);
#ifdef HAVE_PROMETHEUS_SUPPORT
    SwitchManager::TableDescriptionMap fwdTableMap;
    swMgr.getForwardingTableList(fwdTableMap);
    checkPrometheusCounters(expected_pkt_count,
                            expected_byte_count,
                            portConn.getSwitchName(),
                            fwdTableMap[table_id].first);
#endif

}

BOOST_AUTO_TEST_SUITE(TableDropStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testStaticDropFlowsInt, TableDropStatsManagerFixture) {
    start();
    for(int i=IntFlowManager::SEC_TABLE_ID ;
            i < IntFlowManager::EXP_DROP_TABLE_ID ; i++) {
        testOneStaticDropFlow(intPortConn, i,
                getIntTableDropStatsManager(),
                switchManager, (i>3));
    }
    tableDropStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testStaticDropFlowsAcc, TableDropStatsManagerFixture) {
    start();
    for(int i = AccessFlowManager::GROUP_MAP_TABLE_ID;
            i < AccessFlowManager::EXP_DROP_TABLE_ID; i++) {
        testOneStaticDropFlow(accPortConn, i,
                              getAccTableDropStatsManager(),
                              accBr, (i>3));
    }
    tableDropStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
