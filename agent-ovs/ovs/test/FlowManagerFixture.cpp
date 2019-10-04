/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "FlowManagerFixture.h"

#include "ovs-ofputil.h"

extern "C" {
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>
}

namespace opflexagent {

using std::string;

void addExpFlowEntry(std::vector<FlowEntryList>& tables,
                     const string& flowMod) {
    struct ofputil_flow_mod fm;
    enum ofputil_protocol prots;
    char* error =
        parse_ofp_flow_mod_str(&fm, flowMod.c_str(), OFPFC_ADD, &prots);
    if (error) {
        LOG(ERROR) << "Could not parse: " << flowMod << ": " << error;
        return;
    }

    FlowEntryPtr e(new FlowEntry());
    e->entry->match = fm.match;
    e->entry->cookie = fm.new_cookie;
    e->entry->table_id = fm.table_id;
    e->entry->priority = fm.priority;
    e->entry->idle_timeout = fm.idle_timeout;
    e->entry->hard_timeout = fm.hard_timeout;
    e->entry->ofpacts = fm.ofpacts;
    fm.ofpacts = NULL;
    e->entry->ofpacts_len = fm.ofpacts_len;
    e->entry->flags = fm.flags;
    override_flow_has_vlan(e->entry->ofpacts, e->entry->ofpacts_len);
    tables[fm.table_id].push_back(e);

    DsP strBuf;
    ofp_print_flow_stats(strBuf.get(), e->entry);
    string str = (const char*)(ds_cstr(strBuf.get())+1); // trim space
    BOOST_CHECK_EQUAL(str, flowMod);
}

void printAllDiffs(std::vector<FlowEntryList>& expected,
                   FlowEdit* diffs) {
    for (size_t i = 0; i < expected.size(); i++) {
        if (diffs[i].edits.size() != 0) {
            LOG(ERROR) << "== Expected state for table " << i << ": ==";
            for (size_t j = 0; j < expected[i].size(); ++j) {
                LOG(ERROR) << *(expected[i][j]);
            }
            LOG(ERROR) << " == Diffs from expected state to match result table "
                      << i << ": ==";
            for (size_t j = 0; j < diffs[i].edits.size(); ++j) {
                LOG(ERROR) << diffs[i].edits[j];
            }
        }
    }
}

void doDiffTables(SwitchManager* switchManager,
                  std::vector<FlowEntryList>& expected,
                  FlowEdit* diffs,
                  volatile int* fail) {
    for (size_t i = 0; i < expected.size(); i++) {
        diffs[i].edits.clear();
        switchManager->diffTableState(i, expected[i], diffs[i]);
    }
    int failed = 0;
    for (size_t i = 0; i < expected.size(); i++) {
        if (diffs[i].edits.size() > 0) failed += 1;
    }
    *fail = failed;
}

void diffTables(Agent& agent,
                SwitchManager& switchManager,
                std::vector<FlowEntryList>& expected,
                FlowEdit* diffs,
                volatile int* fail) {
    *fail = 512;
    agent.getAgentIOService().dispatch(bind(doDiffTables, &switchManager,
                                            boost::ref(expected), diffs, fail));
    WAIT_FOR(*fail != 512, 1000);
}

static string rstr[] = {
    "NXM_NX_REG0[]", "NXM_NX_REG0[0..11]", "NXM_NX_REG2[]", "NXM_NX_REG4[]",
    "NXM_NX_REG5[]", "NXM_NX_REG5[0..11]", "NXM_NX_REG6[]", "NXM_NX_REG7[]",
    "NXM_NX_TUN_ID[0..31]", "NXM_NX_TUN_IPV4_SRC[]", "NXM_NX_TUN_IPV4_DST[]",
    "OXM_OF_VLAN_VID[]", "NXM_OF_ETH_SRC[]", "NXM_OF_ETH_DST[]",
    "NXM_OF_ARP_OP[]", "NXM_NX_ARP_SHA[]", "NXM_NX_ARP_THA[]",
    "NXM_OF_ARP_SPA[]", "NXM_OF_ARP_TPA[]", "OXM_OF_METADATA[]",
    "NXM_NX_PKT_MARK[]"
};
static string rstr1[] =
    { "reg0", "reg0", "reg2", "reg4", "reg5", "reg5", "reg6", "reg7",
      "", "", "", "", "", "", "" ,"", "", "", "", ""};

Bldr::Bldr() :
    _flag(0), _table("table=0"), _cookie("cookie=0x0"),
    _priority("priority=0") {}

Bldr::Bldr(uint32_t flag) :
    _flag(flag), _table("table=0"), _cookie("cookie=0x0"),
    _priority("priority=0") {}

std::string Bldr::done() {
    std::stringstream ss;
    ss << _cookie << ", duration=0s, "
       << _table << ", n_packets=0, n_bytes=0, ";
    if (_flag == SEND_FLOW_REM) {
        ss << "send_flow_rem ";
    }
    ss << "idle_age=0, " << _priority;
    for (auto& m : _match) {
        ss << "," << m.str();
    }
    ss << " actions=";
    bool first = true;
    for (auto& a : _action) {
        if (!first) ss << ",";
        first = false;
        ss << a.str();
    }
    return ss.str();
}
Bldr& Bldr::reg(REG r, uint32_t v) {
    m(rstr1[r], str(v, true));
    return *this;
}

Bldr& Bldr::load64(REG r, uint64_t v) {
    a("load", str64(v, true) + "->" + rstr[r]);
    return *this;
}

Bldr& Bldr::load(REG r, uint32_t v) {
    a("load", str(v, true) + "->" + rstr[r]);
    return *this;
}

Bldr& Bldr::load(REG r, const string& v) {
    a("load", v + "->" + rstr[r]);
    return *this;
}

Bldr& Bldr::move(REG s, REG d) {
    a("move", rstr[s]+"->"+rstr[d]);
    return *this;
}

Bldr& Bldr::out(REG r) {
    a("output", rstr[r]);
    return *this;
}

std::stringstream& Bldr::m() {
    _match.emplace_back();
    return _match.back();
}

std::stringstream& Bldr::a() {
    _action.emplace_back();
    return _action.back();
}

void Bldr::m(const std::string& pfx, const std::string& val) {
    auto& s = m();
    s << pfx;
    if (val.size()) {
        s << "=" << val;
    }
}
void Bldr::a(const std::string& pfx, const std::string& val) {
    auto& s = a();
    s << pfx;
    if (val.size()) {
        s << ":" << val;
    }
}

string Bldr::str(int i, bool hex) {
    char buf[20];
    sprintf(buf, hex && i != 0 ? "0x%x" : "%d", i);
    return buf;
}
string Bldr::str64(uint64_t i, bool hex) {
    char buf[20];
    sprintf(buf, hex && i != 0 ? "0x%lx" : "%lu", i);
    return buf;
}
string Bldr::strpad(int i) {
    char buf[10];
    sprintf(buf, "0x%04x", i);
    return buf;
}

Bldr& Bldr::dropLog(uint32_t table_id) {
    a("move", "NXM_NX_REG0[]->NXM_NX_TUN_METADATA0[0..31]");
    a("move", "NXM_NX_REG1[]->NXM_NX_TUN_METADATA1[0..31]");
    a("move", "NXM_NX_REG2[]->NXM_NX_TUN_METADATA2[0..31]");
    a("move", "NXM_NX_REG3[]->NXM_NX_TUN_METADATA3[0..31]");
    a("move", "NXM_NX_REG4[]->NXM_NX_TUN_METADATA4[0..31]");
    a("move", "NXM_NX_REG5[]->NXM_NX_TUN_METADATA5[0..31]");
    a("move", "NXM_NX_REG6[]->NXM_NX_TUN_METADATA6[0..31]");
    a("move", "NXM_NX_REG7[]->NXM_NX_TUN_METADATA7[0..31]");
    a("move", "NXM_NX_CT_STATE[]->NXM_NX_TUN_METADATA8[0..31]");
    a("move", "NXM_NX_CT_ZONE[]->NXM_NX_TUN_METADATA9[0..15]");
    a("move", "NXM_NX_CT_MARK[]->NXM_NX_TUN_METADATA10[0..31]");
    a("move", "NXM_NX_CT_LABEL[]->NXM_NX_TUN_METADATA11[0..127]");
    /*
    char buf[32];
    std::snprintf(buf, 32,"%d->NXM_NX_TUN_METADATA12[0..31]",table_id);
    string s(buf);
    a("load", s );
    */
    return *this;
}

} // namespace opflexagent
