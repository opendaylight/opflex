/*
 * Test suite for class PodSvcStatsManager.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>

#include <modelgbp/gbpe/SvcToEpCounter.hpp>
#include <modelgbp/gbpe/EpToSvcCounter.hpp>
#include <opflexagent/logging.h>
#include <opflexagent/test/ModbFixture.h>
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "IntFlowManager.h"
#include "PodSvcStatsManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
#include "RangeMask.h"
#include "FlowConstants.h"
#include "PolicyStatsManagerFixture.h"
#include "eth.h"

extern "C" {
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>
}

using boost::optional;
using std::shared_ptr;
using std::string;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;

namespace opflexagent {

static const uint32_t LAST_PACKET_COUNT = 350; // for removed flow entry

class PodSvcStatsManagerFixture : public PolicyStatsManagerFixture {

public:
    PodSvcStatsManagerFixture() : PolicyStatsManagerFixture(),
                                    intFlowManager(agent, switchManager, idGen,
                                                   ctZoneManager, pktInHandler,
                                                   tunnelEpManager),
                                    pktInHandler(agent, intFlowManager),
                                    podsvcStatsManager(&agent, idGen,
                                                       switchManager,
                                                       intFlowManager, 10) {
        createObjects();
        switchManager.setMaxFlowTables(IntFlowManager::NUM_FLOW_TABLES);
        intFlowManager.start();
        LOG(DEBUG) << "############# SERVICE CREATE START ############";
        createServices();
        checkPodSvcObsObj(true);
        LOG(DEBUG) << "############# SERVICE CREATE END ############";
    }

    virtual ~PodSvcStatsManagerFixture() {
        checkPodSvcObsObj(true);
        LOG(DEBUG) << "############# SERVICE DELETE START ############";
        removeServiceObjects();
        checkPodSvcObsObj(false);
        LOG(DEBUG) << "############# SERVICE DELETE END ############";
        intFlowManager.stop();
        stop();
    }

    IntFlowManager  intFlowManager;
    PacketInHandler pktInHandler;
    PodSvcStatsManager podsvcStatsManager;
    void testFlowStats(MockConnection& portConn,
                       PolicyStatsManager *statsManager,
                       bool testAggregate=false);
    void testFlowRemoved(MockConnection& portConn,
                         PolicyStatsManager *statsManager,
                         uint32_t initPkts);
    void checkPodSvcObsObj(bool);
    void removeServiceObjects(void);
    void checkNewFlowMapInitialized();
    void testFlowAge(PolicyStatsManager *statsManager,
                     bool isOld, bool isFlowStateReAdd);

private:
    Service as;
    Service::ServiceMapping sm1;
    Service::ServiceMapping sm2;
    void createServices(void);
    void checkObjectStats(const std::string& epToSvcUuid,
                         const std::string& svcToEpUuid,
                         uint32_t packet_count,
                         uint32_t byte_count);
};

/*
 * Service delete
 * - delete svc objects
 * - check if observer objects go away from caller
 */
void PodSvcStatsManagerFixture::removeServiceObjects (void)
{
    servSrc.removeService(as.getUUID());
    intFlowManager.serviceUpdated(as.getUUID());
}

void PodSvcStatsManagerFixture::createServices (void)
{
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setDomainURI(URI(rd0->getURI()));
    as.setServiceMode(Service::LOADBALANCER);

    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.addNextHopIP("169.254.169.1");
    sm1.addNextHopIP("169.254.169.2");
    sm1.setServicePort(53);
    sm1.setNextHopPort(5353);
    as.addServiceMapping(sm1);

    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.addNextHopIP("fe80::a9:fe:a9:1");
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    sm2.setServicePort(80);
    as.addServiceMapping(sm2);

    servSrc.updateService(as);

    intFlowManager.serviceUpdated(as.getUUID());
}


