/*
 * Test suite for class FlowManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>

#include <ovs.h>
#include <flowManager.h>
#include <flowExecutor.h>

#include <modbFixture.h>

using namespace std;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;
using namespace opflex::modb;
using namespace modelgbp::gbp;
using namespace ovsagent;

typedef pair<FlowEdit::TYPE, string> MOD;

class MockFlowExecutor : public FlowExecutor {
public:
    MockFlowExecutor() {}
    ~MockFlowExecutor() {}

    bool Execute(const FlowEdit& flowEdits) {
        const char *modStr[] = {"A", "M", "D"};
        struct ds strBuf;
        ds_init(&strBuf);

        BOOST_FOREACH(const FlowEdit::Entry& ed, flowEdits.edits) {
            ofp_print_flow_stats(&strBuf, ed.second->entry);
            string str = (const char*)(ds_cstr(&strBuf)+1); // trim space
            const char *mod = modStr[ed.first];

            DLOG(INFO) << "*** " << mod << "|" << str;
            BOOST_CHECK_MESSAGE(!mods.empty(), "\ngot " << mod << "|" << str);
            if (!mods.empty()) {
                MOD exp = mods.front();
                mods.pop_front();
                BOOST_CHECK_MESSAGE(exp.first == ed.first,
                        "\nexp: " << modStr[exp.first] <<
                        "\ngot: " << mod << "|" << str);
                BOOST_CHECK_MESSAGE(exp.second == str,
                        "\nexp: " << modStr[exp.first] << "|" << exp.second <<
                        "\ngot: " << mod << "|" << str);
            }
            ds_clear(&strBuf);
        }
        ds_destroy(&strBuf);
        return true;
    }
    void Expect(FlowEdit::TYPE mod, const string& fe) {
        mods.push_back(MOD(mod, fe));
    }
    void Expect(FlowEdit::TYPE mod, const vector<string>& fe) {
        BOOST_FOREACH(const string& s, fe) {
            mods.push_back(MOD(mod, s));
        }
    }
    bool IsEmpty() { return mods.empty(); }

    std::list<MOD> mods;
};

class MockPortMapper : public PortMapper {
public:
    uint32_t FindPort(const std::string& name) {
        return ports.find(name) != ports.end() ? ports[name] : OFPP_NONE;
    }
    boost::unordered_map<string, uint32_t> ports;
};

class FlowManagerFixture : public ModbFixture {
public:
    FlowManagerFixture() : ModbFixture(),
                        flowManager(agent),
                        policyMgr(agent.getPolicyManager()) {
        exec = new MockFlowExecutor();
        flowManager.SetExecutor(exec);
        flowManager.SetPortMapper(&portmapper);
        portmapper.ports[ep0->getInterfaceName().get()] = 80;

        WAIT_FOR(policyMgr.groupExists(epg0->getURI()), 500);
        WAIT_FOR(policyMgr.getBDForGroup(epg0->getURI()) != boost::none, 500);

        WAIT_FOR(policyMgr.groupExists(epg1->getURI()), 500);
        WAIT_FOR(policyMgr.getRDForGroup(epg1->getURI()) != boost::none, 500);

        createEntriesForObjects();
        /* flowManager is not Start()-ed here to control the updates it gets */
    }
    ~FlowManagerFixture() {
    }
    void createEntriesForObjects();

    MockFlowExecutor *exec;
    FlowManager flowManager;
    MockPortMapper portmapper;
    PolicyManager& policyMgr;

    vector<string> fe_epg0, fe_epg0_fd0;
    vector<string> fe_ep0, fe_ep0_fd0, fe_ep0_eg1;
    vector<string> fe_ep0_eg0_1, fe_ep0_eg0_2;
    vector<string> fe_ep0_port_1, fe_ep0_port_2, fe_ep0_port_3;
    vector<string> fe_ep2, fe_ep2_eg1;
};

BOOST_AUTO_TEST_SUITE(flowmanager_test)

