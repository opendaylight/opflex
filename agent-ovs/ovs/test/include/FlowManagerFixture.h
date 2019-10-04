/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TEST_FLOWTEST_H_
#define OPFLEXAGENT_TEST_FLOWTEST_H_

#include <opflexagent/test/ModbFixture.h>
#include "SwitchManager.h"
#include "MockSwitchManager.h"
#include "TableState.h"
#include "CtZoneManager.h"
#include "PacketInHandler.h"
#include <opflexagent/logging.h>

#include <vector>
#include <thread>
#include <list>
#include <iomanip>

namespace opflexagent {

class FlowManagerFixture : public ModbFixture {
    typedef opflex::ofcore::OFConstants::OpflexElementMode opflex_elem_t;
public:
    FlowManagerFixture(opflex_elem_t mode = opflex_elem_t::INVALID_MODE)
        : ModbFixture(mode), ctZoneManager(idGen),
        switchManager(agent, exec, reader, portmapper) {
        switchManager.setSyncDelayOnConnect(0);
        ctZoneManager.setCtZoneRange(1, 65534);
        ctZoneManager.init("conntrack");
    }
    virtual ~FlowManagerFixture() {
    }

    virtual void start() {
        switchManager.start("placeholder");
    }

    virtual void stop() {
        switchManager.stop();
        agent.stop();
    }

    void setConnected() {
        switchManager.enableSync();
        switchManager.connect();
    }

    virtual void clearExpFlowTables() {
        for (FlowEntryList& fel : expTables)
            fel.clear();
    }

    IdGenerator idGen;
    CtZoneManager ctZoneManager;
    MockFlowExecutor exec;
    MockFlowReader reader;
    MockPortMapper portmapper;
    MockSwitchManager switchManager;

    std::vector<FlowEntryList> expTables;
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

enum REG {
    SEPG, SEPG12, DEPG, BD, FD, FD12, RD, OUTPORT, TUNID, TUNSRC, TUNDST,
    VLAN, ETHSRC, ETHDST, ARPOP, ARPSHA, ARPTHA, ARPSPA, ARPTPA, METADATA,
    PKT_MARK
};

enum FLAG {
    SEND_FLOW_REM=1, NO_PKT_COUNTS, NO_BYT_COUNTS, CHECK_OVERLAP,
    RESET_COUNTS
};

/**
 * Helper class to build string representation of a flow entry.
 */
class Bldr {
public:
    Bldr();
    Bldr(uint32_t flag);

