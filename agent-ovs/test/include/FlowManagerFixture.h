/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_TEST_FLOWTEST_H_
#define OVSAGENT_TEST_FLOWTEST_H_

#include "ModbFixture.h"
#include "SwitchManager.h"
#include "MockSwitchManager.h"
#include "TableState.h"
#include "BaseStatsManager.h"
#include "CtZoneManager.h"
#include "logging.h"
#include "FlowBuilder.h"
#include "IntFlowManager.h"
#include "AccessFlowManager.h"
#include <boost/lexical_cast.hpp>

#include <boost/asio/io_service.hpp>

#include <vector>
#include <thread>
using boost::optional;

using std::shared_ptr;
using std::string;
using opflex::modb::URI;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;

typedef ovsagent::EndpointListener::uri_set_t uri_set_t;

namespace ovsagent {

enum {
    TEST_CONN_TYPE_INT=0,
    TEST_CONN_TYPE_ACC,
};

enum REG {
    SEPG, SEPG12, DEPG, BD, FD, RD, OUTPORT, TUNID, TUNSRC, TUNDST, VLAN,
    ETHSRC, ETHDST, ARPOP, ARPSHA, ARPTHA, ARPSPA, ARPTPA, METADATA,
    PKT_MARK
};

enum FLAG {
    SEND_FLOW_REM=1, NO_PKT_COUNTS, NO_BYT_COUNTS, CHECK_OVERLAP,
    RESET_COUNTS
};
static const uint32_t INITIAL_PACKET_COUNT = 100;
static const uint32_t FINAL_PACKET_COUNT = 299;
static const uint32_t PACKET_SIZE = 64;
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



class FlowManagerFixture : public ModbFixture {
public:
    FlowManagerFixture()
        : ctZoneManager(idGen), switchManager(agent, exec, reader, portmapper) {
        switchManager.setSyncDelayOnConnect(0);
        ctZoneManager.setCtZoneRange(1, 65534);
        ctZoneManager.init("conntrack");
    }
    virtual ~FlowManagerFixture() {
    }

