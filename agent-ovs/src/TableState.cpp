/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>

#include "logging.h"
#include "ovs.h"
#include "TableState.h"

namespace opflex {
namespace enforcer {
namespace flow {

using namespace std;
using namespace ovsagent;

/** FlowEntry **/

FlowEntry::FlowEntry() {
    entry = (ofputil_flow_stats*)calloc(1, sizeof(ofputil_flow_stats));
}

FlowEntry::~FlowEntry() {
    if (entry->ofpacts) {
        free((void *)entry->ofpacts);
    }
    free(entry);
}

bool
FlowEntry::MatchEq(const FlowEntry *rhs) {
    const ofputil_flow_stats *feRhs = rhs->entry;
    return entry != NULL && feRhs != NULL &&
            (entry->table_id == feRhs->table_id) &&
            (entry->priority == feRhs->priority) &&
            (entry->cookie == feRhs->cookie) &&
            match_equal(&entry->match, &feRhs->match);
}

bool
FlowEntry::ActionEq(const FlowEntry *rhs) {
    const ofputil_flow_stats *feRhs = rhs->entry;
    return entry != NULL && feRhs != NULL &&
            ofpacts_equal(entry->ofpacts, entry->ofpacts_len,
                          feRhs->ofpacts, feRhs->ofpacts_len);
}

ostream & operator<<(ostream& os, const FlowEntry& fe) {
    ds strBuf;
    ds_init(&strBuf);
    ofp_print_flow_stats(&strBuf, fe.entry);
    os << (const char*)(ds_cstr(&strBuf) + 1); // trim space
    ds_destroy(&strBuf);
    return os;
}

ostream& operator<<(ostream& os, const FlowEntryList& el) {
    for (int i = 0; i < el.size(); ++i) {
        os << endl << *(el[i]);
    }
    return os;
}

ostream& operator<<(ostream& os, const FlowEdit::Entry& fe) {
    static const char *op[] = {"ADD", "MOD", "DEL"};
    os << op[fe.first] << "|" << *(fe.second);
    return os;
}

ostream& operator<<(ostream& os, const FlowEdit& fe) {
    BOOST_FOREACH(const FlowEdit::Entry& e, fe.edits) {
        os << endl << e;
    }
}

/** GroupEdit **/

GroupEdit::GroupMod::GroupMod() {
    mod = new ofputil_group_mod();
    mod->command = OFPGC11_MODIFY;
    mod->type = OFPGT11_ALL;
    mod->group_id = 0;
    mod->command_bucket_id = OFPG15_BUCKET_ALL;
    list_init(&mod->buckets);
}

GroupEdit::GroupMod::~GroupMod() {
    if (mod) {
        ofputil_bucket_list_destroy(&mod->buckets);
        delete mod;
    }
    mod = NULL;
}

ostream & operator<<(ostream& os, const GroupEdit::Entry& ge) {
    static const char *groupTypeStr[] = {"all", "select", "indirect",
        "ff", "unknown" };
    ofputil_group_mod& mod = *(ge->mod);
    switch (mod.command) {
    case OFPGC11_ADD:      os << "ADD"; break;
    case OFPGC11_MODIFY:   os << "MOD"; break;
    case OFPGC11_DELETE:   os << "DEL"; break;
    default:               os << "Unknown";
    }
    os << "|group_id=" << mod.group_id << ",type="
       << groupTypeStr[std::min<uint8_t>(4, mod.type)];

    ofputil_bucket *bkt;
    LIST_FOR_EACH (bkt, list_node, &mod.buckets) {
        os << ",bucket=bucket_id:" << bkt->bucket_id << ",actions=";
        ds strBuf;
        ds_init(&strBuf);
        ofpacts_format(bkt->ofpacts, bkt->ofpacts_len, &strBuf);
        os << ds_cstr(&strBuf);
        ds_destroy(&strBuf);
    }
    return os;
}

/** TableState **/

void TableState::DiffEntry(const string& objId,
        const FlowEntryList& newEntries,
        FlowEdit& diffs) {
    EntryMap::iterator itr = entryMap.find(objId);
    const FlowEntryList& oldEntries =
            (itr != entryMap.end() ? itr->second : FlowEntryList());

    vector<bool> keep(oldEntries.size(), false);
    for(int j = 0; j < newEntries.size(); ++j) {

        FlowEntry *newFe = newEntries[j];
        bool found = false;

        for (int i = 0; i < keep.size(); ++i) {
            FlowEntry *currFe = oldEntries[i];
            if (currFe->MatchEq(newFe)) {
                keep[i] = true;
                found = true;
                if (!currFe->ActionEq(newFe)) {
                    diffs.edits.push_back(
                            FlowEdit::Entry(FlowEdit::mod, newFe));
                }
                break;
            }
        }
        if (!found) {
            diffs.edits.push_back(FlowEdit::Entry(FlowEdit::add, newFe));
        }
    }
    for (int i = 0; i < keep.size(); ++i) {
        if (keep[i] == false) {
            diffs.edits.push_back(
                    FlowEdit::Entry(FlowEdit::del, oldEntries[i]));
        }
    }
    LOG(DEBUG) << diffs.edits.size() << " diff(s), objId=" << objId << diffs;
}

void
TableState::Update(const string& objId, FlowEntryList& newEntries) {
    EntryMap::iterator itr = entryMap.find(objId);

    /* newEntries.empty() => delete */
    if (newEntries.empty() && itr != entryMap.end()) {
        itr->second.swap(newEntries);
        entryMap.erase(itr);
    } else {
        if (itr == entryMap.end()) {
            entryMap[objId].swap(newEntries);
        } else {
            itr->second.swap(newEntries);
        }
    }
}

}   // namespace flow
}   // namespace enforcer
}   // namespace opflex
