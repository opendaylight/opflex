/*
 * Test suite for class FlowManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>
#include "logging.h"
#include "ovs.h"
#include "FlowManager.h"
#include "FlowExecutor.h"

#include "ModbFixture.h"
#include "TableState.h"

using namespace std;
using namespace boost::assign;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace ovsagent;

typedef pair<FlowEdit::TYPE, string> MOD;

static string CanonicalizeGroupEntryStr(const string& entryStr);

class MockFlowExecutor : public FlowExecutor {
public:
    MockFlowExecutor(): ignoreFlowMods(false) {}
    ~MockFlowExecutor() {}

    bool Execute(const FlowEdit& flowEdits) {
        if (ignoreFlowMods) {
            return true;
        }
        const char *modStr[] = {"ADD", "MOD", "DEL"};
        struct ds strBuf;
        ds_init(&strBuf);

        BOOST_FOREACH(const FlowEdit::Entry& ed, flowEdits.edits) {
            ofp_print_flow_stats(&strBuf, ed.second->entry);
            string str = (const char*)(ds_cstr(&strBuf)+1); // trim space
            const char *mod = modStr[ed.first];

            LOG(DEBUG) << "*** FlowMod " << ed;
            BOOST_CHECK_MESSAGE(!mods.empty(), "\nexp:\ngot: " << ed);
            if (!mods.empty()) {
                MOD exp = mods.front();
                mods.pop_front();
                BOOST_CHECK_MESSAGE(exp.first == ed.first,
                        "\nexp: " << modStr[exp.first] <<
                        "\ngot: " << ed);
                BOOST_CHECK_MESSAGE(exp.second == str,
                        "\nexp: " << modStr[exp.first] << "|" << exp.second <<
                        "\ngot: " << ed);
            }
            ds_clear(&strBuf);
        }
        ds_destroy(&strBuf);
        return true;
    }
    bool Execute(const GroupEdit& groupEdits) {
        BOOST_FOREACH(const GroupEdit::Entry& ed, groupEdits.edits) {
            LOG(DEBUG) << "*** GroupMod " << ed;
            stringstream ss;
            ss << ed;
            string edStr = CanonicalizeGroupEntryStr(ss.str());

            BOOST_CHECK_MESSAGE(!groupMods.empty(), "\nexp:\ngot: " << edStr);
            if (!groupMods.empty()) {
                string exp = groupMods.front();
                groupMods.pop_front();
                BOOST_CHECK_MESSAGE(exp == edStr,
                        "\nexp: " << exp << "\ngot: " << edStr);
            }
        }
        return true;
    }
    void Expect(FlowEdit::TYPE mod, const string& fe) {
        ignoreFlowMods = false;
        mods.push_back(MOD(mod, fe));
    }
    void Expect(FlowEdit::TYPE mod, const vector<string>& fe) {
        ignoreFlowMods = false;
        BOOST_FOREACH(const string& s, fe) {
            mods.push_back(MOD(mod, s));
        }
    }
    void ExpectGroup(FlowEdit::TYPE mod, const string& ge) {
        const char *modStr[] = {"ADD", "MOD", "DEL"};
        groupMods.push_back(CanonicalizeGroupEntryStr(
                string(modStr[mod]) + "|" + ge));
    }
    void IgnoreFlowMods() {
        ignoreFlowMods = true;
        mods.clear();
    }
    bool IsEmpty() { return mods.empty(); }
    bool IsGroupEmpty() { return groupMods.empty(); }

    std::list<MOD> mods;
    std::list<string> groupMods;
    bool ignoreFlowMods;
};

class MockPortMapper : public PortMapper {
public:
    uint32_t FindPort(const std::string& name) {
        return ports.find(name) != ports.end() ? ports[name] : OFPP_NONE;
    }
    boost::unordered_map<string, uint32_t> ports;
};

class MockSwitchConnection : public SwitchConnection {
public:
    MockSwitchConnection() : SwitchConnection("mockBridge") {
    }
    virtual ~MockSwitchConnection() {
        clear();
    }

    void clear() {
        BOOST_FOREACH(ofpbuf* msg, sentMsgs) {
            ofpbuf_delete(msg);
        }
        sentMsgs.clear();
    }

    ofp_version GetProtocolVersion() { return OFP13_VERSION; }

    int SendMessage(ofpbuf *msg) {
        sentMsgs.push_back(msg);
        return 0;
    }

    std::vector<ofpbuf*> sentMsgs;
};

class FlowManagerFixture : public ModbFixture {
public:
    FlowManagerFixture() : ModbFixture(),
                        flowManager(agent),
                        policyMgr(agent.getPolicyManager()),
                           ep2_port(11), ep4_port(22) {
        string tunIf("br0_vxlan0");
        flowManager.SetExecutor(&exec);
        flowManager.SetPortMapper(&portmapper);
        flowManager.SetEncapIface(tunIf);
        flowManager.SetTunnelRemoteIp("10.11.12.13");

        portmapper.ports[ep0->getInterfaceName().get()] = 80;
        portmapper.ports[tunIf] = 2048;

        WAIT_FOR(policyMgr.groupExists(epg0->getURI()), 500);
        WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg1->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg1->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg3->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg3->getURI()) != boost::none, 500);

        PolicyManager::uri_set_t egs;
        WAIT_FOR_DO(egs.size() == 2, 1000,
            egs.clear(); policyMgr.getContractProviders(con1->getURI(), egs));
        egs.clear();
        WAIT_FOR_DO(egs.size() == 2, 500,
            egs.clear(); policyMgr.getContractConsumers(con1->getURI(), egs));

        createEntriesForObjects();
        /* flowManager is not Start()-ed here to control the updates it gets */
    }
    ~FlowManagerFixture() {
    }
    void createEntriesForObjects();

    MockFlowExecutor exec;
    FlowManager flowManager;
    MockPortMapper portmapper;
    PolicyManager& policyMgr;

    vector<string> fe_fixed;
    vector<string> fe_epg0, fe_epg0_fd0;
    vector<string> fe_ep0, fe_ep0_fd0_1, fe_ep0_fd0_2, fe_ep0_eg1;
    vector<string> fe_ep0_eg0_1, fe_ep0_eg0_2;
    vector<string> fe_ep0_port_1, fe_ep0_port_2, fe_ep0_port_3;
    vector<string> fe_ep2, fe_ep2_eg1;
    vector<string> fe_con1, fe_con2;
    string ge_fd0, ge_bkt_ep0, ge_bkt_ep2, ge_bkt_tun;
    string ge_fd0_prom;
    string ge_fd1, ge_bkt_ep4;
    string ge_fd1_prom;
    uint32_t ep2_port;
    uint32_t ep4_port;
};

