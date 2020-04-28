/*
 * Test suite for class ServiceStatsManager.
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

#include <opflexagent/Agent.h>
#include <modelgbp/gbpe/SvcToEpCounter.hpp>
#include <modelgbp/gbpe/EpToSvcCounter.hpp>
#include <opflexagent/logging.h>
#include <opflexagent/test/ModbFixture.h>
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "IntFlowManager.h"
#include "ServiceStatsManager.h"
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
using modelgbp::observer::SvcStatUniverse;

namespace opflexagent {

static const uint32_t LAST_PACKET_COUNT = 350; // for removed flow entry

class ServiceStatsManagerFixture : public PolicyStatsManagerFixture {

public:
    ServiceStatsManagerFixture() : PolicyStatsManagerFixture(),
                                    intFlowManager(agent, switchManager, idGen,
                                                   ctZoneManager, tunnelEpManager),
                                    pktInHandler(agent, intFlowManager),
                                    serviceStatsManager(&agent, idGen,
                                                       switchManager,
                                                       intFlowManager, 10) {
        createObjects();
        switchManager.setMaxFlowTables(IntFlowManager::NUM_FLOW_TABLES);
        intFlowManager.start();

        setLoggingLevel("trace");
        // cloud nodeport tests need veth_host_ac
        portmapper.setPort("veth_host_ac", 72);
        portmapper.setPort(72, "veth_host_ac");
        createVethHostAccessObjects();
        intFlowManager.endpointUpdated(vethhostac->getUUID());

        LOG(DEBUG) << "############# SERVICE CREATE START ############";
        createServices();
        checkPodSvcObsObj(true);
        checkSvcTgtObsObj(true);
        LOG(DEBUG) << "############# SERVICE CREATE END ############";
    }

    virtual ~ServiceStatsManagerFixture() {
        checkPodSvcObsObj(true);
        checkSvcTgtObsObj(true);
        LOG(DEBUG) << "############# SERVICE DELETE START ############";
        removeServiceObjects();
        checkPodSvcObsObj(false);
        checkSvcTgtObsObj(false);
        LOG(DEBUG) << "############# SERVICE DELETE END ############";
        intFlowManager.stop();
        stop();
    }

    IntFlowManager  intFlowManager;
    PacketInHandler pktInHandler;
    ServiceStatsManager serviceStatsManager;
    void testFlowStatsPodSvc(MockConnection& portConn,
                             PolicyStatsManager *statsManager,
                             bool testAggregate=false);
    void testFlowStatsSvcTgt(MockConnection& portConn,
                             PolicyStatsManager *statsManager,
                             const string& nhip,
                             bool isNodePort=false);
    void testFlowRemovedPodSvc(MockConnection& portConn,
                               PolicyStatsManager *statsManager,
                               uint32_t initPkts);
    void testFlowRemovedSvcTgt(MockConnection& portConn,
                               PolicyStatsManager *statsManager,
                               uint32_t initPkts,
                               const string& nhip,
                               bool isNodePort=false);
    void checkPodSvcObsObj(bool);
    void checkSvcTgtObsObj(bool);
    void removeServiceObjects(void);
    void checkNewFlowMapInitialized();
    void testFlowAge(PolicyStatsManager *statsManager,
                     bool isOld, bool isFlowStateReAdd);

#ifdef HAVE_PROMETHEUS_SUPPORT
    const string cmd = "curl --proxy \"\" --compressed --silent http://127.0.0.1:9612/metrics 2>&1;";
    void checkSvcTgtPromMetrics(uint64_t pkts, uint64_t bytes, const string& ip, bool isNodePort=false);
    void checkPodSvcPromMetrics(uint64_t pkts, uint64_t bytes);
#endif
private:
    Service as;
    Service::ServiceMapping sm1;
    Service::ServiceMapping sm2;
    void createServices(void);
    void checkPodSvcObjectStats(const std::string& epToSvcUuid,
                                const std::string& svcToEpUuid,
                                uint32_t packet_count,
                                uint32_t byte_count);
    void checkAnySvcTgtObjectStats(const std::string& svcUuid,
                                   const std::string& nhip,
                                   uint32_t packet_count,
                                   uint32_t byte_count);
    void checkNodeSvcTgtObjectStats(const std::string& svcUuid,
                                    const std::string& nhip,
                                    uint32_t packet_count,
                                    uint32_t byte_count);
    bool checkNewFlowMapSize(void);
};

/*
 * Service delete
 * - delete svc objects
 * - check if observer objects go away from caller
 */
void ServiceStatsManagerFixture::removeServiceObjects (void)
{
    servSrc.removeService(as.getUUID());
    intFlowManager.serviceUpdated(as.getUUID());
}

