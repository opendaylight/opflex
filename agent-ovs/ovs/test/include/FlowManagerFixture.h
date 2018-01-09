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

namespace opflexagent {

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
    SEPG, SEPG12, DEPG, BD, FD, RD, OUTPORT, TUNID, TUNSRC, TUNDST, VLAN,
    ETHSRC, ETHDST, ARPOP, ARPSHA, ARPTHA, ARPSPA, ARPTPA, METADATA,
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
    Bldr& noVlan() { rep(",vlan_tci=0x0000"); return *this; }
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

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_FLOWTEST_H_ */