/**
 * Listing the flow stats logic below to capture different test cases
 *
 * Flow stats logic:
 * ----------------
 * - flowCountersState has old, new and removed state for flow entries
 * - on_timer will walk over all entries in a given table:
 *    - each flow will have a FlowCounter state: visited, age, flow.match, diff/last count
 *    - all configured flows will be kept initially in newFlowCounters[flow].
 *    - during every iteration of on_timer, newFlowCounters[flow].age will be incremented
 *    - diff/last count will be 0 initially.
 * - on_timer will then initiate stats request to OVS for the given table
 *
 *
 * on_timer from client (Contract, PodSvc, SecGrp):
 * -----------------------------------------------
 * - idea is to walk through all the flows in flowCounterState_t and collect
 *   aggregated stats in <Client>CounterMap_t
 * - For each entry in oldFlowCounters:
 *     - if not visited, then age += 1 <-- i.e. flow just moved from new to oldmap...
 *                                         so visited is false....
 *         - continue <-- dont process unvisited flows
 *     - if old.diffs > 0:
 *         - create or update clientCounterMap: form key from flow.cookie
 *             - if agg counters are there retrieve them. Else it is 0.
 *             - agg = agg + old.diffs
 *         - old.diffs = 0 <-- note that last is already saved
 *         - old.age = 0
 *       - mark visited = false
 * - For each entry in removedFlowCounters:
 *     - if rem.diffs:
 *         - create or update clientCounterMap: form key from flow.cookie
 *             - if agg counters are there retrieve them. Else it is 0.
 *             - agg = agg + rem.diffs
 * - clear removedFlowCounters completely
 * - walk over oldFlowCounters: if flow not visited and age>MAX, erase from map
 * - walk over newFlowCounters: if age>MAX, erase from map <-- visited is a NO-OP in newFlowCounters
 *
 *
 * Stats reply from ovs:
 * --------------------
 * - if flow is not there in new or old map, then ignore
 * - if flow is present in newFlowCounters:
 *     - if flow_pkts > 0:
 *         - insert flow in oldFlowCounters, erase from newFlowCounters
 *         - update old.last count, visited=false, age=0
 *         - dont touch diffs. It will be 0. <-- diffs for a new flow will be 0.
 * - if flow is present in oldFlowCounters:
 *     - visited = true
 *     - if (flow_pkts - last_pkts) > 0:
 *         - diff = flow_pkts - last_pkts
 *         - last_pkts = flow_pkts
 *
 *
 * Flow removed from ovs:
 * ----------------------
 * - if flow is not there in new or old map, then ignore
 * - if flow is present in newFlowCounters: // This means this flow is not processed even
 *                                          // once from client after prior response from ovs
 *     - insert flow in removedFlowCounters
 *         - set removed.diffs = flow_pkt_count
 *         - Dont touch last. This will be 0. <-- Since removed, the last is kept 0
 *         - erase flow from newFlowCounters
 * - if flow is present in oldFlowCounters:
 *     - insert flow in removedFlowCounters
 *         - set removed.diffs = old.diffs
 *         - Dont touch removed.last. This will be 0. <-- for removed flows, last is always 0!
 *         - erase flow from oldFlowCounters
 */