void ServiceStatsManagerFixture::createServices (void)
{
    intFlowManager.egDomainUpdated(epg0->getURI());
    intFlowManager.domainUpdated(RoutingDomain::CLASS_ID, rd0->getURI());

    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setDomainURI(URI(rd0->getURI()));
    as.setServiceMode(Service::LOADBALANCER);

    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.addNextHopIP("10.20.44.2");
    sm1.addNextHopIP("169.254.169.2");
    sm1.setServicePort(53);
    sm1.setNextHopPort(5353);
    sm1.setNodePort(32000);
    as.addServiceMapping(sm1);

    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.addNextHopIP("2001:db8::2");
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    sm2.setServicePort(80);
    sm1.setNodePort(32001);
    as.addServiceMapping(sm2);

    as.addAttribute("name", "coredns");
    as.addAttribute("scope", "cluster");
    as.addAttribute("namespace", "kube-system");

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
 * on_timer from client (Contract, Service, SecGrp):
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
ServiceStatsManagerFixture::testFlowAge (PolicyStatsManager *statsManager,
                                        bool isOld,
                                        bool isFlowStateReAdd)
{
    std::unique_lock<std::mutex> guard(serviceStatsManager.pstatMtx);
    LOG(DEBUG) << "####### START - testFlowAge " << isOld << " " << isFlowStateReAdd
               << " new: " << serviceStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << serviceStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << serviceStatsManager.statsState.removedFlowCounterMap.size();
    guard.unlock();

    if (isOld && !isFlowStateReAdd) {
        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.oldFlowCounterMap.size(), 2);
        guard.unlock();

        for (auto age = 0; age < PolicyStatsManager::MAX_AGE; age++) {
            boost::system::error_code ec;
            ec = make_error_code(boost::system::errc::success);
            statsManager->on_timer(ec);
        }

        // 2 flows got aged
        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.oldFlowCounterMap.size(), 0);
        guard.unlock();
    }

    if (isOld && isFlowStateReAdd) {
        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.oldFlowCounterMap.size(), 0);
        // 17 flows based on config, -2 aged flows
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.newFlowCounterMap.size(), 15);
        guard.unlock();

        boost::system::error_code ec;
        ec = make_error_code(boost::system::errc::success);
        statsManager->on_timer(ec);

        // 2 flows get readded to new map
        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.oldFlowCounterMap.size(), 0);
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.newFlowCounterMap.size(), 17);
        guard.unlock();
    }

    if (!isOld && !isFlowStateReAdd) {
        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.newFlowCounterMap.size(), 17);
        guard.unlock();

        for (auto age = 0; age < PolicyStatsManager::MAX_AGE; age++) {
            boost::system::error_code ec;
            ec = make_error_code(boost::system::errc::success);
            statsManager->on_timer(ec);
        }

        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.newFlowCounterMap.size(), 0);
        guard.unlock();
    }

    if (!isOld && isFlowStateReAdd) {
        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.newFlowCounterMap.size(), 0);
        guard.unlock();

        boost::system::error_code ec;
        ec = make_error_code(boost::system::errc::success);
        statsManager->on_timer(ec);

        guard.lock();
        BOOST_CHECK_EQUAL(serviceStatsManager.statsState.newFlowCounterMap.size(), 17);
        guard.unlock();
    }

    guard.lock();
    LOG(DEBUG) << "####### END - testFlowAge " << isOld << " " << isFlowStateReAdd
               << " new: " << serviceStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << serviceStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << serviceStatsManager.statsState.removedFlowCounterMap.size();
    guard.unlock();
}

void
ServiceStatsManagerFixture::testFlowStatsSvcTgt (MockConnection& portConn,
                                                PolicyStatsManager *statsManager,
                                                const string& nhip,
                                                bool isNodePort)
{
    // create */node to svc-tgt and svc-tgt to */node flows
    const auto& svcUuid = as.getUUID();
    const string& iPfx = isNodePort?"notosvc:svc-nod:":"antosvc:svc-tgt:";
    const string& ePfx = isNodePort?"svctono:svc-nod:":"svctoan:svc-tgt:";
    const auto& iKey = iPfx+svcUuid+":"+nhip;
    const auto& eKey = ePfx+svcUuid+":"+nhip;
    uint32_t iSvcCk =
        idGen.getId(IntFlowManager::getIdNamespace(SvcTargetCounter::CLASS_ID),
                                                   iKey);
    uint32_t eSvcCk =
        idGen.getId(IntFlowManager::getIdNamespace(SvcTargetCounter::CLASS_ID),
                                                   eKey);

    FlowEntryList entryList;
    int table_id = IntFlowManager::STATS_TABLE_ID;

    auto createFlowExpr =
        [&] (address nhAddr, bool isNodePort) -> void {
        if (isNodePort) {
            FlowBuilder().proto(17).tpDst(5353)
                     .priority(98).ethType(eth::type::IP)
                     .ipSrc(address::from_string("1.100.201.11"))
                     .ipDst(nhAddr)
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(iSvcCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);

            FlowBuilder().proto(17).tpSrc(5353)
                     .priority(98).ethType(eth::type::IP)
                     .ipSrc(nhAddr)
                     .ipDst(address::from_string("1.100.201.11"))
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(eSvcCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);
        } else {
            if (nhAddr.is_v4()) {
                FlowBuilder().proto(17).tpDst(5353)
                         .priority(97).ethType(eth::type::IP)
                         .ipDst(nhAddr).reg(12, 0, 1<<31)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(iSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList);

                FlowBuilder().proto(17).tpSrc(5353)
                         .priority(97).ethType(eth::type::IP)
                         .ipSrc(nhAddr).reg(12, 0, 1<<31)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(eSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList);
            } else {
                FlowBuilder().proto(6).tpDst(80)
                         .priority(97).ethType(eth::type::IPV6)
                         .ipDst(nhAddr).reg(12, 0, 1<<31)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(iSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList);

                FlowBuilder().proto(6).tpSrc(80)
                         .priority(97).ethType(eth::type::IPV6)
                         .ipSrc(nhAddr).reg(12, 0, 1<<31)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(eSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList);
            }
        }
    };

    address nhAddr = address::from_string(nhip);
    createFlowExpr(nhAddr, isNodePort);

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
    ofp_header *msgHdr = (ofp_header *)res_msg->data;
    statsManager->testInjectTxnId(msgHdr->xid);

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
    if (isNodePort)
        checkNodeSvcTgtObjectStats(svcUuid, nhip, expPkts, expBytes);
    else
        checkAnySvcTgtObjectStats(svcUuid, nhip, expPkts, expBytes);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&portConn,
                                     FINAL_PACKET_COUNT,
                                     table_id,
                                     entryList);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "2 makeFlowStatReplyMessage successful";
    msgHdr = (ofp_header *)res_msg->data;
    statsManager->testInjectTxnId(msgHdr->xid);

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
    if (isNodePort)
        checkNodeSvcTgtObjectStats(svcUuid, nhip, expPkts, expBytes);
    else
        checkAnySvcTgtObjectStats(svcUuid, nhip, expPkts, expBytes);
    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";
}