BOOST_AUTO_TEST_SUITE(FlowManager_test)

BOOST_FIXTURE_TEST_CASE(epg, FlowManagerFixture) {
    /* create */
    exec.Expect(FlowEdit::add, fe_fixed);
    exec.Expect(FlowEdit::add, fe_epg0);
    exec.Expect(FlowEdit::add, fe_ep0);
    exec.Expect(FlowEdit::add, fe_ep2);
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec.IsEmpty());

    /* no change */
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec.IsEmpty());

    /* forwarding object change */
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    exec.Expect(FlowEdit::mod, fe_epg0_fd0);
    exec.Expect(FlowEdit::mod, fe_ep0_fd0_1);
    exec.Expect(FlowEdit::add, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::add, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd0_prom + ge_bkt_tun);
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec.IsEmpty());
    BOOST_CHECK(exec.IsGroupEmpty());

    /* remove */
    Mutator m2(framework, policyOwner);
    epg0->remove();
    m2.commit();
    WAIT_FOR(policyMgr.groupExists(epg0->getURI()) == false, 500);
    exec.Expect(FlowEdit::del, fe_epg0_fd0);
    exec.Expect(FlowEdit::del, fe_epg0[1]);
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec.IsEmpty());
}

BOOST_FIXTURE_TEST_CASE(localEp, FlowManagerFixture) {
    /* created */
    exec.Expect(FlowEdit::add, fe_ep0);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsEmpty());

    /* endpoint group change */
    ep0->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep0);
    exec.Expect(FlowEdit::mod, fe_ep0_eg1);
    exec.Expect(FlowEdit::del, fe_ep0[6]);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsEmpty());

    /* endpoint group changes back to old one */
    ep0->setEgURI(epg0->getURI());
    epSrc.updateEndpoint(*ep0);
    exec.Expect(FlowEdit::mod, fe_ep0_eg0_1);
    exec.Expect(FlowEdit::add, fe_ep0[6]);
    exec.Expect(FlowEdit::mod, fe_ep0_eg0_2);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsEmpty());

    /* port-mapping change */
    portmapper.ports[ep0->getInterfaceName().get()] = 180;
    exec.Expect(FlowEdit::add, fe_ep0_port_1);
    for (int i = 0; i < fe_ep0_port_1.size(); ++i)  {
        exec.Expect(FlowEdit::del, fe_ep0[i]);
    }
    exec.Expect(FlowEdit::add, fe_ep0_port_2);
    for (int i = fe_ep0_port_1.size();
         i < fe_ep0_port_1.size() + fe_ep0_port_2.size();
         ++i) {
        exec.Expect(FlowEdit::del, fe_ep0[i]);
    }
    exec.Expect(FlowEdit::mod, fe_ep0_port_3);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsEmpty());

    /* remove endpoint */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.Expect(FlowEdit::del, fe_ep0_port_1);
    exec.Expect(FlowEdit::del, fe_ep0_port_2);
    exec.Expect(FlowEdit::del, fe_ep0_port_3);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsEmpty());
}