BOOST_FIXTURE_TEST_CASE(epg, FlowManagerFixture) {
    /* create */
    exec->Expect(FlowEdit::add, fe_epg0);
    exec->Expect(FlowEdit::add, fe_ep0);
    exec->Expect(FlowEdit::add, fe_ep2);
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec->IsEmpty());

    /* no change */
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec->IsEmpty());

    /* forwarding object change */
    Mutator m1(framework, policyOwner);
    epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnet(subnetsfd0_1->getURI());
    m1.commit();
    WAIT_FOR(policyMgr.getFDForGroup(epg0->getURI()) != boost::none, 500);
    exec->Expect(FlowEdit::mod, fe_epg0_fd0);
    exec->Expect(FlowEdit::mod, fe_ep0_fd0);
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec->IsEmpty());

    /* remove */
    Mutator m2(framework, policyOwner);
    epg0->remove();
    m2.commit();
    WAIT_FOR(policyMgr.groupExists(epg0->getURI()) == false, 500);
    exec->Expect(FlowEdit::del, fe_epg0_fd0);
    exec->Expect(FlowEdit::del, fe_epg0[1]);
    flowManager.egDomainUpdated(epg0->getURI());
    BOOST_CHECK(exec->IsEmpty());
}

BOOST_FIXTURE_TEST_CASE(localEp, FlowManagerFixture) {
    /* created */
    exec->Expect(FlowEdit::add, fe_ep0);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec->IsEmpty());

    /* endpoint group change */
    ep0->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep0);
    exec->Expect(FlowEdit::mod, fe_ep0_eg1);
    exec->Expect(FlowEdit::del, fe_ep0[6]);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec->IsEmpty());

    /* endpoint group changes back to old one */
    ep0->setEgURI(epg0->getURI());
    epSrc.updateEndpoint(*ep0);
    exec->Expect(FlowEdit::mod, fe_ep0_eg0_1);
    exec->Expect(FlowEdit::add, fe_ep0[6]);
    exec->Expect(FlowEdit::mod, fe_ep0_eg0_2);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec->IsEmpty());

    /* port-mapping change */
    portmapper.ports[ep0->getInterfaceName().get()] = 180;
    exec->Expect(FlowEdit::add, fe_ep0_port_1);
    for (int i = 0; i < fe_ep0_port_1.size(); ++i)  {
        exec->Expect(FlowEdit::del, fe_ep0[i]);
    }
    exec->Expect(FlowEdit::add, fe_ep0_port_2);
    for (int i = fe_ep0_port_1.size();
         i < fe_ep0_port_1.size() + fe_ep0_port_2.size();
         ++i) {
        exec->Expect(FlowEdit::del, fe_ep0[i]);
    }
    exec->Expect(FlowEdit::mod, fe_ep0_port_3);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec->IsEmpty());

    /* remove endpoint */
    epSrc.removeEndpoint(ep0->getUUID());
    exec->Expect(FlowEdit::del, fe_ep0_port_1);
    exec->Expect(FlowEdit::del, fe_ep0_port_2);
    exec->Expect(FlowEdit::del, fe_ep0_port_3);
    flowManager.endpointUpdated(ep0->getUUID());
    BOOST_CHECK(exec->IsEmpty());
}

BOOST_FIXTURE_TEST_CASE(remoteEp, FlowManagerFixture) {
    /* created */
    exec->Expect(FlowEdit::add, fe_ep2);
    flowManager.endpointUpdated(ep2->getUUID());
    BOOST_CHECK(exec->IsEmpty());

    /* endpoint group change */
    ep2->setEgURI(epg1->getURI());
    epSrc.updateEndpoint(*ep2);
    exec->Expect(FlowEdit::mod, fe_ep2_eg1);
    exec->Expect(FlowEdit::del, fe_ep2[0]);
    flowManager.endpointUpdated(ep2->getUUID());
    BOOST_CHECK(exec->IsEmpty());
}

BOOST_AUTO_TEST_SUITE_END()