void
ServiceStatsManagerFixture::testFlowStatsPodSvc (MockConnection& portConn,
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

    // Using EP IPs that arent the next-hops of the created services
    address svc1Addr = address::from_string("169.254.169.254");
    address ep1Addr = address::from_string("10.20.44.3");
    address svc2Addr = address::from_string("fe80::a9:fe:a9:fe");
    address ep2Addr = address::from_string("2001:db8::3");

    auto createFlowExpr =
        [&] (address epAddr, address svcAddr, bool isV4) -> void {
        if (isV4) {
            FlowBuilder().proto(17).tpDst(5353)
                     .priority(100).ethType(eth::type::IP)
                     .ipSrc(epAddr)
                     .reg(8, svcAddr.to_v4().to_ulong())
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(eptosvcCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);

            FlowBuilder().proto(17).tpSrc(53)
                     .priority(100).ethType(eth::type::IP)
                     .ipSrc(svcAddr)
                     .ipDst(epAddr)
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(svctoepCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);
        } else {
            uint32_t pAddr[4];
            IntFlowManager::in6AddrToLong(svcAddr, &pAddr[0]);
            FlowBuilder().proto(6).tpDst(80)
                     .priority(100).ethType(eth::type::IPV6)
                     .ipSrc(epAddr)
                     .reg(8, pAddr[0]).reg(9, pAddr[1])
                     .reg(10, pAddr[2]).reg(11, pAddr[3])
                     .flags(OFPUTIL_FF_SEND_FLOW_REM)
                     .cookie(ovs_htonll(uint64_t(eptosvcCk)))
                     .action().go(IntFlowManager::OUT_TABLE_ID)
                     .parent().build(entryList);

            FlowBuilder().proto(6).tpSrc(80)
                     .priority(100).ethType(eth::type::IPV6)
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
    ofp_header *msgHdr = (ofp_header *)res_msg->data;
    statsManager->testInjectTxnId(msgHdr->xid);

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
    checkPodSvcObjectStats(epToSvcUuid, svcToEpUuid, expPkts, expBytes);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&portConn,
                                     FINAL_PACKET_COUNT,
                                     table_id,
                                     entryList);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "2 makeFlowStatReplyMessage successful";
    msgHdr = (ofp_header *)res_msg->data;
    statsManager->testInjectTxnId(msgHdr->xid);

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
    checkPodSvcObjectStats(epToSvcUuid,
                           svcToEpUuid,
                           expPkts, expBytes);
    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";
}

void
ServiceStatsManagerFixture::testFlowRemovedSvcTgt (MockConnection& portConn,
                                                  PolicyStatsManager *statsManager,
                                                  uint32_t  initPkts,
                                                  const string& nhip,
                                                  bool isNodePort)
{
    // create */node to svc-tgt and svc-tgt to */node flows
    const auto& svcUuid = as.getUUID();
    const string& iPfx = isNodePort?"notosvc:svc-nod:":"antosvc:svc-tgt:";
    const string& ePfx = isNodePort?"svctono:svc-nod:":"svctoan:svc-tgt:";
    const auto& iKey = iPfx+svcUuid+":"+nhip;
    const auto& eKey = ePfx+svcUuid+":"+nhip;
    uint32_t iSvcCk =
        idGen.getId(IntFlowManager::getIdNamespace(SvcTargetCounter::CLASS_ID),
                                                   iKey);
    uint32_t eSvcCk =
        idGen.getId(IntFlowManager::getIdNamespace(SvcTargetCounter::CLASS_ID),
                                                   eKey);

    FlowEntryList entryList1, entryList2;
    int table_id = IntFlowManager::STATS_TABLE_ID;
    address nhAddr = address::from_string(nhip);

    auto createFlowExpr =
        [&] (bool isIngress, bool isNodePort) -> void {
        if (isNodePort) {
            if (isIngress) {
                FlowBuilder().proto(17).tpDst(5353)
                         .priority(98).ethType(eth::type::IP)
                         .ipSrc(address::from_string("1.100.201.11"))
                         .ipDst(nhAddr)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(iSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList1);
            } else {
                FlowBuilder().proto(17).tpSrc(5353)
                         .priority(98).ethType(eth::type::IP)
                         .ipSrc(nhAddr)
                         .ipDst(address::from_string("1.100.201.11"))
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(eSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList2);
            }
        } else {
            if (isIngress) {
                FlowBuilder().proto(17).tpDst(5353)
                         .priority(97).ethType(eth::type::IP)
                         .ipDst(nhAddr).reg(12, 0, 1<<31)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(iSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList1);
            } else {
                FlowBuilder().proto(17).tpSrc(5353)
                         .priority(97).ethType(eth::type::IP)
                         .ipSrc(nhAddr).reg(12, 0, 1<<31)
                         .flags(OFPUTIL_FF_SEND_FLOW_REM)
                         .cookie(ovs_htonll(uint64_t(eSvcCk)))
                         .action().go(IntFlowManager::OUT_TABLE_ID)
                         .parent().build(entryList2);
            }
        }
    };

    createFlowExpr(true, isNodePort);
    createFlowExpr(false, isNodePort);

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
    struct ofputil_flow_removed fentry;
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_REMOVED,
                         res_msg,
                         &fentry);
    LOG(DEBUG) << "1 FlowRemovedMessage handling successful";
    ofpbuf_delete(res_msg);

    // create flow removed message 2
    res_msg = makeFlowRemovedMessage_2(&portConn,
                                   LAST_PACKET_COUNT,
                                   table_id,
                                   entryList2);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "2 makeFlowRemovedMessage successful";
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_REMOVED,
                         res_msg,
                         &fentry);
    LOG(DEBUG) << "2 FlowRemovedMessage handling successful";
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and update Genie objects for stats
    statsManager->on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object
    uint32_t expPkts = LAST_PACKET_COUNT - initPkts;
    uint32_t expBytes = expPkts * PACKET_SIZE;
    if (isNodePort)
        checkNodeSvcTgtObjectStats(svcUuid, nhip, expPkts, expBytes);
    else
        checkAnySvcTgtObjectStats(svcUuid, nhip, expPkts, expBytes);
}