void
PodSvcStatsManagerFixture::testFlowAge (PolicyStatsManager *statsManager,
                                        bool isOld,
                                        bool isFlowStateReAdd)
{

    LOG(DEBUG) << "####### START - testFlowAge " << isOld << " " << isFlowStateReAdd
               << " new: " << podsvcStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << podsvcStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << podsvcStatsManager.statsState.removedFlowCounterMap.size();

    if (isOld && !isFlowStateReAdd) {
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.oldFlowCounterMap.size(), 2);
        for (auto age = 0; age < PolicyStatsManager::MAX_AGE; age++) {
            boost::system::error_code ec;
            ec = make_error_code(boost::system::errc::success);
            statsManager->on_timer(ec);
        }
        // 2 flows got aged
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.oldFlowCounterMap.size(), 0);
    }

    if (isOld && isFlowStateReAdd) {
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.oldFlowCounterMap.size(), 0);
        // 16 flows based on config, -2 aged flows
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.newFlowCounterMap.size(), 14);

        boost::system::error_code ec;
        ec = make_error_code(boost::system::errc::success);
        statsManager->on_timer(ec);

        // 2 flows get readded to new map
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.oldFlowCounterMap.size(), 0);
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.newFlowCounterMap.size(), 16);
    }

    if (!isOld && !isFlowStateReAdd) {
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.newFlowCounterMap.size(), 16);
        for (auto age = 0; age < PolicyStatsManager::MAX_AGE; age++) {
            boost::system::error_code ec;
            ec = make_error_code(boost::system::errc::success);
            statsManager->on_timer(ec);
        }
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.newFlowCounterMap.size(), 0);
    }

    if (!isOld && isFlowStateReAdd) {
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.newFlowCounterMap.size(), 0);
        boost::system::error_code ec;
        ec = make_error_code(boost::system::errc::success);
        statsManager->on_timer(ec);
        BOOST_CHECK_EQUAL(podsvcStatsManager.statsState.newFlowCounterMap.size(), 16);
    }

    LOG(DEBUG) << "####### END - testFlowAge " << isOld << " " << isFlowStateReAdd
               << " new: " << podsvcStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << podsvcStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << podsvcStatsManager.statsState.removedFlowCounterMap.size();
}

void
PodSvcStatsManagerFixture::testFlowStats (MockConnection& portConn,
                                          PolicyStatsManager *statsManager,
                                          bool testAggregate)
{
    // create pod to svc and svc to pod flows
    auto epUuid = ep0->getUUID();
    auto svcUuid = as.getUUID();
    auto epSvcUuid = epUuid + ":" + svcUuid;
    auto epToSvcUuid = "eptosvc:"+epSvcUuid;
    auto svcToEpUuid = "svctoep:"+epSvcUuid;
    uint32_t eptosvcCk =
        idGen.getId(IntFlowManager::getIdNamespace(EpToSvcCounter::CLASS_ID),
                                                   epToSvcUuid);
    uint32_t svctoepCk =
        idGen.getId(IntFlowManager::getIdNamespace(SvcToEpCounter::CLASS_ID),
                                                   svcToEpUuid);

    FlowEntryList entryList;
    int table_id = IntFlowManager::STATS_TABLE_ID;

    address svc1Addr = address::from_string("169.254.169.254");
    address ep1Addr = address::from_string("10.20.44.2");
    address svc2Addr = address::from_string("fe80::a9:fe:a9:fe");
    address ep2Addr = address::from_string("2001:db8::2");

    auto createFlowExpr =
        [&] (address epAddr, address svcAddr, bool isV4) -> void {
        if (isV4) {
            FlowBuilder().priority(100).ethType(eth::type::IP)
                     .ipSrc(epAddr)
                     .reg(8, svcAddr.to_v4().to_ulong())
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(eptosvcCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);

            FlowBuilder().priority(100).ethType(eth::type::IP)
                     .ipSrc(svcAddr)
                     .ipDst(epAddr)
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(svctoepCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);
        } else {
            uint32_t pAddr[4];
            IntFlowManager::in6AddrToLong(svcAddr, &pAddr[0]);
            FlowBuilder().priority(100).ethType(eth::type::IPV6)
                     .ipSrc(epAddr)
                     .reg(8, pAddr[0]).reg(9, pAddr[1])
                     .reg(10, pAddr[2]).reg(11, pAddr[3])
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(eptosvcCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);

            FlowBuilder().priority(100).ethType(eth::type::IPV6)
                     .ipSrc(svcAddr)
                     .ipDst(epAddr)
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(svctoepCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);
        }
    };

    createFlowExpr(ep1Addr, svc1Addr, true);
    if (testAggregate)
        createFlowExpr(ep2Addr, svc2Addr, false);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to setup flow stat state.
    statsManager->on_timer(ec);

    // create first flow reply message
    struct ofpbuf *res_msg = makeFlowStatReplyMessage_2(&portConn,
                                   INITIAL_PACKET_COUNT,
                                   table_id,
                                   entryList);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "1 makeFlowStatsReplyMessage successful";

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_STATS_REPLY,
                         res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handling successful";
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and update Genie objects for stats
    statsManager->on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object
    uint32_t expPkts = 0;
    uint32_t expBytes = 0;
    checkObjectStats(epToSvcUuid, svcToEpUuid, expPkts, expBytes);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&portConn,
                                     FINAL_PACKET_COUNT,
                                     table_id,
                                     entryList);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "2 makeFlowStatReplyMessage successful";

    // send second flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "2 FlowStatsReplyMessage handling successful";
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and update Genie objects for stats
    statsManager->on_timer(ec);

    uint32_t numFlows = entryList.size()/2;
    expPkts =
        (FINAL_PACKET_COUNT - INITIAL_PACKET_COUNT) * numFlows;
    expBytes = expPkts * PACKET_SIZE;

    // Verify the expected packet and byte count
    checkObjectStats(epToSvcUuid,
                    svcToEpUuid,
                    expPkts, expBytes);
    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";
}