    virtual void verifyFlowStats(shared_ptr<L24Classifier> classifier,
                         uint32_t packet_count,
                         uint32_t byte_count,uint32_t table_id,
                         BaseStatsManager *statsManager,
                         shared_ptr<EpGroup> srcEpg = NULL,
                         shared_ptr<EpGroup> dstEpg = NULL) {
      optional<shared_ptr<PolicyStatUniverse> > su =
          PolicyStatUniverse::resolve(agent.getFramework());
      if (srcEpg != NULL && dstEpg != NULL) {
          auto uuid =
             boost::lexical_cast<std::string>(statsManager->getAgentUUID());
          optional<shared_ptr<L24ClassifierCounter> > myCounter =
            su.get()-> resolveGbpeL24ClassifierCounter(uuid,
                                                   statsManager
                                                   ->getCurrClsfrGenId(),
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
      } else {
          auto uuid =
            boost::lexical_cast<std::string>(statsManager->getAgentUUID());
          optional<shared_ptr<HPPClassifierCounter> > myCounter =
          su.get()-> resolveGbpeHPPClassifierCounter(uuid,
                                                   statsManager
                                                   ->getCurrClsfrGenId(),
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
    }


    virtual void start() {
        using boost::asio::io_service;
        using std::thread;
        agent.getAgentIOService().reset();
        io_service& io = agent.getAgentIOService();
        ioWork.reset(new io_service::work(io));
        ioThread.reset(new thread([&io] { io.run(); }));
        switchManager.start("placeholder");
    }

    virtual void stop() {
        switchManager.stop();
        ioWork.reset();
        ioThread->join();
        ioThread.reset();
    }

    void setConnected() {
        switchManager.enableSync();
        switchManager.connect();
    }

    virtual void clearExpFlowTables() {
        for (FlowEntryList& fel : expTables)
            fel.clear();
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

void writeClassifierFlows(FlowEntryList& entryList,uint32_t table_id,
    uint32_t portNum,shared_ptr<L24Classifier>& classifier,
    shared_ptr<EpGroup> srcEpg = NULL,
    shared_ptr<EpGroup> dstEpg = NULL,
    PolicyManager *policyManager = NULL) {

    //FlowEntryList entryList;
    struct ofputil_flow_mod fm;
    enum ofputil_protocol prots;
    uint32_t secGrpSetId = idGen.getId("secGroupSet",
                                       getSecGrpSetId(ep0->getSecurityGroups()));
    uint32_t priority = PolicyManager::MAX_POLICY_RULE_PRIORITY;
    const string& classifierId = classifier->getURI().toString();
    uint32_t cookie =
        idGen.getId(IntFlowManager::getIdNamespace(L24Classifier::CLASS_ID),
                    classifier->getURI().toString());
    if (srcEpg != NULL && dstEpg != NULL)
    {
        WAIT_FOR(policyManager->getVnidForGroup(srcEpg->getURI()), 500);
        WAIT_FOR(policyManager->getVnidForGroup(dstEpg->getURI()), 500);

        uint32_t epg1_vnid = policyManager->getVnidForGroup(srcEpg->getURI()).get();
        uint32_t epg2_vnid = policyManager->getVnidForGroup(dstEpg->getURI()).get();
        FlowBuilder().cookie(cookie).inPort(portNum).table(table_id).priority(priority).reg(SEPG,epg1_vnid).reg(DEPG,epg2_vnid).build(entryList);
    } else {
        FlowBuilder().cookie(cookie).inPort(portNum).table(table_id).priority(priority).reg(SEPG,secGrpSetId).build(entryList);
    }
    FlowEntryList entryListCopy(entryList);
    switchManager.writeFlow(classifierId, table_id,
                                entryListCopy);
}

void testOneFlow(MockConnection& portConn,
    shared_ptr<L24Classifier>& classifier,uint32_t table_id,
    uint32_t portNum,BaseStatsManager *statsManager,
    shared_ptr<EpGroup> srcEpg = NULL,
    shared_ptr<EpGroup> dstEpg = NULL,
    PolicyManager *policyManager = NULL) {
    // add flows in switchManager
    FlowEntryList entryList;
    writeClassifierFlows(entryList,table_id,portNum,classifier,srcEpg,dstEpg,policyManager);

    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    statsManager->on_timer(ec);

    // create first flow stats reply message
    struct ofpbuf *res_msg = makeFlowStatReplyMessage_2(&portConn,
                                                        INITIAL_PACKET_COUNT,
                                                        table_id,
                                                        entryList);
    LOG(DEBUG) << "1 makeFlowStatReplyMessage successful";
    BOOST_REQUIRE(res_msg!=0);

    // send first flow stats reply message
    statsManager->Handle(&portConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handling successful";
    ofpbuf_delete(res_msg);

    // create second flow stats reply message
    res_msg = makeFlowStatReplyMessage_2(&portConn,
                                         FINAL_PACKET_COUNT,
                                         table_id,
                                         entryList);
    // send second flow stats reply message
    statsManager->Handle(&portConn,
                              OFPTYPE_FLOW_STATS_REPLY, res_msg);
    ofpbuf_delete(res_msg);
    // Call on_timer function to process the stats collected
    // and generate Genie objects for stats

    statsManager->on_timer(ec);

    // calculate expected packet count and byte count
    // that we should have in Genie object

    uint32_t num_flows = entryList.size();
    uint32_t exp_classifier_packet_count =
        (FINAL_PACKET_COUNT - INITIAL_PACKET_COUNT) * num_flows;

    // Verify per classifier/per epg pair packet and byte count

    verifyFlowStats(classifier,
                    exp_classifier_packet_count,
                    exp_classifier_packet_count * PACKET_SIZE,
                    table_id,statsManager,srcEpg,dstEpg);

    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";
}


    IdGenerator idGen;
    CtZoneManager ctZoneManager;
    MockFlowExecutor exec;
    MockFlowReader reader;
    MockPortMapper portmapper;
    MockSwitchManager switchManager;

    std::vector<FlowEntryList> expTables;

    std::unique_ptr<std::thread> ioThread;
    std::unique_ptr<boost::asio::io_service::work> ioWork;
};



void addExpFlowEntry(std::vector<FlowEntryList>& tables,
                     const std::string& flowMod);

void printAllDiffs(std::vector<FlowEntryList>& expected,
                   FlowEdit* diffs);

void doDiffTables(SwitchManager* switchManager,
                  std::vector<FlowEntryList>& expected,
                  FlowEdit* diffs,
                  volatile int* fail);

void diffTables(Agent& agent,
                SwitchManager& switchManager,
                std::vector<FlowEntryList>& expected,
                FlowEdit* diffs,
                volatile int* fail);

#define WAIT_FOR_TABLES(test, count)                                    \
    {                                                                   \
        FlowEdit _diffs[expTables.size()];                              \
        volatile int _fail = 512;                                       \
        WAIT_FOR_DO_ONFAIL(_fail == 0, count,                           \
                           diffTables(agent, switchManager, expTables,  \
                                      _diffs, &_fail),                  \
                           LOG(ERROR) << test ": Incorrect tables: "    \
                           << _fail;                                    \
                           printAllDiffs(expTables, _diffs));           \
    }


/**
 * Helper class to build string representation of a flow entry.
 */
class Bldr {
public:
    Bldr(const std::string& init="");
    Bldr(const std::string& init, uint32_t flag);

    std::string done() { cntr = 0; return entry; }
    Bldr& table(uint8_t t) { rep(", table=", str(t)); return *this; }
    Bldr& priority(uint16_t p) { rep(", priority=", str(p)); return *this; }
    Bldr& cookie(uint64_t c) { rep("cookie=", str64(c, true)); return *this; }
    Bldr& tunId(uint32_t id) { rep(",tun_id=", str(id, true)); return *this; }
    Bldr& in(uint32_t p) { rep(",in_port=", str(p)); return *this; }
    Bldr& reg(REG r, uint32_t v);
    Bldr& isEthSrc(const std::string& s) { rep(",dl_src=", s); return *this; }
    Bldr& isEthDst(const std::string& s) { rep(",dl_dst=", s); return *this; }
    Bldr& ip() { rep(",ip"); return *this; }
    Bldr& ipv6() { rep(",ipv6"); return *this; }
    Bldr& arp() { rep(",arp"); return *this; }
    Bldr& tcp() { rep(",tcp"); return *this; }
    Bldr& tcp6() { rep(",tcp6"); return *this; }
    Bldr& udp() { rep(",udp"); return *this; }
    Bldr& udp6() { rep(",udp6"); return *this; }
    Bldr& icmp() { rep(",icmp"); return *this; }
    Bldr& icmp6() { rep(",icmp6"); return *this; }
    Bldr& icmp_type(uint8_t t) { rep(",icmp_type=", str(t)); return *this; }
    Bldr& icmp_code(uint8_t c) { rep(",icmp_code=", str(c)); return *this; }
    Bldr& isArpOp(uint8_t op) { rep(",arp_op=", str(op)); return *this; }
    Bldr& isSpa(const std::string& s) { rep(",arp_spa=", s); return *this; }
    Bldr& isTpa(const std::string& s) {
        rep(",arp_tpa=", s); return *this;
    }
    Bldr& isIpSrc(const std::string& s) {
        rep(",nw_src=", s); return *this;
    }
    Bldr& isIpv6Src(const std::string& s) {
        rep(",ipv6_src=", s); return *this;
    }
    Bldr& isIpDst(const std::string& s) {
        rep(",nw_dst=", s); return *this;
    }
    Bldr& isIpv6Dst(const std::string& s) {
        rep(",ipv6_dst=", s); return *this;
    }
    Bldr& isTpSrc(uint16_t p) { rep(",tp_src=", str(p)); return *this; }
    Bldr& isTpDst(uint16_t p) { rep(",tp_dst=", str(p)); return *this; }
    Bldr& isTpSrc(uint16_t p, uint16_t m) {
        rep(",tp_src=", str(p, true) + "/" + str(m, true)); return *this;
    }
    Bldr& isTpDst(uint16_t p, uint16_t m) {
        rep(",tp_dst=", str(p, true) + "/" + str(m, true)); return *this;
    }
    Bldr& isVlan(uint16_t v) { rep(",dl_vlan=", str(v)); return *this; }
    Bldr& isNdTarget(const std::string& t) {
        rep(",nd_target=", t); return *this;
    }
    Bldr& isEth(uint16_t t)  { rep(",dl_type=", str(t, true)); return *this; }
    Bldr& isTcpFlags(const std::string& s)  {
        rep(",tcp_flags=", s); return *this;
    }
    Bldr& isCtState(const std::string& s) {
        rep(",ct_state=" + s); return *this;
    }
    Bldr& isCtMark(const std::string& s) {
        rep(",ct_mark=" + s); return *this;
    }
    Bldr& isMdAct(uint8_t a) {
        rep(",metadata=", str(a, true), "/0xff"); return *this;
    }
    Bldr& isPolicyApplied() { rep(",metadata=0x100/0x100"); return *this; }
    Bldr& isFromServiceIface(bool yes = true) {
        rep((std::string(",metadata=") +
             (yes ? "0x200" : "0") + "/0x200").c_str());
        return *this;
    }
    Bldr& isMd(const std::string& m) { rep(",metadata=", m); return *this; }
    Bldr& isPktMark(uint32_t m) {
        rep(",pkt_mark=", str(m, true)); return *this;
    }
    Bldr& actions() { rep(" actions="); cntr = 1; return *this; }
    Bldr& drop() { rep("drop"); return *this; }
    Bldr& load64(REG r, uint64_t v);
    Bldr& load(REG r, uint32_t v);
    Bldr& load(REG r, const std::string& v);
    Bldr& move(REG s, REG d);
    Bldr& ethSrc(const std::string& s) {
        rep("set_field:", s, "->eth_src"); return *this;
    }
    Bldr& ethDst(const std::string& s) {
        rep("set_field:", s, "->eth_dst"); return *this;
    }
    Bldr& ipSrc(const std::string& s) {
        rep("mod_nw_src:", s); return *this;
    }
    Bldr& ipDst(const std::string& s) {
        rep("mod_nw_dst:", s); return *this;
    }
    Bldr& tpSrc(uint16_t p) {
        rep("mod_tp_src:", str(p)); return *this;
    }
    Bldr& tpDst(uint16_t p) {
        rep("mod_tp_dst:", str(p)); return *this;
    }
    Bldr& ipv6Src(const std::string& s) {
        rep("set_field:", s, "->ipv6_src"); return *this;
    }
    Bldr& ipv6Dst(const std::string& s) {
        rep("set_field:", s, "->ipv6_dst"); return *this;
    }
    Bldr& go(uint8_t t) { rep("goto_table:", str(t)); return *this; }
    Bldr& out(REG r);
    Bldr& decTtl() { rep("dec_ttl"); return *this; }
    Bldr& group(uint32_t g) { rep("group:", str(g)); return *this; }
    Bldr& bktId(uint32_t b) { rep("bucket_id:", str(b)); return *this; }
    Bldr& bktActions() { rep(",actions="); cntr = 1; return *this; }
    Bldr& outPort(uint32_t p) { rep("output:", str(p)); return *this; }
    Bldr& pushVlan() { rep("push_vlan:0x8100"); return *this; }
    Bldr& popVlan() { rep("pop_vlan"); return *this; }
    Bldr& setVlan(uint16_t v) { rep("set_vlan_vid:", str(v)); return *this; }
    Bldr& inport() { rep("IN_PORT"); return *this; }
    Bldr& controller(uint16_t len) {
        rep("CONTROLLER:", str(len)); return *this;
    }
    Bldr& meta(uint64_t a, uint64_t m) {
        rep("write_metadata:", str(a, true),
            "/" + str(m, true)); return *this;
    }
    Bldr& mdAct(uint8_t a) {
        rep("write_metadata:", str(a, true), "/0xff"); return *this;
    }
    Bldr& ct(const std::string& s) { rep("ct(", s, ")"); return *this; }
    Bldr& multipath(const std::string& s) {
        rep("multipath(", s, ")"); return *this;
    }
    Bldr& polApplied() { rep("write_metadata:0x100/0x100"); return *this; }
    Bldr& resubmit(uint8_t t) { rep("resubmit(,", str(t), ")"); return *this; }

private:
    /**
     * Matches a field in the entry string using prefix and optional
     * suffix, and replaces the value-part with the given value. If
     * field is not found, appends it to the end.
     */
    void rep(const std::string& pfx, const std::string& val="",
             const std::string& sfx="");
    std::string str(int i, bool hex = false);
    std::string str64(uint64_t i, bool hex = false);
    std::string strpad(int i);

    std::string entry;
    int cntr;
};

} // namespace ovsagent

#endif /* OVSAGENT_TEST_FLOWTEST_H_ */
