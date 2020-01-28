/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "MockFlowExecutor.h"
#include <opflexagent/logging.h>

#include <boost/test/unit_test.hpp>

#include <sstream>
#include <algorithm>

#include "ovs-shim.h"
extern "C" {
#include <openvswitch/dynamic-string.h>
#include <openvswitch/ofp-print.h>
#include <openvswitch/ofp-errors.h>
#include <openvswitch/ofp-flow.h>
#include <openflow/openflow-common.h>
}

namespace opflexagent {

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
    for (const string& s : tokens) {
        out.append(s);
    }
    return out;
}

MockFlowExecutor::MockFlowExecutor()
    : ignoreFlowMods(true), ignoreGroupMods(true), ignoreTlvMods(true) {}

bool MockFlowExecutor::Execute(const FlowEdit& flowEdits) {
    if (ignoreFlowMods) return true;

    FlowEdit editCopy = flowEdits;
    std::sort(editCopy.edits.begin(), editCopy.edits.end());

    const char *modStr[] = {"ADD", "MOD", "DEL"};
    DsP strBuf;

    for (const FlowEdit::Entry& ed : editCopy.edits) {
        if (ignoredFlowMods.find(ed.first) != ignoredFlowMods.end())
            continue;

        ofputil_flow_stats_format(strBuf.get(), ed.second->entry,
                                  NULL, NULL, true);
        string str = (const char*)(ds_cstr(strBuf.get())); // trim space

        BOOST_CHECK_MESSAGE(!flowMods.empty(), "\nexp:\ngot: " << ed);
        if (!flowMods.empty()) {
            mod_t exp = flowMods.front();
            flowMods.pop_front();
            BOOST_CHECK_MESSAGE(exp.first == ed.first,
                                "\nexp: " << modStr[exp.first] <<
                                "\ngot: " << ed);
            BOOST_CHECK_MESSAGE(exp.second == str,
                                "\nexp: " << modStr[exp.first] << "|"
                                << exp.second <<
                                "\ngot: " << ed);
        }
        ds_clear(strBuf.get());
    }

    return true;
}
bool MockFlowExecutor::Execute(const GroupEdit& groupEdits) {
    std::lock_guard<std::mutex> guard(group_mod_mutex);
    if (ignoreGroupMods) return true;

    for (const GroupEdit::Entry& ed : groupEdits.edits) {
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
bool MockFlowExecutor::Execute(const TlvEdit& TlvEdits) {
    if (ignoreTlvMods) return true;

    for (const TlvEdit::Entry& ed : TlvEdits.edits) {
        std::stringstream ss;
        ss << ed;
        BOOST_CHECK_MESSAGE(!tlvMods.empty(), "\nexp:\ngot: " << ss.str());
        if (!tlvMods.empty()) {
            tlv_mod_t exp = tlvMods.front();
            tlvMods.pop_front();
            BOOST_CHECK_MESSAGE(exp.second == ss.str(),
                                "\nexp: " << exp.second << "\ngot: " << ss.str());
        }
    }
    return true;
}
void MockFlowExecutor::Expect(FlowEdit::type mod, const string& fe) {
    ignoreFlowMods = false;
    flowMods.push_back(mod_t(mod, fe));
}
void MockFlowExecutor::Expect(FlowEdit::type mod, const vector<string>& fe) {
    ignoreFlowMods = false;
    for (const string& s : fe)
        flowMods.push_back(mod_t(mod, s));
}
void MockFlowExecutor::ExpectGroup(FlowEdit::type mod, const string& ge) {
    std::lock_guard<std::mutex> guard(group_mod_mutex);
    ignoreGroupMods = false;
    const char *modStr[] = {"ADD", "MOD", "DEL"};
    groupMods.push_back(canonicalizeGroupEntryStr(string(modStr[mod]) + "|" +
                                                  ge));
}
void MockFlowExecutor::Expect(TlvEdit::type mod, const string& te) {
    ignoreTlvMods = false;
    tlvMods.push_back(tlv_mod_t(mod, te));
}
void MockFlowExecutor::IgnoreTlvMods() {
    ignoreTlvMods = true;
    tlvMods.clear();
}
void MockFlowExecutor::IgnoreFlowMods() {
    ignoreFlowMods = true;
    flowMods.clear();
}
void MockFlowExecutor::IgnoreGroupMods() {
    std::lock_guard<std::mutex> guard(group_mod_mutex);
    ignoreGroupMods = true;
    groupMods.clear();
}
bool MockFlowExecutor::IsEmpty() { return flowMods.empty() || ignoreFlowMods; }
bool MockFlowExecutor::IsGroupEmpty() {
    std::lock_guard<std::mutex> guard(group_mod_mutex);
    return groupMods.empty() || ignoreGroupMods;
}
bool MockFlowExecutor::IsTlvEmpty() { return tlvMods.empty() || ignoreTlvMods; }
void MockFlowExecutor::Clear() {
    std::lock_guard<std::mutex> guard(group_mod_mutex);
    flowMods.clear();
    groupMods.clear();
    tlvMods.clear();
}

} // namespace opflexagent
