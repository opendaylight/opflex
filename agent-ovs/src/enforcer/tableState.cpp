/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>

#include <flow/tableState.h>
#include <opflex/modb/URI.h>

#include <ovs.h>

namespace opflex {
namespace enforcer {
namespace flow {

using namespace std;
using namespace opflex::modb;

/** FlowEntry **/

FlowEntry::FlowEntry() {
    entry = (ofputil_flow_stats*)calloc(1, sizeof(ofputil_flow_stats));
}

FlowEntry::~FlowEntry() {
    free(entry);
}

bool
FlowEntry::MatchEq(const Entry *rhs) {
    assert(dynamic_cast<const FlowEntry*>(rhs) != NULL);
    const ofputil_flow_stats *feRhs = dynamic_cast<const FlowEntry*>(rhs)->entry;
    return entry != NULL && feRhs != NULL &&
            (entry->table_id == feRhs->table_id) &&
            (entry->priority == feRhs->priority) &&
            (entry->cookie == feRhs->cookie) &&
            match_equal(&entry->match, &feRhs->match);
}

bool
FlowEntry::ActionEq(const Entry *rhs) {
    assert(dynamic_cast<const FlowEntry*>(rhs) != NULL);
    const ofputil_flow_stats *feRhs = dynamic_cast<const FlowEntry*>(rhs)->entry;
    return entry != NULL && feRhs != NULL &&
            ofpacts_equal(entry->ofpacts, entry->ofpacts_len,
                          feRhs->ofpacts, feRhs->ofpacts_len);
}


/** TableState **/

void TableState::DiffEntry(const URI& uri,
        const EntryList& newEntries,
        FlowEdit& diffs) {
    EntryMap::iterator itr = entryMap.find(uri);
    const EntryList& oldEntries = (itr != entryMap.end() ? itr->second : EntryList());

    vector<bool> keep(oldEntries.size(), false);
    for(int j = 0; j < newEntries.size(); ++j) {

        Entry *newFe = newEntries[j];
        bool found = false;

        for (int i = 0; i < keep.size(); ++i) {
            Entry *currFe = oldEntries[i];
            if (currFe->MatchEq(newFe)) {
                keep[i] = true;
                found = true;
                if (!currFe->ActionEq(newFe)) {
                    diffs.edits.push_back(
                            FlowEdit::EntryEdit(FlowEdit::mod, newFe));
                }
                break;
            }
        }
        if (!found) {
            diffs.edits.push_back(FlowEdit::EntryEdit(FlowEdit::add, newFe));
        }
    }
    for (int i = 0; i < keep.size(); ++i) {
        if (keep[i] == false) {
            diffs.edits.push_back(
                    FlowEdit::EntryEdit(FlowEdit::del, oldEntries[i]));
        }
    }
}

void
TableState::Update(const URI& uri, EntryList& newEntries) {
    EntryMap::iterator itr = entryMap.find(uri);

    /* newEntries.empty() => delete */
    if (newEntries.empty() && itr != entryMap.end()) {
        itr->second.swap(newEntries);
        entryMap.erase(itr);
    } else {
        if (itr == entryMap.end()) {
            entryMap[uri].swap(newEntries);
        } else {
            itr->second.swap(newEntries);
        }
    }
}

}   // namespace flow
}   // namespace enforcer
}   // namespace opflex