void
ServiceStatsManagerFixture::testFlowRemovedPodSvc (MockConnection& portConn,
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
    // Using an EP IP that isnt the next-hop
    address ep1Addr = address::from_string("10.20.44.3");

    auto createFlowExpr =
        [&] (bool isEpToSvc) -> void {
        if (isEpToSvc) {
            FlowBuilder().proto(17).tpDst(5353)
                 .priority(100).ethType(eth::type::IP)
                 .ipSrc(ep1Addr)
                 .reg(8, svc1Addr.to_v4().to_ulong())
                 .flags(OFPUTIL_FF_SEND_FLOW_REM)
                 .cookie(ovs_htonll(uint64_t(eptosvcCk)))
                 .action().go(IntFlowManager::OUT_TABLE_ID)
                 .parent().build(entryList1);
        } else {
            FlowBuilder().proto(17).tpSrc(53)
                 .priority(100).ethType(eth::type::IP)
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
    struct ofputil_flow_removed fentry;
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_REMOVED,
                         res_msg,
                         &fentry);
    LOG(DEBUG) << "1 FlowRemovedMessage handling successful";
    ofpbuf_delete(res_msg);

    // create flow removed message 2
    res_msg = makeFlowRemovedMessage_2(&portConn,
                                   LAST_PACKET_COUNT,
                                   table_id,
                                   entryList2);
    BOOST_REQUIRE(res_msg!=0);
    LOG(DEBUG) << "2 makeFlowRemovedMessage successful";
    SwitchConnection::DecodeFlowRemoved(res_msg, &fentry);

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                         OFPTYPE_FLOW_REMOVED,
                         res_msg,
                         &fentry);
    LOG(DEBUG) << "2 FlowRemovedMessage handling successful";
    ofpbuf_delete(res_msg);

    // Call on_timer function to process the stats collected
    // and update Genie objects for stats
    statsManager->on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object
    uint32_t expPkts = LAST_PACKET_COUNT - initPkts;
    uint32_t expBytes = expPkts * PACKET_SIZE;
    checkPodSvcObjectStats(epToSvcUuid, svcToEpUuid, expPkts, expBytes);
}