enum REG {
    SEPG, DEPG, BD, FD, RD, OUTPORT, TUNID, TUNDST
};
string rstr[] = {
    "NXM_NX_REG0[]", "NXM_NX_REG2[]", "NXM_NX_REG4[]", "NXM_NX_REG5[]",
    "NXM_NX_REG6[]", "NXM_NX_REG7[]", "NXM_NX_TUN_ID[]", "NXM_NX_TUN_IPV4_DST[]"
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
    Bldr& priority(uint8_t p) { rep(", priority=", str(p)); return *this; }
    Bldr& tunId(uint32_t id) { rep(",tun_id=", str(id, true)); return *this; }
    Bldr& in(uint32_t p) { rep(",in_port=", str(p)); return *this; }
    Bldr& reg(REG r, uint32_t v) {
        rep("," + rstr1[r] + "=", str(v, true)); return *this;
    }
    Bldr& isEthSrc(string& s) { rep(",dl_src=", s); return *this; }
    Bldr& isEthDst(string& s) { rep(",dl_dst=", s); return *this; }
    Bldr& ip() { rep(",ip"); return *this; }
    Bldr& arp() { rep(",arp"); return *this; }
    Bldr& isArpOp(uint8_t op) { rep(",arp_op=", str(op)); return *this; }
    Bldr& isSpa(string& s) { rep(",arp_spa=", s); return *this; }
    Bldr& isTpa(string& s) { rep(",arp_tpa=", s); return *this; }
    Bldr& isIpSrc(string& s) { rep(",nw_src=", s); return *this; }
    Bldr& isIpDst(string& s) { rep(",nw_dst=", s); return *this; }
    Bldr& actions() { rep(" actions="); cntr = 1; return *this; }
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

    /* epg0 */
    uint32_t epg0_vnid = policyMgr.getVnidForGroup(epg0->getURI()).get();
    uint32_t epg1_vnid = policyMgr.getVnidForGroup(epg1->getURI()).get();
    fe_epg0.push_back(Bldr().table(1).priority(150).tunId(epg0_vnid)
            .in(tunPort).actions().load(SEPG, epg0_vnid).load(BD, 1)
            .load(FD, 0).load(RD, 1).go(2).done());
    fe_epg0.push_back(Bldr().table(3).priority(100).reg(SEPG, epg0_vnid)
            .reg(DEPG, epg0_vnid).actions().out(OUTPORT).done());

    /* epg0 connected to fd0 */
    fe_epg0_fd0.push_back(Bldr(fe_epg0[0]).load(FD, 1).done());

    /* local EP ep0 */
    string ep0_mac = ep0->getMAC().toString();
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
            .load(OUTPORT, ep0_port).go(3).done());
    fe_ep0.push_back(Bldr().table(2).priority(15).ip().reg(RD, 1)
            .isEthDst(rmac).isIpDst(ep0_ip0)
            .actions().load(DEPG, epg0_vnid).load(OUTPORT, ep0_port)
            .ethSrc(rmac).ethDst(ep0_mac).decTtl().go(3).done());
    fe_ep0.push_back(Bldr().table(2).priority(20).arp().reg(RD, 1)
            .isEthDst(bmac).isTpa(ep0_ip0).isArpOp(1)
            .actions().load(DEPG, epg0_vnid).load(OUTPORT, ep0_port)
            .ethDst(ep0_mac).go(3).done());
    fe_ep0.push_back(Bldr(fe_ep0[7]).isIpDst(ep0_ip1).done());
    fe_ep0.push_back(Bldr(fe_ep0[8]).isTpa(ep0_ip1).done());

    /* ep0 when eg0 connected to fd0 */
    fe_ep0_fd0.push_back(Bldr().table(1).priority(140).in(ep0_port)
            .isEthSrc(ep0_mac).actions().load(SEPG, epg0_vnid).load(BD, 1)
            .load(FD, 1).load(RD, 1).go(2).done());

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
    string ep2_mac = ep2->getMAC().toString();
    string ep2_ip0 = *(ep2->getIPs().begin());
    fe_ep2.push_back(Bldr().table(2).priority(10).reg(BD, 1).isEthDst(ep2_mac)
            .actions().load(DEPG, epg0_vnid).load(OUTPORT, tunPort)
            .move(SEPG, TUNID).load(TUNDST, tunDst).go(3)
            .done());
    fe_ep2.push_back(Bldr().table(2).priority(15).ip().reg(RD, 1)
            .isEthDst(rmac).isIpDst(ep2_ip0).actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, tunPort).move(SEPG, TUNID).load(TUNDST, tunDst)
            .ethSrc(rmac).decTtl().go(3).done());
    fe_ep2.push_back(Bldr().table(2).priority(20).arp().reg(RD, 1)
            .isEthDst(bmac).isTpa(ep2_ip0).isArpOp(1)
            .actions().load(DEPG, epg0_vnid)
            .load(OUTPORT, tunPort).move(SEPG, TUNID).load(TUNDST, tunDst)
            .ethDst(ep2_mac).go(3).done());

    /* ep2 connected to new group epg1 */
    fe_ep2_eg1.push_back(Bldr(fe_ep2[1]).load(DEPG, epg1_vnid).done());
    fe_ep2_eg1.push_back(Bldr(fe_ep2[2]).load(DEPG, epg1_vnid).done());
}

