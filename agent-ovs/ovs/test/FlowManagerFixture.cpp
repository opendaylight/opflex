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
    "NXM_NX_REG5[]", "NXM_NX_REG6[]", "NXM_NX_REG7[]", "NXM_NX_TUN_ID[0..31]",
    "NXM_NX_TUN_IPV4_SRC[]", "NXM_NX_TUN_IPV4_DST[]", "OXM_OF_VLAN_VID[]",
    "NXM_OF_ETH_SRC[]", "NXM_OF_ETH_DST[]", "NXM_OF_ARP_OP[]",
    "NXM_NX_ARP_SHA[]", "NXM_NX_ARP_THA[]", "NXM_OF_ARP_SPA[]",
    "NXM_OF_ARP_TPA[]", "OXM_OF_METADATA[]", "NXM_NX_PKT_MARK[]"
};
static string rstr1[] =
    { "reg0", "reg0", "reg2", "reg4", "reg5", "reg6", "reg7",
      "", "", "", "", "", "", "" ,"", "", "", "", ""};

Bldr::Bldr(const string& init) : entry(init) {
    cntr = 0;
    if (entry.empty()) {
        entry = "cookie=0x0, duration=0s, table=0, n_packets=0, "
            "n_bytes=0, idle_age=0, priority=0";
    }
}

Bldr::Bldr(const string& init, uint32_t flag) : entry(init) {
    cntr = 0;
    if (entry.empty()) {
        if (flag == SEND_FLOW_REM)
            entry = "cookie=0x0, duration=0s, table=0, n_packets=0, "
                "n_bytes=0, send_flow_rem idle_age=0, priority=0";
        else
            entry = "cookie=0x0, duration=0s, table=0, n_packets=0, "
                "n_bytes=0, idle_age=0, priority=0";
    }
}

Bldr& Bldr::reg(REG r, uint32_t v) {
    rep("," + rstr1[r] + "=", str(v, true));
    return *this;
}

Bldr& Bldr::load64(REG r, uint64_t v) {
    rep("load:", str64(v, true), "->" + rstr[r]);
    return *this;
}

Bldr& Bldr::load(REG r, uint32_t v) {
    rep("load:", str(v, true), "->" + rstr[r]);
    return *this;
}

Bldr& Bldr::load(REG r, const string& v) {
    rep("load:", v, "->" + rstr[r]);
    return *this;
}

Bldr& Bldr::move(REG s, REG d) {
    rep("move:"+rstr[s]+"->"+rstr[d]);
    return *this;
}

Bldr& Bldr::out(REG r) {
    rep("output:" + rstr[r]);
    return *this;
}

void Bldr::rep(const string& pfx, const string& val, const string& sfx) {
    string sep(cntr == 2 ? "," : "");
    if (cntr == 1) {
        cntr = 2;
    }
    size_t pos2 = 0;
    do {
        size_t pos1 = entry.find(pfx, pos2);
        if (pos1 == string::npos) {
            entry.append(sep).append(pfx).append(val).append(sfx);
            return;
        }
        pos2 = pos1+pfx.size();
        while (pos2 < entry.size() && entry[pos2] != ',' &&
               entry[pos2] != ' ' && entry[pos2] != '-') {
            ++pos2;
        }
        if (!sfx.empty()) {
            if (pos2 < entry.size() &&
                entry.compare(pos2, sfx.size(), sfx)) {
                continue;
            }
        }
        string tmp;
        if (pos1 > 0)
            tmp += entry.substr(0, pos1) + sep;
        tmp += pfx + val + entry.substr(pos2);
        entry = tmp;
        return;
    } while(true);
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

} // namespace opflexagent
