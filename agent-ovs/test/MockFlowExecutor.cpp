/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "MockFlowExecutor.h"
#include "logging.h"

#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include <sstream>

namespace ovsagent {

using std::string;
using std::vector;

static string canonicalizeGroupEntryStr(const string& entryStr) {
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

MockFlowExecutor::MockFlowExecutor()
    : ignoreFlowMods(true), ignoreGroupMods(true) {}

bool MockFlowExecutor::Execute(const FlowEdit& flowEdits) {
    if (ignoreFlowMods) return true;

    const char *modStr[] = {"ADD", "MOD", "DEL"};
    struct ds strBuf;
    ds_init(&strBuf);

    BOOST_FOREACH(const FlowEdit::Entry& ed, flowEdits.edits) {
        if (ignoredFlowMods.find(ed.first) != ignoredFlowMods.end())
            continue;

        ofp_print_flow_stats(&strBuf, ed.second->entry);
        string str = (const char*)(ds_cstr(&strBuf)+1); // trim space

        BOOST_CHECK_MESSAGE(!flowMods.empty(), "\nexp:\ngot: " << ed);
        if (!flowMods.empty()) {
            MOD exp = flowMods.front();
            flowMods.pop_front();
            BOOST_CHECK_MESSAGE(exp.first == ed.first,
                                "\nexp: " << modStr[exp.first] <<
                                "\ngot: " << ed);
            BOOST_CHECK_MESSAGE(exp.second == str,
                                "\nexp: " << modStr[exp.first] << "|"
                                << exp.second <<
                                "\ngot: " << ed);
        }
        ds_clear(&strBuf);
    }
    ds_destroy(&strBuf);

    return true;
}
bool MockFlowExecutor::Execute(const GroupEdit& groupEdits) {
    if (ignoreGroupMods) return true;

    BOOST_FOREACH(const GroupEdit::Entry& ed, groupEdits.edits) {
        LOG(DEBUG) << "*** GroupMod " << ed;
        std::stringstream ss;
        ss << ed;
        string edStr = canonicalizeGroupEntryStr(ss.str());

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
void MockFlowExecutor::Expect(FlowEdit::TYPE mod, const string& fe) {
    ignoreFlowMods = false;
    flowMods.push_back(MOD(mod, fe));
}
void MockFlowExecutor::Expect(FlowEdit::TYPE mod, const vector<string>& fe) {
    ignoreFlowMods = false;
    BOOST_FOREACH(const string& s, fe)
        flowMods.push_back(MOD(mod, s));
}
void MockFlowExecutor::ExpectGroup(FlowEdit::TYPE mod, const string& ge) {
    ignoreGroupMods = false;
    const char *modStr[] = {"ADD", "MOD", "DEL"};
    groupMods.push_back(canonicalizeGroupEntryStr(string(modStr[mod]) + "|" +
                                                  ge));
}
void MockFlowExecutor::IgnoreFlowMods() {
    ignoreFlowMods = true;
    flowMods.clear();
}
void MockFlowExecutor::IgnoreGroupMods() {
    ignoreGroupMods = true;
    groupMods.clear();
}
bool MockFlowExecutor::IsEmpty() { return flowMods.empty() || ignoreFlowMods; }
bool MockFlowExecutor::IsGroupEmpty() {
    return groupMods.empty() || ignoreGroupMods;
}
void MockFlowExecutor::Clear() {
    flowMods.clear();
    groupMods.clear();
}

void addExpFlowEntry(FlowEntryList* tables, const string& flowMod) {
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

    struct ds strBuf;
    ds_init(&strBuf);
    ofp_print_flow_stats(&strBuf, e->entry);
    string str = (const char*)(ds_cstr(&strBuf)+1); // trim space
    BOOST_CHECK_EQUAL(str, flowMod);
    ds_destroy(&strBuf);
}

} // namespace ovsagent