#ifdef HAVE_PROMETHEUS_SUPPORT
// Check prom dyn gauge service metrics along with stats
void ServiceStatsManagerFixture::checkSvcTgtPromMetrics (uint64_t pkts,
                                                         uint64_t bytes,
                                                         const string& ip,
                                                         bool isNodePort)
{
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    const string& scope = isNodePort?"nodePort":"cluster";
    const string& s_rx_bytes = "opflex_svc_rx_bytes{name=\"coredns\"" \
                               ",namespace=\"kube-system\",scope=\"" + scope + "\"} "
                            + boost::lexical_cast<std::string>(bytes) + ".000000";
    const string& s_rx_pkts = "opflex_svc_rx_packets{name=\"coredns\"" \
                              ",namespace=\"kube-system\",scope=\"" + scope + "\"} "
                            + boost::lexical_cast<std::string>(pkts) + ".000000";
    const string& s_tx_bytes = "opflex_svc_tx_bytes{name=\"coredns\"" \
                               ",namespace=\"kube-system\",scope=\"" + scope + "\"} "
                            + boost::lexical_cast<std::string>(bytes) + ".000000";
    const string& s_tx_pkts = "opflex_svc_tx_packets{name=\"coredns\"" \
                              ",namespace=\"kube-system\",scope=\"" + scope + "\"} "
                            + boost::lexical_cast<std::string>(pkts) + ".000000";
    pos = output.find(s_rx_bytes);
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find(s_rx_pkts);
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find(s_tx_bytes);
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find(s_tx_pkts);
    BOOST_CHECK_NE(pos, std::string::npos);

    const string& ep_attr = "ep_name=\"coredns\",ep_namespace=\"default\",";
    const string& s_ann = "\",svc_name=\"coredns\",svc_namespace=\"kube-system\"" \
                          ",svc_scope=\"" + scope + "\"} ";
    string rx_bytes, rx_pkts, tx_bytes, tx_pkts;
    if (ip == "10.20.44.2" || ip == "2001:db8::2") {
        rx_bytes = "opflex_svc_target_rx_bytes{" + ep_attr + "ip=\"";
        rx_pkts = "opflex_svc_target_rx_packets{" + ep_attr + "ip=\"";
        tx_bytes = "opflex_svc_target_tx_bytes{" + ep_attr + "ip=\"";
        tx_pkts = "opflex_svc_target_tx_packets{" + ep_attr + "ip=\"";
    } else {
        rx_bytes = "opflex_svc_target_rx_bytes{ip=\"";
        rx_pkts = "opflex_svc_target_rx_packets{ip=\"";
        tx_bytes = "opflex_svc_target_tx_bytes{ip=\"";
        tx_pkts = "opflex_svc_target_tx_packets{ip=\"";
    }
    const string& st_rx_bytes = rx_bytes + ip + s_ann
                            + boost::lexical_cast<std::string>(bytes) + ".000000";
    const string& st_rx_pkts = rx_pkts + ip + s_ann
                            + boost::lexical_cast<std::string>(pkts) + ".000000";
    const string& st_tx_bytes = tx_bytes + ip + s_ann
                            + boost::lexical_cast<std::string>(bytes) + ".000000";
    const string& st_tx_pkts = tx_pkts + ip + s_ann
                            + boost::lexical_cast<std::string>(pkts) + ".000000";
    pos = output.find(st_rx_bytes);
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find(st_rx_pkts);
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find(st_tx_bytes);
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find(st_tx_pkts);
    BOOST_CHECK_NE(pos, std::string::npos);
}

// Check prom dyn gauge pod<-->service metrics along with stats
void ServiceStatsManagerFixture::checkPodSvcPromMetrics (uint64_t pkts,
                                                         uint64_t bytes)
{
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    const string& rx_bytes = "opflex_endpoint_to_svc_bytes{ep_name=\"coredns\"," \
                             "ep_namespace=\"default\",svc_name=\"coredns\"," \
                             "svc_namespace=\"kube-system\",svc_scope=\"cluster\"} " \
                            + boost::lexical_cast<std::string>(bytes) + ".000000";
    const string& rx_pkts = "opflex_endpoint_to_svc_packets{ep_name=\"coredns\"," \
                             "ep_namespace=\"default\",svc_name=\"coredns\"," \
                             "svc_namespace=\"kube-system\",svc_scope=\"cluster\"} " \
                            + boost::lexical_cast<std::string>(pkts) + ".000000";
    const string& tx_bytes = "opflex_svc_to_endpoint_bytes{ep_name=\"coredns\"," \
                             "ep_namespace=\"default\",svc_name=\"coredns\"," \
                             "svc_namespace=\"kube-system\",svc_scope=\"cluster\"} " \
                            + boost::lexical_cast<std::string>(bytes) + ".000000";
    const string& tx_pkts = "opflex_svc_to_endpoint_packets{ep_name=\"coredns\"," \
                             "ep_namespace=\"default\",svc_name=\"coredns\"," \
                             "svc_namespace=\"kube-system\",svc_scope=\"cluster\"} " \
                            + boost::lexical_cast<std::string>(pkts) + ".000000";
    size_t pos1 = output.find(rx_bytes);
    size_t pos2 = output.find(rx_pkts);
    size_t pos3 = output.find(tx_bytes);
    size_t pos4 = output.find(tx_pkts);
    if (!pkts) {
        BOOST_CHECK_EQUAL(pos1, std::string::npos);
        BOOST_CHECK_EQUAL(pos2, std::string::npos);
        BOOST_CHECK_EQUAL(pos3, std::string::npos);
        BOOST_CHECK_EQUAL(pos4, std::string::npos);
    } else {
        BOOST_CHECK_NE(pos1, std::string::npos);
        BOOST_CHECK_NE(pos2, std::string::npos);
        BOOST_CHECK_NE(pos3, std::string::npos);
        BOOST_CHECK_NE(pos4, std::string::npos);
    }
}
#endif

void
ServiceStatsManagerFixture::checkNodeSvcTgtObjectStats (const std::string& svcUuid,
                                                        const std::string& nhip,
                                                        uint32_t packet_count,
                                                        uint32_t byte_count)
{
    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());

    LOG(DEBUG) << "checkObj expected pkt count: " << packet_count;

    // Note: the objects should always be present. But checking for
    // presence of object, just for safety
    optional<shared_ptr<SvcCounter> > opSvcCntr
                        = su.get()->resolveGbpeSvcCounter(svcUuid);
    BOOST_CHECK(opSvcCntr);
    if (opSvcCntr) {
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getNodePortRxpackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getNodePortRxpackets().get(), packet_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getNodePortRxbytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getNodePortRxbytes().get(), byte_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getNodePortTxpackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getNodePortTxpackets().get(), packet_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getNodePortTxbytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getNodePortTxbytes().get(), byte_count);
           });

        optional<shared_ptr<SvcTargetCounter> > opSvcTgtCntr
                            = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip);
        BOOST_CHECK(opSvcTgtCntr);
        if (opSvcTgtCntr) {
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getNodePortRxpackets().get() == packet_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getNodePortRxpackets().get(), packet_count);
               });
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getNodePortRxbytes().get() == byte_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getNodePortRxbytes().get(), byte_count);
               });
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getNodePortTxpackets().get() == packet_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getNodePortTxpackets().get(), packet_count);
               });
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getNodePortTxbytes().get() == byte_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getNodePortTxbytes().get(), byte_count);
               });
        }