void
PodSvcStatsManagerFixture::testFlowRemoved (MockConnection& portConn,
                                            PolicyStatsManager *statsManager,
                                            uint32_t  initPkts)
{

    // create pod to svc and svc to pod flows
    auto epUuid = ep0->getUUID();
    auto svcUuid = as.getUUID();
    auto epSvcUuid = epUuid + ":" + svcUuid;
    auto epToSvcUuid = "eptosvc:"+epSvcUuid;
    auto svcToEpUuid = "svctoep:"+epSvcUuid;
    uint32_t eptosvcCk =
        idGen.getId(IntFlowManager::getIdNamespace(EpToSvcCounter::CLASS_ID),
                                                   epToSvcUuid);
    uint32_t svctoepCk =
        idGen.getId(IntFlowManager::getIdNamespace(SvcToEpCounter::CLASS_ID),
                                                   svcToEpUuid);

    FlowEntryList entryList1, entryList2;
    int table_id = IntFlowManager::STATS_TABLE_ID;
    address svc1Addr = address::from_string("169.254.169.254");
    address ep1Addr = address::from_string("10.20.44.2");

    auto createFlowExpr =
        [&] (bool isEpToSvc) -> void {
        if (isEpToSvc) {
            FlowBuilder().priority(100).ethType(eth::type::IP)
                 .ipSrc(ep1Addr)
                 .reg(8, svc1Addr.to_v4().to_ulong())
                 .flags(OFPUTIL_FF_SEND_FLOW_REM)
                 .cookie(ovs_htonll(uint64_t(eptosvcCk)))
                 .action().go(IntFlowManager::OUT_TABLE_ID)
                 .parent().build(entryList1);
        } else {
            FlowBuilder().priority(100).ethType(eth::type::IP)
                 .ipSrc(svc1Addr)
                 .ipDst(ep1Addr)
                 .flags(OFPUTIL_FF_SEND_FLOW_REM)
                 .cookie(ovs_htonll(uint64_t(svctoepCk)))
                 .action().go(IntFlowManager::OUT_TABLE_ID)
                 .parent().build(entryList2);
        }
    };

    createFlowExpr(true);
    createFlowExpr(false);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to setup flow stat state.
    statsManager->on_timer(ec);

    // create flow removed message 1
    struct ofpbuf *res_msg = makeFlowRemovedMessage_2(&portConn,
                                   LAST_PACKET_COUNT,
                                   table_id,
                                   entryList1);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "1 makeFlowRemovedMessage successful";

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_REMOVED,
                         res_msg);
    LOG(DEBUG) << "1 FlowRemovedMessage handling successful";
    ofpbuf_delete(res_msg);

    // create flow removed message 2
    res_msg = makeFlowRemovedMessage_2(&portConn,
                                   LAST_PACKET_COUNT,
                                   table_id,
                                   entryList2);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "2 makeFlowRemovedMessage successful";

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_REMOVED,
                         res_msg);
    LOG(DEBUG) << "2 FlowRemovedMessage handling successful";
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and update Genie objects for stats
    statsManager->on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object
    uint32_t expPkts = LAST_PACKET_COUNT - initPkts;
    uint32_t expBytes = expPkts * PACKET_SIZE;
    checkObjectStats(epToSvcUuid, svcToEpUuid, expPkts, expBytes);
}


