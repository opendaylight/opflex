/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>

#include "ovs.h"
#include "TableState.h"

namespace opflex {
namespace enforcer {
namespace flow {

using namespace std;

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

ostream & operator<<(ostream &os, const FlowEntry& fe) {
    ds strBuf;
    ds_init(&strBuf);
    ofp_print_flow_stats(&strBuf, fe.entry);
    os << (const char*)(ds_cstr(&strBuf) + 1); // trim space
    ds_destroy(&strBuf);
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