#ifdef HAVE_PROMETHEUS_SUPPORT
        checkSvcTgtPromMetrics(packet_count, byte_count, nhip, true);
#endif
    }
}

void
ServiceStatsManagerFixture::checkAnySvcTgtObjectStats (const std::string& svcUuid,
                                                       const std::string& nhip,
                                                       uint32_t packet_count,
                                                       uint32_t byte_count)
{
    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());

    LOG(DEBUG) << "checkObj expected pkt count: " << packet_count;

    // Note: the objects should always be present. But checking for
    // presence of object, just for safety
    optional<shared_ptr<SvcCounter> > opSvcCntr
                        = su.get()->resolveGbpeSvcCounter(svcUuid);
    BOOST_CHECK(opSvcCntr);
    if (opSvcCntr) {
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getRxpackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getRxpackets().get(), packet_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getRxbytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getRxbytes().get(), byte_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getTxpackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getTxpackets().get(), packet_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (opSvcCntr
                && (opSvcCntr.get()->getTxbytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (opSvcCntr = su.get()->resolveGbpeSvcCounter(svcUuid)),
           if (opSvcCntr) {
               BOOST_CHECK_EQUAL(opSvcCntr.get()->getTxbytes().get(), byte_count);
           });

        optional<shared_ptr<SvcTargetCounter> > opSvcTgtCntr
                            = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip);
        BOOST_CHECK(opSvcTgtCntr);
        if (opSvcTgtCntr) {
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getRxpackets().get() == packet_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getRxpackets().get(), packet_count);
               });
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getRxbytes().get() == byte_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getRxbytes().get(), byte_count);
               });
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getTxpackets().get() == packet_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getTxpackets().get(), packet_count);
               });
            WAIT_FOR_DO_ONFAIL(
               (opSvcTgtCntr
                    && (opSvcTgtCntr.get()->getTxbytes().get() == byte_count)),
               500, // usleep(1000) * 500 = 500ms
               (opSvcTgtCntr = opSvcCntr.get()->resolveGbpeSvcTargetCounter(nhip)),
               if (opSvcTgtCntr) {
                   BOOST_CHECK_EQUAL(opSvcTgtCntr.get()->getTxbytes().get(), byte_count);
               });
        }
#ifdef HAVE_PROMETHEUS_SUPPORT
        checkSvcTgtPromMetrics(packet_count, byte_count, nhip);
#endif
    }
}