void
PodSvcStatsManagerFixture::checkObjectStats (const std::string& epToSvcUuid,
                                            const std::string& svcToEpUuid,
                                            uint32_t packet_count,
                                            uint32_t byte_count)
{

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    auto aUuid =
        boost::lexical_cast<std::string>(agent.getUuid());

    LOG(DEBUG) << "checkObj expected pkt count: " << packet_count;

    // Note: the objects should always be present. But checking for
    // presence of object, just for safety
    optional<shared_ptr<EpToSvcCounter> > epToSvcCntr;
    epToSvcCntr = su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid);
    if (epToSvcCntr) {
        WAIT_FOR_DO_ONFAIL(
           (epToSvcCntr
                && (epToSvcCntr.get()->getPackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (epToSvcCntr = su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid)),
           BOOST_CHECK_EQUAL(epToSvcCntr.get()->getPackets().get(),
                             packet_count));
        WAIT_FOR_DO_ONFAIL(
           (epToSvcCntr
                && (epToSvcCntr.get()->getBytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (epToSvcCntr = su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid)),
           BOOST_CHECK_EQUAL(epToSvcCntr.get()->getBytes().get(),
                             byte_count));
    } else {
        LOG(ERROR) << "EpToSvcCounter obj not present";
    }

    optional<shared_ptr<SvcToEpCounter> > svcToEpCntr =
        su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid);
    if (svcToEpCntr) {
        WAIT_FOR_DO_ONFAIL(
           (svcToEpCntr
                && (svcToEpCntr.get()->getPackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (svcToEpCntr = su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid)),
           BOOST_CHECK_EQUAL(svcToEpCntr.get()->getPackets().get(),
                             packet_count));
        WAIT_FOR_DO_ONFAIL(
           (svcToEpCntr
                && (svcToEpCntr.get()->getBytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (svcToEpCntr = su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid)),
           BOOST_CHECK_EQUAL(svcToEpCntr.get()->getBytes().get(),
                             byte_count));
    } else {
        LOG(ERROR) << "SvcToEpCounter obj not present";
    }
}

void PodSvcStatsManagerFixture::checkPodSvcObsObj (bool add)
{

    auto epUuid = ep0->getUUID();
    auto svcUuid = as.getUUID();
    auto epSvcUuid = epUuid + ":" + svcUuid;
    auto epToSvcUuid = "eptosvc:"+epSvcUuid;
    auto svcToEpUuid = "svctoep:"+epSvcUuid;

    optional<shared_ptr<PolicyStatUniverse> > su =
        PolicyStatUniverse::resolve(agent.getFramework());

    auto aUuid =
        boost::lexical_cast<std::string>(agent.getUuid());

    LOG(DEBUG) << "Checking presence of"
               << " agentUuid: " << aUuid
               << " epToSvcUuid: " << epToSvcUuid;
    if (add) {
        WAIT_FOR_DO_ONFAIL(su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid), 500,
                            ,LOG(ERROR) << "Obj not resolved";);
    } else {
        WAIT_FOR_DO_ONFAIL(!(su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid)), 500,
                            ,LOG(ERROR) << "Obj still present";);
    }

    LOG(DEBUG) << "Checking presence of"
               << " agentUuid: " << aUuid
               << " svcToEpUuid: " << svcToEpUuid;
    if (add) {
        WAIT_FOR_DO_ONFAIL(su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid), 500,
                            ,LOG(ERROR) << "Obj not resolved";);
    } else {
        WAIT_FOR_DO_ONFAIL(!(su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid)), 500,
                            ,LOG(ERROR) << "Obj still present";);
    }
}