BOOST_FIXTURE_TEST_CASE(remoteEp, FlowManagerFixture) {
    /* created */
    exec.Expect(FlowEdit::add, fe_ep2);
    flowManager.endpointUpdated(ep2->getUUID());
    BOOST_CHECK(exec.IsEmpty());

    /* endpoint group change */
    ep2->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep2);
    exec.Expect(FlowEdit::mod, fe_ep2_eg1);
    exec.Expect(FlowEdit::del, fe_ep2[0]);
    flowManager.endpointUpdated(ep2->getUUID());
    BOOST_CHECK(exec.IsEmpty());
}

BOOST_FIXTURE_TEST_CASE(fd, FlowManagerFixture) {
    exec.IgnoreFlowMods();
    portmapper.ports[ep2->getInterfaceName().get()] = ep2_port;
    portmapper.ports[ep4->getInterfaceName().get()] = ep4_port;
    flowManager.endpointUpdated(ep0->getUUID());
    flowManager.endpointUpdated(ep2->getUUID());

    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    exec.Expect(FlowEdit::mod, fe_ep0_fd0_1);
    exec.Expect(FlowEdit::add, fe_ep0_fd0_2);
    exec.ExpectGroup(FlowEdit::add, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsEmpty());
    BOOST_CHECK(exec.IsGroupEmpty());

    exec.IgnoreFlowMods();
    exec.ExpectGroup(FlowEdit::mod, ge_fd0 + ge_bkt_ep0 + ge_bkt_ep2
            + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::mod, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    BOOST_CHECK(exec.IsGroupEmpty());

    /* remove port-mapping for ep2 */
    portmapper.ports.erase(ep2->getInterfaceName().get());
    exec.ExpectGroup(FlowEdit::mod, ge_fd0 + ge_bkt_ep0 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::mod, ge_fd0_prom + ge_bkt_tun);
    flowManager.endpointUpdated(ep2->getUUID());
    BOOST_CHECK(exec.IsGroupEmpty());

    /* remove ep0 */
    epSrc.removeEndpoint(ep0->getUUID());
    exec.ExpectGroup(FlowEdit::del, ge_fd0);
    exec.ExpectGroup(FlowEdit::del, ge_fd0_prom);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec.IsGroupEmpty());

    /* check promiscous flood */
    WAIT_FOR(policyMgr.getFDForGroup(epg2->getURI()) != boost::none, 500);
    exec.ExpectGroup(FlowEdit::add, ge_fd1 + ge_bkt_ep4 + ge_bkt_tun);
    exec.ExpectGroup(FlowEdit::add, ge_fd1_prom + ge_bkt_ep4 + ge_bkt_tun);
    flowManager.endpointUpdated(ep4->getUUID());
    BOOST_CHECK(exec.IsGroupEmpty());
}

BOOST_FIXTURE_TEST_CASE(policy, FlowManagerFixture) {
    exec.Expect(FlowEdit::add, fe_con2);
    flowManager.contractUpdated(con2->getURI());
    BOOST_CHECK(exec.IsEmpty());

    exec.Expect(FlowEdit::add, fe_con1);
    flowManager.contractUpdated(con1->getURI());
    BOOST_CHECK(exec.IsEmpty());

    /* remove */
    Mutator m2(framework, policyOwner);
    con2->remove();
    m2.commit();
    WAIT_FOR(policyMgr.contractExists(con2->getURI()) == false, 500);
    exec.Expect(FlowEdit::del, fe_con2);
    flowManager.contractUpdated(con2->getURI());
    BOOST_CHECK(exec.IsEmpty());
}