    std::string done();
    Bldr& table(uint8_t t) {
        _table = "table=" + str(t);
        return *this;
    }
    Bldr& priority(uint16_t p) {
        _priority = "priority=" + str(p); return *this;
    }
    Bldr& cookie(uint64_t c) {
        _cookie = "cookie=" + str64(c, true); return *this;
    }
    Bldr& tunId(uint32_t id) { m("tun_id", str(id, true)); return *this; }
    Bldr& in(uint32_t p) { m("in_port", str(p)); return *this; }
    Bldr& reg(REG r, uint32_t v);
    Bldr& isEthSrc(const std::string& s) { m("dl_src", s); return *this; }
    Bldr& isEthDst(const std::string& s) { m("dl_dst", s); return *this; }
    Bldr& ip() { m("ip"); return *this; }
    Bldr& ipv6() { m("ipv6"); return *this; }
    Bldr& arp() { m("arp"); return *this; }
    Bldr& tcp() { m("tcp"); return *this; }
    Bldr& tcp6() { m("tcp6"); return *this; }
    Bldr& udp() { m("udp"); return *this; }
    Bldr& udp6() { m("udp6"); return *this; }
    Bldr& icmp() { m("icmp"); return *this; }
    Bldr& icmp6() { m("icmp6"); return *this; }
    Bldr& icmp_type(uint8_t t) { m("icmp_type", str(t)); return *this; }
    Bldr& icmp_code(uint8_t c) { m("icmp_code", str(c)); return *this; }
    Bldr& isArpOp(uint8_t op) { m("arp_op", str(op)); return *this; }
    Bldr& isSpa(const std::string& s) { m("arp_spa", s); return *this; }
    Bldr& isTpa(const std::string& s) {
        m("arp_tpa", s); return *this;
    }
    Bldr& isIpSrc(const std::string& s) {
        m("nw_src", s); return *this;
    }
    Bldr& isIpv6Src(const std::string& s) {
        m("ipv6_src", s); return *this;
    }
    Bldr& isIpDst(const std::string& s) {
        m("nw_dst", s); return *this;
    }
    Bldr& isIpv6Dst(const std::string& s) {
        m("ipv6_dst", s); return *this;
    }
    Bldr& isTpSrc(uint16_t p) { m("tp_src", str(p)); return *this; }
    Bldr& isTpDst(uint16_t p) { m("tp_dst", str(p)); return *this; }
    Bldr& isTpSrc(uint16_t p, uint16_t mask) {
        m("tp_src", str(p, true) + "/" + str(mask, true)); return *this;
    }
    Bldr& isTpDst(uint16_t p, uint16_t mask) {
        m("tp_dst", str(p, true) + "/" + str(mask, true)); return *this;
    }
    Bldr& isVlan(uint16_t v) { m("dl_vlan", str(v)); return *this; }
    Bldr& noVlan() { m("vlan_tci=0x0000/0x1fff"); return *this; }
    Bldr& isVlanTci(const std::string& v) {
        m("vlan_tci", v); return *this;
    }
    Bldr& isVlanTci(uint16_t v, uint16_t mask) {
        if (mask == 0x1fff) {
            isVlan(v & 0xfff);
        } else {
            m() << "vlan_tci=0x"
                << std::setfill('0') << std::setw(4) << std::hex << v
                << "/0x"
                << std::setfill('0') << std::setw(4) << std::hex << mask;
        }
        return *this;
    }
    Bldr& isNdTarget(const std::string& t) {
        m("nd_target", t); return *this;
    }
    Bldr& isEth(uint16_t t)  { m("dl_type", str(t, true)); return *this; }
    Bldr& isTcpFlags(const std::string& s)  {
        m("tcp_flags", s); return *this;
    }
    Bldr& isCtState(const std::string& s) {
        m("ct_state", s); return *this;
    }
    Bldr& isCtMark(const std::string& s) {
        m("ct_mark", s); return *this;
    }
    Bldr& isMdAct(uint8_t a) {
        m("metadata", str(a, true) + "/0xff"); return *this;
    }
    Bldr& isPolicyApplied() { m("metadata", "0x100/0x100"); return *this; }
    Bldr& isFromServiceIface(bool yes = true) {
        m() << "metadata=" << (yes ? "0x200" : "0") << "/0x200";
        return *this;
    }
    Bldr& isMd(const std::string& md) { m("metadata", md); return *this; }
    Bldr& isPktMark(uint32_t mark) {
        m("pkt_mark", str(mark, true)); return *this;
    }
    Bldr& actions() { return *this; }
    Bldr& drop() { a("drop"); return *this; }
    Bldr& load64(REG r, uint64_t v);
    Bldr& load(REG r, uint32_t v);
    Bldr& load(REG r, const std::string& v);
    Bldr& move(REG s, REG d);
    Bldr& ethSrc(const std::string& s) {
        a("set_field", s + "->eth_src"); return *this;
    }
    Bldr& ethDst(const std::string& s) {
        a("set_field", s + "->eth_dst"); return *this;
    }
    Bldr& ipSrc(const std::string& s) {
        a("mod_nw_src", s); return *this;
    }
    Bldr& ipDst(const std::string& s) {
        a("mod_nw_dst", s); return *this;
    }
    Bldr& tpSrc(uint16_t p) {
        a("mod_tp_src", str(p)); return *this;
    }
    Bldr& tpDst(uint16_t p) {
        a("mod_tp_dst", str(p)); return *this;
    }
    Bldr& ipv6Src(const std::string& s) {
        a("set_field", s + "->ipv6_src"); return *this;
    }
    Bldr& ipv6Dst(const std::string& s) {
        a("set_field", s + "->ipv6_dst"); return *this;
    }
    Bldr& go(uint8_t t) { a("goto_table", str(t)); return *this; }
    Bldr& out(REG r);
    Bldr& decTtl() { a("dec_ttl"); return *this; }
    Bldr& group(uint32_t g) { a("group", str(g)); return *this; }
    Bldr& outPort(uint32_t p) { a("output", str(p)); return *this; }
    Bldr& pushVlan() { a("push_vlan:0x8100"); return *this; }
    Bldr& popVlan() { a("pop_vlan"); return *this; }
    Bldr& setVlan(uint16_t v) { a("set_vlan_vid", str(v)); return *this; }
    Bldr& inport() { a("IN_PORT"); return *this; }
    Bldr& controller(uint16_t len) {
        a("CONTROLLER", str(len)); return *this;
    }
    Bldr& meta(uint64_t meta, uint64_t mask) {
        a("write_metadata", str(meta, true) + "/" + str(mask, true));
        return *this;
    }
    Bldr& mdAct(uint8_t act) {
        a("write_metadata", str(act, true) + "/0xff"); return *this;
    }
    Bldr& ct(const std::string& s) {
        a() << "ct(" << s << ")"; return *this;
    }
    Bldr& learn(const std::string& s) {
        a() << "learn(" << s << ")"; return *this;
    }
    Bldr& multipath(const std::string& s) {
        a() << "multipath(" << s << ")"; return *this;
    }
    Bldr& polApplied() { a("write_metadata", "0x100/0x100"); return *this; }
    Bldr& resubmit(uint8_t t) {
        a() << "resubmit(," << str(t) << ")"; return *this;
    }
    Bldr& dropLog(uint32_t table_id);

private:
    std::stringstream& m();
    std::stringstream& a();
    void m(const std::string& pfx, const std::string& val="");
    void a(const std::string& pfx, const std::string& val="");
    std::string str(int i, bool hex = false);
    std::string str64(uint64_t i, bool hex = false);
    std::string strpad(int i);

    uint32_t _flag;
    std::list<std::stringstream> _match;
    std::list<std::stringstream> _action;
    std::string _table;
    std::string _cookie;
    std::string _priority;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_FLOWTEST_H_ */