void PodSvcStatsManagerFixture::checkNewFlowMapInitialized (void)
{
    LOG(DEBUG) << "####### START - checkNewFlowMapInitialized"
               << " new: " << podsvcStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << podsvcStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << podsvcStatsManager.statsState.removedFlowCounterMap.size();

    WAIT_FOR_DO_ONFAIL(
        podsvcStatsManager.statsState.newFlowCounterMap.size() == 16, // 6 eps with 6 ipv4 + 2 ipv6
                                                                      // 2 svc mappings with 1 v6 and 1v4
                   500,,
                   LOG(ERROR) << "##### flow state not fully setup ####";);

    LOG(DEBUG) << "####### END - checkNewFlowMapInitialized"
               << " new: " << podsvcStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << podsvcStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << podsvcStatsManager.statsState.removedFlowCounterMap.size();
}

BOOST_AUTO_TEST_SUITE(PodSvcStatsManager_test)



/*
 * 1) verify stats:
 *  - run on_timer to create flows in newMap
 *  - make and handle stats response (new --> old) for flow with 100 pkts : old.diff=0, old.last=100, visited=false
 *  - run on_timer - agg stats update  :  old.diff=0, old.last=100
 *  - check counters (obj: 0)
 *
 * 2) verify stats - 2 (test1+):
 * - make and handle stats response (old --> old) 299 pkts  :  diff=199, last=299, visited=true
 * - run on_timer - agg stats update  :   diff=0 (consumed), last=299, visited=false
 * - check counters (obj:199)
 *
 * 3) test EP agg, SVC agg and both: test1+2 to be run for all these cases separately
 *
 */
BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsNoAgg, PodSvcStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    podsvcStatsManager.registerConnection(&integrationPortConn);
    podsvcStatsManager.start();

    // Ensure the flow stats table is fully setup
    // If there are multiple EPs with multiple IPs and Services with many SMs,
    // then IntFlowManager might take some time to add all the flow combinations
    // per (POD, SVC) combination. on the start of statsManager timer, the flow
    // counter state wont be setup fully for all flows in stats manager.
    // Due to this, stats accounting checks will get impacted.
    // Note: even though we are checking for Observer objects getting created
    // earlier, we can end up in a case where one flow of a (POD,SVC) combination
    // could have contributed to the MO creation, but not all flows for the
    // combination might have been created. hence we need this check.
    checkNewFlowMapInitialized();

    // Check if the below negative cases are fine
    podsvcStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    podsvcStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_STATS_REPLY, NULL);

    LOG(DEBUG) << "############# NOAGG START ############";
    testFlowStats(integrationPortConn, &podsvcStatsManager);
    LOG(DEBUG) << "############# NOAGG END ############";
    podsvcStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsEpSvcAgg, PodSvcStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    podsvcStatsManager.registerConnection(&integrationPortConn);
    podsvcStatsManager.start();
    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# EP+SVC AGG START ############";
    testFlowStats(integrationPortConn, &podsvcStatsManager, true);
    LOG(DEBUG) << "############# EP+SVC AGG END ############";
    podsvcStatsManager.stop();
}

/*
 * Verify aging - can happen when ovs hasnt responded to a stats request for more than
 *                MAX_AGE on_timer iterations
 * - run on_timer to create flows in newMap
 * - make and handle stats response (new --> old) 100 pkts      diff=0, last=100, visited=false
 * - make and handle stats response (old --> old) 299 pkts  :  diff=199, last=299, visited=true
 * - call on_timer: diff=0, last=299, visited=false
 * - check counters (obj:199)
 * - MAX_AGE=9, keep calling on_timer 9 times
 * - verify aging by checking statsMap.old.size, also confirm statsMap.new.size
 * - run on_timer to create flows in newMap  diff=0,last=0
 * - check new map size
 */