void
ServiceStatsManagerFixture::checkPodSvcObjectStats (const std::string& epToSvcUuid,
                                                   const std::string& svcToEpUuid,
                                                   uint32_t packet_count,
                                                   uint32_t byte_count)
{
    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());

    auto aUuid =
        boost::lexical_cast<std::string>(agent.getUuid());

    LOG(DEBUG) << "checkObj expected pkt count: " << packet_count;

    // Note: the objects should always be present. But checking for
    // presence of object, just for safety
    optional<shared_ptr<EpToSvcCounter> > epToSvcCntr;
    epToSvcCntr = su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid);
    BOOST_CHECK(epToSvcCntr);
    if (epToSvcCntr) {
        WAIT_FOR_DO_ONFAIL(
           (epToSvcCntr
                && (epToSvcCntr.get()->getPackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (epToSvcCntr = su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid)),
           if (epToSvcCntr) {
               BOOST_CHECK_EQUAL(epToSvcCntr.get()->getPackets().get(), packet_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (epToSvcCntr
                && (epToSvcCntr.get()->getBytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (epToSvcCntr = su.get()->resolveGbpeEpToSvcCounter(aUuid, epToSvcUuid)),
           if (epToSvcCntr) {
               BOOST_CHECK_EQUAL(epToSvcCntr.get()->getBytes().get(), byte_count);
           });
    } else {
        LOG(ERROR) << "EpToSvcCounter obj not present";
    }

    optional<shared_ptr<SvcToEpCounter> > svcToEpCntr =
        su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid);
    BOOST_CHECK(svcToEpCntr);
    if (svcToEpCntr) {
        WAIT_FOR_DO_ONFAIL(
           (svcToEpCntr
                && (svcToEpCntr.get()->getPackets().get() == packet_count)),
           500, // usleep(1000) * 500 = 500ms
           (svcToEpCntr = su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid)),
           if (svcToEpCntr) {
               BOOST_CHECK_EQUAL(svcToEpCntr.get()->getPackets().get(), packet_count);
           });
        WAIT_FOR_DO_ONFAIL(
           (svcToEpCntr
                && (svcToEpCntr.get()->getBytes().get() == byte_count)),
           500, // usleep(1000) * 500 = 500ms
           (svcToEpCntr = su.get()->resolveGbpeSvcToEpCounter(aUuid, svcToEpUuid)),
           if (svcToEpCntr) {
               BOOST_CHECK_EQUAL(svcToEpCntr.get()->getBytes().get(), byte_count);
           });
    } else {
        LOG(ERROR) << "SvcToEpCounter obj not present";
    }
#ifdef HAVE_PROMETHEUS_SUPPORT
    checkPodSvcPromMetrics(packet_count, byte_count);
#endif
}

void ServiceStatsManagerFixture::checkSvcTgtObsObj (bool add)
{
    auto svcUuid = as.getUUID();
    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());

    LOG(DEBUG) << "Checking presence of svc counter svcUuid: " << svcUuid;
    if (add) {
        WAIT_FOR_DO_ONFAIL(su.get()->resolveGbpeSvcCounter(svcUuid), 500,,
                    LOG(ERROR) << "Obj not resolved svcUuid: " << svcUuid;);
        for (const auto& sm : as.getServiceMappings()) {
            for (const auto& ip : sm.getNextHopIPs()) {
                WAIT_FOR_DO_ONFAIL(SvcTargetCounter::resolve(agent.getFramework(), svcUuid, ip),
                        500,,
                        LOG(ERROR) << "SvcTargetCounter obs Obj not resolved" << ip;);
            }
        }
    } else {
        WAIT_FOR_DO_ONFAIL(!(su.get()->resolveGbpeSvcCounter(svcUuid)), 500,,
                    LOG(ERROR) << "Obj still present svcUuid" << svcUuid;);
        for (const auto& sm : as.getServiceMappings()) {
            for (const auto& ip : sm.getNextHopIPs()) {
                WAIT_FOR_DO_ONFAIL(!SvcTargetCounter::resolve(agent.getFramework(), svcUuid, ip),
                        500,,
                        LOG(ERROR) << "SvcTargetCounter obs Obj not resolved" << ip;);
            }
        }
    }
}

void ServiceStatsManagerFixture::checkPodSvcObsObj (bool add)
{
    auto epUuid = ep0->getUUID();
    auto svcUuid = as.getUUID();
    auto epSvcUuid = epUuid + ":" + svcUuid;
    auto epToSvcUuid = "eptosvc:"+epSvcUuid;
    auto svcToEpUuid = "svctoep:"+epSvcUuid;

    optional<shared_ptr<SvcStatUniverse> > su =
        SvcStatUniverse::resolve(agent.getFramework());

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

bool ServiceStatsManagerFixture::checkNewFlowMapSize (void)
{
    std::lock_guard<std::mutex> lock(serviceStatsManager.pstatMtx);
    // We have 4 local eps with "5 ipv4" + "2 ipv6".
    // There is veth_host_ac as well with 1 ipv4
    // and 2 svc mappings with "1 ipv6" and "1 ipv4"
    // The service's next hops are part of ep0. Flows arent needed for these.
    // => Total pod<-->svc flows: 4*1*2 + 2*1*1 = 10 flows

    // We have 2 NHs which are local EP IPs. So 4 flows get
    // created for any <--> svc-tgt. 2 flows get created for nodeport stats.

    // Also there are 2 default entries but only 1 has send_flow_rem
    // So totally we have 17 flows in STATS table for which we collect stats
    if (serviceStatsManager.statsState.newFlowCounterMap.size() == 17)
        return true;
    return false;
}

void ServiceStatsManagerFixture::checkNewFlowMapInitialized (void)
{
    std::unique_lock<std::mutex> guard(serviceStatsManager.pstatMtx);
    LOG(DEBUG) << "####### START - checkNewFlowMapInitialized"
               << " new: " << serviceStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << serviceStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << serviceStatsManager.statsState.removedFlowCounterMap.size();
    guard.unlock();

    WAIT_FOR_DO_ONFAIL(checkNewFlowMapSize(),
                       500,,
                       LOG(ERROR) << "##### flow state not fully setup ####";);

    guard.lock();
    LOG(DEBUG) << "####### END - checkNewFlowMapInitialized"
               << " new: " << serviceStatsManager.statsState.newFlowCounterMap.size()
               << " old: " << serviceStatsManager.statsState.oldFlowCounterMap.size()
               << " rem: " << serviceStatsManager.statsState.removedFlowCounterMap.size();
    guard.unlock();
}

BOOST_AUTO_TEST_SUITE(ServiceStatsManager_test)



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
BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsEpSvcNoAgg, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

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
    serviceStatsManager.Handle(NULL, OFPTYPE_FLOW_STATS_REPLY, NULL);
    serviceStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_STATS_REPLY, NULL);

    LOG(DEBUG) << "############# NOAGG START ############";
    testFlowStatsPodSvc(integrationPortConn, &serviceStatsManager);
    LOG(DEBUG) << "############# NOAGG END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsEpSvcAgg, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();
    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# EP+SVC AGG START ############";
    testFlowStatsPodSvc(integrationPortConn, &serviceStatsManager, true);
    LOG(DEBUG) << "############# EP+SVC AGG END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsNodeSvcTgt, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();
    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# node <--> SVC-TGT Flow stat match START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "10.20.44.2", true);
    LOG(DEBUG) << "############# node <--> SVC-TGT Flow stat match END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsAnySvcTgtV4, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();
    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# any <--> SVC-TGT Flow stat match v4 START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "10.20.44.2");
    LOG(DEBUG) << "############# any <--> SVC-TGT Flow stat match v4 END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowMatchStatsAnySvcTgtV6, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();
    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# any <--> SVC-TGT Flow stat match v6 START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "2001:db8::2");
    LOG(DEBUG) << "############# any <--> SVC-TGT Flow stat match v6 END ############";
    serviceStatsManager.stop();
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
BOOST_FIXTURE_TEST_CASE(testFlowAgePodSvcOld, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# AGE OLD pod-svc START ############";
    testFlowStatsPodSvc(integrationPortConn, &serviceStatsManager);
    testFlowAge(&serviceStatsManager, true, false);
    testFlowAge(&serviceStatsManager, true, true);
    LOG(DEBUG) << "############# AGE OLD pod-svc END ############";
    serviceStatsManager.stop();
}

// Verify aging of svc-tgt flows
BOOST_FIXTURE_TEST_CASE(testFlowAgeAnySvcTgtOld, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# AGE OLD any <--> svc-tgt START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "10.20.44.2");
    testFlowAge(&serviceStatsManager, true, false);
    testFlowAge(&serviceStatsManager, true, true);
    LOG(DEBUG) << "############# AGE OLD any <--> svc-tgt END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowAgeNodeSvcTgtOld, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# AGE OLD node <--> svc-tgt START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "10.20.44.2", true);
    testFlowAge(&serviceStatsManager, true, false);
    testFlowAge(&serviceStatsManager, true, true);
    LOG(DEBUG) << "############# AGE OLD node <--> svc-tgt END ############";
    serviceStatsManager.stop();
}

/*
 * Verify aging:
 * - run on_timer to create flows in newMap
 * - MAX_AGE=9, keep calling on_timer 9 times
 * - verify aging by checking statsMap.new.size
 * - check new map size. All entries must have aged.
 * - call on_timer again and check that all entries have been added to newMap.
 */
BOOST_FIXTURE_TEST_CASE(testFlowAgeNew, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();
    LOG(DEBUG) << "############# AGE NEW START ############";
    testFlowAge(&serviceStatsManager, false, false);
    testFlowAge(&serviceStatsManager, false, true);
    LOG(DEBUG) << "############# AGE NEW END ############";
    serviceStatsManager.stop();
}

/*
 * flows removed - newMap to RemovedMap:
 * - run on_timer to add flows to newMap     diffs=0, last=0
 * - handle flow removed with 100 pkts (new to remMap)  diffs=100, last = 0
 * - run on_timer to aggregate stats         entry removed from remMap
 * - check counters (obj:100)
 */
BOOST_FIXTURE_TEST_CASE(testFlowRemovedPodSvcNew, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();

    // Check if the below negative cases are fine
    serviceStatsManager.Handle(NULL, OFPTYPE_FLOW_REMOVED, NULL);
    serviceStatsManager.Handle(&integrationPortConn,
                                OFPTYPE_FLOW_REMOVED, NULL);

    LOG(DEBUG) << "############# REMOVEDNEW pod-svc START ############";
    testFlowRemovedPodSvc(integrationPortConn, &serviceStatsManager, 0);
    LOG(DEBUG) << "############# REMOVEDNEW pod-svc END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemovedAnySvcTgtNew, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();

    LOG(DEBUG) << "############# REMOVEDNEW any <--> svc-tgt START ############";
    testFlowRemovedSvcTgt(integrationPortConn, &serviceStatsManager, 0, "10.20.44.2");
    LOG(DEBUG) << "############# REMOVEDNEW any <--> svc-tgt END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemovedNodeSvcTgtNew, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();

    LOG(DEBUG) << "############# REMOVEDNEW node <--> svc-tgt START ############";
    testFlowRemovedSvcTgt(integrationPortConn, &serviceStatsManager, 0, "10.20.44.2", true);
    LOG(DEBUG) << "############# REMOVEDNEW node <--> svc-tgt END ############";
    serviceStatsManager.stop();
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
BOOST_FIXTURE_TEST_CASE(testFlowRemovedPodSvcOld, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();

    LOG(DEBUG) << "############# REMOVEDOLD pod-svc START ############";
    testFlowStatsPodSvc(integrationPortConn, &serviceStatsManager);
    testFlowRemovedPodSvc(integrationPortConn, &serviceStatsManager,
                    INITIAL_PACKET_COUNT);
    LOG(DEBUG) << "############# REMOVEDOLD pod-svc END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemovedAnySvcTgtOld, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();

    LOG(DEBUG) << "############# REMOVEDOLD any <--> svc-tgt START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "10.20.44.2");
    testFlowRemovedSvcTgt(integrationPortConn, &serviceStatsManager,
                          INITIAL_PACKET_COUNT, "10.20.44.2");
    LOG(DEBUG) << "############# REMOVEDOLD any <--> svc-tgt END ############";
    serviceStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testFlowRemovedNodeSvcTgtOld, ServiceStatsManagerFixture) {
    MockConnection integrationPortConn(TEST_CONN_TYPE_INT);
    serviceStatsManager.registerConnection(&integrationPortConn);
    serviceStatsManager.start();

    checkNewFlowMapInitialized();

    LOG(DEBUG) << "############# REMOVEDOLD node <--> svc-tgt START ############";
    testFlowStatsSvcTgt(integrationPortConn, &serviceStatsManager, "10.20.44.2", true);
    testFlowRemovedSvcTgt(integrationPortConn, &serviceStatsManager,
                          INITIAL_PACKET_COUNT, "10.20.44.2", true);
    LOG(DEBUG) << "############# REMOVEDOLD node <--> svc-tgt END ############";
    serviceStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