BOOST_FIXTURE_TEST_CASE(learn, FlowManagerFixture) {
    MockSwitchConnection conn;
    char packet_buf[512];
    ofputil_packet_in pin1, pin2;
    memset(packet_buf, 0xdeadbeef, sizeof(packet_buf));
    memset(&pin1, 0, sizeof(pin1));
    memset(&pin2, 0, sizeof(pin2));
    ofputil_protocol proto = 
        ofputil_protocol_from_ofp_version(conn.GetProtocolVersion());

    // initialize just the first part of the ethernet header
    char mac1[6] = {0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    char mac2[6] = {0xf, 0xe, 0xd, 0xc, 0xb, 0xa};
    memcpy(packet_buf, mac1, sizeof(mac1));
    memcpy(packet_buf + sizeof(mac1), mac2, sizeof(mac2));

    // stage 1 
    pin1.reason = OFPR_ACTION;
    pin1.packet = &packet_buf;
    pin1.packet_len = sizeof(packet_buf);
    pin1.total_len = sizeof(packet_buf);
    pin1.buffer_id = UINT32_MAX;
    pin1.table_id = 3;
    pin1.fmd.in_port = 42;
    pin1.fmd.regs[0] = 5;
    pin1.fmd.regs[5] = 10;
    
    ofpbuf* b = ofputil_encode_packet_in(&pin1,
                                          OFPUTIL_P_OF10_NXM,
                                          NXPIF_NXM);
    flowManager.Handle(&conn, OFPTYPE_PACKET_IN, b);
    ofpbuf_delete(b);

    BOOST_CHECK(conn.sentMsgs.size() == 3);
    uint64_t ofpacts_stub1[1024 / 8];
    uint64_t ofpacts_stub2[1024 / 8];
    uint64_t ofpacts_stub3[1024 / 8];
    struct ofpbuf ofpacts1, ofpacts2, ofpacts3;
    struct ofputil_flow_mod fm1, fm2;
    struct ofputil_packet_out po;

    ofpbuf_use_stub(&ofpacts1, ofpacts_stub1, sizeof ofpacts_stub1);
    ofpbuf_use_stub(&ofpacts2, ofpacts_stub2, sizeof ofpacts_stub2);
    ofpbuf_use_stub(&ofpacts3, ofpacts_stub3, sizeof ofpacts_stub3);
    ofputil_decode_flow_mod(&fm1, (ofp_header*)ofpbuf_data(conn.sentMsgs[0]),
                            proto, &ofpacts1, u16_to_ofp(64), 8);
    ofputil_decode_flow_mod(&fm2, (ofp_header*)ofpbuf_data(conn.sentMsgs[1]),
                            proto, &ofpacts2, u16_to_ofp(64), 8);
    ofputil_decode_packet_out(&po, (ofp_header*)ofpbuf_data(conn.sentMsgs[2]),
                              &ofpacts3);

    BOOST_CHECK(0 == memcmp(fm1.match.flow.dl_dst, mac2, sizeof(mac2)));
    BOOST_CHECK_EQUAL(10, fm1.match.flow.regs[5]);
    struct ofpact* a;
    int i;
    i = 0;
    OFPACT_FOR_EACH (a, fm1.ofpacts, fm1.ofpacts_len) {
        if (i == 0) BOOST_CHECK_EQUAL(OFPACT_SET_FIELD, a->type);
        if (i == 1) BOOST_CHECK_EQUAL(OFPACT_SET_FIELD, a->type);
        if (i == 2) BOOST_CHECK_EQUAL(OFPACT_OUTPUT, a->type);
        if (i == 3) BOOST_CHECK_EQUAL(OFPACT_CONTROLLER, a->type);
        ++i;
    }
    BOOST_CHECK_EQUAL(4, i);

    BOOST_CHECK(0 == memcmp(fm2.match.flow.dl_src, mac2, sizeof(mac2)));
    BOOST_CHECK_EQUAL(42, ofp_to_u16(fm2.match.flow.in_port.ofp_port));
    i = 0;
    OFPACT_FOR_EACH (a, fm2.ofpacts, fm2.ofpacts_len) {
        if (i == 0) BOOST_CHECK_EQUAL(OFPACT_GROUP, a->type);
        ++i;
    }
    BOOST_CHECK_EQUAL(1, i);

    BOOST_CHECK_EQUAL(sizeof(packet_buf), po.packet_len);
    BOOST_CHECK(0 == memcmp(po.packet, packet_buf, sizeof(packet_buf)));
    i = 0;
    OFPACT_FOR_EACH (a, po.ofpacts, po.ofpacts_len) {
        if (i == 0) BOOST_CHECK_EQUAL(OFPACT_GROUP, a->type);
        ++i;
    }
    BOOST_CHECK_EQUAL(1, i);
    
    conn.clear();

    // stage2
    pin1.reason = OFPR_ACTION;
    pin1.packet = &packet_buf;
    pin1.packet_len = 512;
    pin1.total_len = 512;
    pin1.buffer_id = UINT32_MAX;
    pin1.table_id = 3;
    pin1.fmd.in_port = 24;
    pin1.fmd.regs[0] = 5;
    pin1.fmd.regs[5] = 10;
    pin1.fmd.regs[7] = 42;

    b = ofputil_encode_packet_in(&pin1,
                                 OFPUTIL_P_OF10_NXM,
                                 NXPIF_NXM);
    flowManager.Handle(&conn, OFPTYPE_PACKET_IN, b);
    ofpbuf_delete(b);

    BOOST_CHECK(conn.sentMsgs.size() == 2);
    ofputil_decode_flow_mod(&fm1, (ofp_header *)ofpbuf_data(conn.sentMsgs[0]),
                            proto, &ofpacts1, u16_to_ofp(64), 8);
    ofputil_decode_flow_mod(&fm2, (ofp_header *)ofpbuf_data(conn.sentMsgs[1]),
                            proto, &ofpacts2, u16_to_ofp(64), 8);
    BOOST_CHECK(0 == memcmp(fm1.match.flow.dl_dst, mac2, sizeof(mac2)));
    BOOST_CHECK_EQUAL(10, fm1.match.flow.regs[5]);

    BOOST_CHECK(0 == memcmp(fm2.match.flow.dl_dst, mac1, sizeof(mac1)));
    BOOST_CHECK(0 == memcmp(fm2.match.flow.dl_src, mac2, sizeof(mac2)));
    BOOST_CHECK_EQUAL(10, fm2.match.flow.regs[5]);
    i = 0;
    OFPACT_FOR_EACH (a, fm2.ofpacts, fm2.ofpacts_len) {
        if (i == 0) BOOST_CHECK_EQUAL(OFPACT_SET_FIELD, a->type);
        if (i == 1) BOOST_CHECK_EQUAL(OFPACT_SET_FIELD, a->type);
        if (i == 2) BOOST_CHECK_EQUAL(OFPACT_GOTO_TABLE, a->type);
        ++i;
    }
    BOOST_CHECK_EQUAL(3, i);
}

BOOST_AUTO_TEST_SUITE_END()

enum REG {
    SEPG, DEPG, BD, FD, RD, OUTPORT, TUNID, TUNDST
};
string rstr[] = {
    "NXM_NX_REG0[]", "NXM_NX_REG2[]", "NXM_NX_REG4[]", "NXM_NX_REG5[]",
    "NXM_NX_REG6[]", "NXM_NX_REG7[]", "NXM_NX_TUN_ID[0..31]",
    "NXM_NX_TUN_IPV4_DST[]"
};
string rstr1[] = { "reg0", "reg2", "reg4", "reg5", "reg6", "reg7", "", ""};

/**
 * Helper class to build string representation of a flow entry.
 */
class Bldr {
public:
    Bldr(const string& init="") : entry(init) {
        cntr = 0;
        if (entry.empty()) {
            entry = "cookie=0x0, duration=0s, table=0, n_packets=0, "
                    "n_bytes=0, idle_age=0";
        }
    }
    string done() { cntr = 0; return entry; }
    Bldr& table(uint8_t t) { rep(", table=", str(t)); return *this; }
    Bldr& priority(uint16_t p) { rep(", priority=", str(p)); return *this; }
    Bldr& cookie(uint64_t c) { rep("cookie=", str(c, hex)); return *this; }
    Bldr& tunId(uint32_t id) { rep(",tun_id=", str(id, true)); return *this; }
    Bldr& in(uint32_t p) { rep(",in_port=", str(p)); return *this; }
    Bldr& reg(REG r, uint32_t v) {
        rep("," + rstr1[r] + "=", str(v, true)); return *this;
    }
    Bldr& isEthSrc(string& s) { rep(",dl_src=", s); return *this; }
    Bldr& isEthDst(string& s) { rep(",dl_dst=", s); return *this; }
    Bldr& ip() { rep(",ip"); return *this; }
    Bldr& arp() { rep(",arp"); return *this; }
    Bldr& tcp() { rep(",tcp"); return *this; }
    Bldr& isArpOp(uint8_t op) { rep(",arp_op=", str(op)); return *this; }
    Bldr& isSpa(string& s) { rep(",arp_spa=", s); return *this; }
    Bldr& isTpa(string& s) { rep(",arp_tpa=", s); return *this; }
    Bldr& isIpSrc(string& s) { rep(",nw_src=", s); return *this; }
    Bldr& isIpDst(string& s) { rep(",nw_dst=", s); return *this; }
    Bldr& isTpDst(uint16_t p) { rep(",tp_dst=", str(p)); return *this; }
    Bldr& actions() { rep(" actions="); cntr = 1; return *this; }
    Bldr& drop() { rep("drop"); return *this; }
    Bldr& load(REG r, uint32_t v) {
        rep("load:", str(v, true), "->" + rstr[r]); return *this;
    }
    Bldr& move(REG s, REG d) {
        rep("move:"+rstr[s]+"->"+rstr[d]); return *this;
    }
    Bldr& ethSrc(string& s) { rep("mod_dl_src:", s); return *this; }
    Bldr& ethDst(string& s) { rep("mod_dl_dst:", s); return *this; }
    Bldr& go(uint8_t t) { rep("goto_table:", str(t)); return *this; }
    Bldr& out(REG r) { rep("output:" + rstr[r]); return *this; }
    Bldr& decTtl() { rep("dec_ttl"); return *this; }
    Bldr& group(uint32_t g) { rep("group:", str(g)); return *this; }
    Bldr& bktId(uint32_t b) { rep("bucket_id:", str(b)); return *this; }
    Bldr& bktActions() { rep(",actions="); cntr = 1; return *this; }
    Bldr& outPort(uint32_t p) { rep("output:" + str(p)); return *this; }

private:
    /**
     * Matches a field in the entry string using prefix and optional
     * suffix, and replaces the value-part with the given value. If
     * field is not found, appends it to the end.
     */
    void rep(const string& pfx, const string& val="", const string& sfx="") {
        string sep(cntr == 2 ? "," : "");
        if (cntr == 1) {
            cntr = 2;
        }
        int pos2 = 0;
        do {
            int pos1 = entry.find(pfx, pos2);
            if (pos1 == string::npos) {
                entry.append(sep).append(pfx).append(val).append(sfx);
                return;
            }
            pos2 = pos1+pfx.size();
            while (pos2 != string::npos && entry[pos2] != ',' &&
                   entry[pos2] != ' ' && entry[pos2] != '-') {
                ++pos2;
            }
            if (!sfx.empty()) {
                if (pos2 != string::npos &&
                    entry.compare(pos2, sfx.size(), sfx)) {
                    continue;
                }
            }
            string tmp = entry.substr(0, pos1) + sep + pfx + val +
                    entry.substr(pos2);
            entry = tmp;
            return;
        } while(true);
    }
    string str(int i, bool hex = false) {
        char buf[10];
        sprintf(buf, hex && i != 0 ? "0x%x" : "%d", i);
        return buf;
    }

    string entry;
    int cntr;
};

/**
 * Create flow-entries corresponding to the state of objects and
 * modifications performed in the tests.
 */
void
FlowManagerFixture::createEntriesForObjects() {
    uint32_t tunPort = flowManager.GetTunnelPort();
    uint32_t tunDst = flowManager.GetTunnelDstIpv4();
    uint8_t rmacArr[6];
    memcpy(rmacArr, flowManager.GetRouterMacAddr(), sizeof(rmacArr));
    string rmac = MAC(rmacArr).toString();
    string bmac("ff:ff:ff:ff:ff:ff");
    string mmac("01:00:00:00:00:00/01:00:00:00:00:00");

    fe_fixed.push_back(Bldr().table(0).priority(25).arp()
                       .actions().drop().done());
    fe_fixed.push_back(Bldr().table(0).priority(25).ip()
                       .actions().drop().done());
    fe_fixed.push_back(Bldr().table(0).priority(50)
                       .in(tunPort).actions().go(1).done());

    /* epg0 */
    uint32_t epg0_vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    uint32_t epg1_vnid = policyMgr.getVnidForGroup(epg1->getURI()).get();
    fe_epg0.push_back(Bldr().table(1).priority(150).tunId(epg0_vnid)
            .in(tunPort).actions().load(SEPG, epg0_vnid).load(BD, 1)
            .load(FD, 0).load(RD, 1).go(2).done());
    fe_epg0.push_back(Bldr().table(4).priority(100).reg(SEPG, epg0_vnid)
            .reg(DEPG, epg0_vnid).actions().out(OUTPORT).done());

    /* epg0 connected to fd0 */
    fe_epg0_fd0.push_back(Bldr(fe_epg0[0]).load(FD, 1).done());

    /* local EP ep0 */
    string ep0_mac = ep0->getMAC().get().toString();
    string ep0_ip0 = *(ep0->getIPs().begin());
    string ep0_ip1 = *next(ep0->getIPs().begin());
    uint32_t ep0_port = portmapper.FindPort(ep0->getInterfaceName().get());

    fe_ep0.push_back(Bldr().table(0).priority(20).in(ep0_port)
            .isEthSrc(ep0_mac).actions().go(1).done());
    fe_ep0.push_back(Bldr().table(0).priority(30).ip().in(ep0_port)
            .isEthSrc(ep0_mac).isIpSrc(ep0_ip0).actions().go(1).done());
    fe_ep0.push_back(Bldr().table(0).priority(40).arp().in(ep0_port)
            .isEthSrc(ep0_mac).isSpa(ep0_ip0).actions().go(1).done());
    fe_ep0.push_back(Bldr(fe_ep0[1]).isIpSrc(ep0_ip1).done());
    fe_ep0.push_back(Bldr(fe_ep0[2]).isSpa(ep0_ip1).done());
    fe_ep0.push_back(Bldr().table(1).priority(140).in(ep0_port)
            .isEthSrc(ep0_mac).actions().load(SEPG, epg0_vnid).load(BD, 1)
            .load(FD, 0).load(RD, 1).go(2).done());
    fe_ep0.push_back(Bldr().table(2).priority(10).reg(BD, 1)
            .isEthDst(ep0_mac).actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, ep0_port).go(4).done());
    fe_ep0.push_back(Bldr().table(2).priority(15).ip().reg(RD, 1)
            .isEthDst(rmac).isIpDst(ep0_ip0)
            .actions().load(DEPG, epg0_vnid).load(OUTPORT, ep0_port)
            .ethSrc(rmac).ethDst(ep0_mac).decTtl().go(4).done());
    fe_ep0.push_back(Bldr().table(2).priority(20).arp().reg(RD, 1)
            .isEthDst(bmac).isTpa(ep0_ip0).isArpOp(1)
            .actions().load(DEPG, epg0_vnid).load(OUTPORT, ep0_port)
            .ethDst(ep0_mac).go(4).done());
    fe_ep0.push_back(Bldr(fe_ep0[7]).isIpDst(ep0_ip1).done());
    fe_ep0.push_back(Bldr(fe_ep0[8]).isTpa(ep0_ip1).done());

    /* ep0 when eg0 connected to fd0 */
    fe_ep0_fd0_1.push_back(Bldr(fe_ep0[5]).load(FD, 1).done());
    fe_ep0_fd0_2.push_back(Bldr().table(2).priority(10).reg(FD, 1)
            .isEthDst(mmac).actions().group(1).done());

    /* ep0 when moved to new group epg1 */
    fe_ep0_eg1.push_back(Bldr(fe_ep0[5]).load(SEPG, epg1_vnid)
            .load(BD, 0).done());
    for (int i = 7; i < fe_ep0.size(); ++i) {
        fe_ep0_eg1.push_back(Bldr(fe_ep0[i]).load(DEPG, epg1_vnid).done());
    }

    /* ep0 when moved back to old group epg0 */
    fe_ep0_eg0_1.push_back(fe_ep0[5]);
    for (int i = 7; i < fe_ep0.size(); ++i) {
        fe_ep0_eg0_2.push_back(fe_ep0[i]);
    }

    /* ep0 on new port */
    uint32_t ep0_port_new = 180;
    int idx = 0;
    for (; idx < 5; ++idx) {
        fe_ep0_port_1.push_back(Bldr(fe_ep0[idx]).in(ep0_port_new).done());
    }
    fe_ep0_port_2.push_back(Bldr(fe_ep0[idx++]).in(ep0_port_new).done());
    for (; idx < fe_ep0.size(); ++idx) {
        fe_ep0_port_3.push_back(Bldr(fe_ep0[idx]).load(OUTPORT, ep0_port_new)
                .done());
    }

    /* Remote EP ep2 */
    string ep2_mac = ep2->getMAC().get().toString();
    string ep2_ip0 = *(ep2->getIPs().begin());
    fe_ep2.push_back(Bldr().table(2).priority(10).reg(BD, 1).isEthDst(ep2_mac)
            .actions().load(DEPG, epg0_vnid).load(OUTPORT, tunPort)
            .move(SEPG, TUNID).load(TUNDST, tunDst).go(4)
            .done());
    fe_ep2.push_back(Bldr().table(2).priority(15).ip().reg(RD, 1)
            .isEthDst(rmac).isIpDst(ep2_ip0).actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, tunPort).move(SEPG, TUNID).load(TUNDST, tunDst)
            .ethSrc(rmac).decTtl().go(4).done());
    fe_ep2.push_back(Bldr().table(2).priority(20).arp().reg(RD, 1)
            .isEthDst(bmac).isTpa(ep2_ip0).isArpOp(1)
            .actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, tunPort).move(SEPG, TUNID).load(TUNDST, tunDst)
            .ethDst(ep2_mac).go(4).done());

    /* ep2 connected to new group epg1 */
    fe_ep2_eg1.push_back(Bldr(fe_ep2[1]).load(DEPG, epg1_vnid).done());
    fe_ep2_eg1.push_back(Bldr(fe_ep2[2]).load(DEPG, epg1_vnid).done());

    /* Group entries */
    string bktInit = ",bucket=";
    ge_fd0 = "group_id=1,type=all";
    ge_bkt_ep0 = Bldr(bktInit).bktId(ep0_port).bktActions().outPort(ep0_port)
            .done();
    ge_bkt_ep2 = Bldr(bktInit).bktId(ep2_port).bktActions().outPort(ep2_port)
            .done();
    ge_bkt_tun = Bldr(bktInit).bktId(tunPort).bktActions().move(SEPG, TUNID)
            .load(TUNDST, tunDst).outPort(tunPort).done();
    ge_fd0_prom = "group_id=2147483649,type=all";

    ge_bkt_ep4 = Bldr(bktInit).bktId(ep4_port).bktActions().outPort(ep4_port)
            .done();
    ge_fd1 = "group_id=2,type=all";
    ge_fd1_prom = "group_id=2147483650,type=all";

    uint32_t epg2_vnid = policyMgr.getVnidForGroup(epg2->getURI()).get();
    uint32_t epg3_vnid = policyMgr.getVnidForGroup(epg3->getURI()).get();
    uint16_t prio = FlowManager::MAX_POLICY_RULE_PRIORITY;

    /* con2 */
    uint32_t con2_cookie = flowManager.GetId(con2->getClassId(),
            con2->getURI());
    fe_con2.push_back(Bldr().table(4).priority(prio).cookie(con2_cookie)
            .reg(SEPG, epg3_vnid).reg(DEPG, epg2_vnid).actions().out(OUTPORT)
            .done());
    fe_con2.push_back(Bldr(fe_con2[0]).reg(SEPG, epg2_vnid)
            .reg(DEPG, epg3_vnid).done());

    /* con1 */
    PolicyManager::uri_set_t ps, cs;
    policyMgr.getContractProviders(con1->getURI(), ps);
    policyMgr.getContractConsumers(con1->getURI(), cs);
    unordered_set<uint32_t> pvnids, cvnids;
    flowManager.GetGroupVnids(ps, pvnids);
    flowManager.GetGroupVnids(cs, cvnids);
    uint32_t con1_cookie = flowManager.GetId(con1->getClassId(),
            con1->getURI());
    BOOST_FOREACH(uint32_t pvnid, pvnids) {
        BOOST_FOREACH(uint32_t cvnid, cvnids) {
            fe_con1.push_back(Bldr().table(4).priority(prio)
                    .cookie(con1_cookie).tcp()
                    .reg(SEPG, cvnid).reg(DEPG, pvnid).isTpDst(80)
                    .actions().out(OUTPORT).done());
            fe_con1.push_back(Bldr().table(4).priority(prio-1)
                    .cookie(con1_cookie).arp()
                    .reg(SEPG, pvnid).reg(DEPG, cvnid)
                    .actions().out(OUTPORT).done());
        }
    }
}

string CanonicalizeGroupEntryStr(const string& entryStr) {
    size_t p = 0;
    vector<string> tokens;
    while (p != string::npos) {
        size_t np = entryStr.find(",bucket=", p > 0 ? p+1 : p);
        size_t len = np == string::npos ? string::npos : (np-p);
        tokens.push_back(entryStr.substr(p, len));
        p = np;
    }
    if (tokens.size() > 1) {
        sort(tokens.begin()+1, tokens.end());
    }
    string out;
    BOOST_FOREACH(const string& s, tokens) {
        out.append(s);
    }
    return out;
}