BOOST_FIXTURE_TEST_CASE(testFlowAgeOld, PodSvcStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    podsvcStatsManager.registerConnection(&integrationPortConn);
    podsvcStatsManager.start();

    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# AGE OLD START ############";
    testFlowStats(integrationPortConn, &podsvcStatsManager);
    testFlowAge(&podsvcStatsManager, true, false);
    testFlowAge(&podsvcStatsManager, true, true);
    LOG(DEBUG) << "############# AGE OLD END ############";
    podsvcStatsManager.stop();
}

/*
 * Verify aging:
 * - run on_timer to create flows in newMap
 * - MAX_AGE=9, keep calling on_timer 9 times
 * - verify aging by checking statsMap.new.size
 * - check new map size. All entries must have aged.
 * - call on_timer again and check that all entries have been added to newMap.
 */
BOOST_FIXTURE_TEST_CASE(testFlowAgeNew, PodSvcStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    podsvcStatsManager.registerConnection(&integrationPortConn);
    podsvcStatsManager.start();

    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# AGE NEW START ############";
    testFlowAge(&podsvcStatsManager, false, false);
    testFlowAge(&podsvcStatsManager, false, true);
    LOG(DEBUG) << "############# AGE NEW END ############";
    podsvcStatsManager.stop();
}

/*
 * flows removed - newMap to RemovedMap:
 * - run on_timer to add flows to newMap     diffs=0, last=0
 * - handle flow removed with 100 pkts (new to remMap)  diffs=100, last = 0
 * - run on_timer to aggregate stats         entry removed from remMap
 * - check counters (obj:100)
 */
BOOST_FIXTURE_TEST_CASE(testFlowRemovedNew, PodSvcStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    podsvcStatsManager.registerConnection(&integrationPortConn);
    podsvcStatsManager.start();

    checkNewFlowMapInitialized();

    // Check if the below negative cases are fine
    podsvcStatsManager.Handle(NULL, OFPTYPE_FLOW_REMOVED, NULL);
    podsvcStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_REMOVED, NULL);

    LOG(DEBUG) << "############# REMOVEDNEW START ############";
    testFlowRemoved(integrationPortConn, &podsvcStatsManager, 0);
    LOG(DEBUG) << "############# REMOVEDNEW END ############";
    podsvcStatsManager.stop();
}

/*
 * flows removed - oldMap to RemovedMap:
 * - run on_timer to add flows to newMap     diffs=0, last=0
 * - make and handle stats response (new --> old) 100 pkts      diff=0, last=100, visited=false
 * - make and handle stats response (old --> old) 299 pkts  :  diff=199, last=299, visited=true
 * - run on_timer to aggregate counts => old.diff=0, last=299
 * - check counters (obj: 199)
 * - handle flow removed with 350 pkts (old --> remMap)    rem.diff = old.diff + flowCount-old.last = 151, last=0
 * - run on_timer to aggregate stats       entry removed from remMap
 * - check counters (obj: 199 + 51 = 250)
 *
 */
BOOST_FIXTURE_TEST_CASE(testFlowRemovedOld, PodSvcStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    podsvcStatsManager.registerConnection(&integrationPortConn);
    podsvcStatsManager.start();

    checkNewFlowMapInitialized();

    LOG(DEBUG) << "############# REMOVEDOLD START ############";
    testFlowStats(integrationPortConn, &podsvcStatsManager);
    testFlowRemoved(integrationPortConn, &podsvcStatsManager,
                    INITIAL_PACKET_COUNT);
    LOG(DEBUG) << "############# REMOVEDOLD END ############";
    podsvcStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
