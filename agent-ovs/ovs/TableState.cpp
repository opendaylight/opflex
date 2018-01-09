/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <unordered_map>
#include <unordered_set>

#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>

#include "TableState.h"
#include <opflexagent/logging.h>
#include "ovs-shim.h"
#include "ovs-ofputil.h"

#include <openvswitch/ofp-print.h>
#include <openvswitch/list.h>

extern "C" {
#include <openvswitch/dynamic-string.h>
}


namespace opflexagent {

struct match_key_t {
    uint16_t prio;
    struct match match;
};

} /* namespace opflexagent */

namespace std {

template<> struct hash<opflexagent::match_key_t> {
    size_t operator()(const opflexagent::match_key_t& match_key) const noexcept {
        size_t hashv = match_hash(&match_key.match, 0);
        boost::hash_combine(hashv, match_key.prio);
        return hashv;
    }
};

} /* namespace std */

namespace opflexagent {

using std::ostream;
using std::vector;
using std::make_pair;
using boost::optional;

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
FlowEntry::matchEq(const FlowEntry *rhs) {
    const ofputil_flow_stats *feRhs = rhs->entry;
    return entry != NULL && feRhs != NULL &&
        (entry->table_id == feRhs->table_id) &&
        (entry->priority == feRhs->priority) &&
        match_equal(&entry->match, &feRhs->match);
}

bool
FlowEntry::actionEq(const FlowEntry *rhs) {
    const ofputil_flow_stats *feRhs = rhs->entry;
    return entry != NULL && feRhs != NULL &&
            action_equal(entry->ofpacts, entry->ofpacts_len,
                         feRhs->ofpacts, feRhs->ofpacts_len);
}

ostream & operator<<(ostream& os, const FlowEntry& fe) {
    os << *fe.entry;
    return os;
}

ostream & operator<<(ostream& os, const struct ofputil_flow_stats& fs) {
    DsP str;
    ofp_print_flow_stats(str.get(), (ofputil_flow_stats*)&fs);
    os << (const char*)(ds_cstr(str.get()) + 1); // trim space
    return os;
}

ostream & operator<<(ostream& os, const match& m) {
    DsP str;
    match_format(&m, str.get(), OFP_DEFAULT_PRIORITY);
    os << (const char*)(ds_cstr(str.get()));
    return os;
}

ostream& operator<<(ostream& os, const FlowEdit::Entry& fe) {
    static const char *op[] = {"ADD", "MOD", "DEL"};
    os << op[fe.first] << "|" << *(fe.second);
    return os;
}

/** FlowEdit **/
void FlowEdit::add(type t, FlowEntryPtr fe) {
    edits.push_back(std::make_pair(t, fe));
}

bool operator<(const FlowEdit::Entry& f1, const FlowEdit::Entry& f2) {
    if (f1.first < f2.first) return true;
    if (f1.first > f2.first) return false;
    int r = std::memcmp(&f1.second->entry->match, &f2.second->entry->match,
                        sizeof(struct match));
    return r < 0;
}

/** GroupEdit **/

GroupEdit::GroupMod::GroupMod() {
    mod = new ofputil_group_mod();
    mod->command = OFPGC11_MODIFY;
    mod->type = OFPGT11_ALL;
    mod->group_id = 0;
    mod->command_bucket_id = OFPG15_BUCKET_ALL;
    ovs_list_init(&mod->buckets);
}

GroupEdit::GroupMod::~GroupMod() {
    if (mod) {
        ofputil_bucket_list_destroy(&mod->buckets);
        delete mod;
    }
    mod = NULL;
}

bool GroupEdit::groupEq(const GroupEdit::Entry& lhs,
                        const GroupEdit::Entry& rhs) {
    if (lhs == rhs) {
        return true;
    }
    if (lhs == NULL || rhs == NULL) {
        return false;
    }
    return group_mod_equal(lhs->mod, rhs->mod);
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
        DsP str;
        format_action(bkt->ofpacts, bkt->ofpacts_len, str.get());
        os << ds_cstr(str.get());
    }
    return os;
}

/** TableState **/

typedef std::vector<FlowEntryPtr> flow_vec_t;
typedef std::pair<std::string, FlowEntryPtr> obj_id_flow_t;
typedef std::vector<obj_id_flow_t> obj_id_flow_vec_t;
typedef std::unordered_map<match_key_t, obj_id_flow_vec_t> match_obj_map_t;
typedef std::unordered_map<match_key_t, flow_vec_t> match_map_t;
typedef std::unordered_map<std::string, match_map_t> entry_map_t;
typedef std::unordered_set<match_key_t> cookie_set_t;
typedef std::unordered_map<uint64_t, cookie_set_t> cookie_map_t;

bool operator==(const match_key_t& lhs, const match_key_t& rhs) {
    return lhs.prio == rhs.prio && match_equal(&lhs.match, &rhs.match);
}
bool operator!=(const match_key_t& lhs, const match_key_t& rhs) {
    return !(lhs == rhs);
}

class TableState::TableStateImpl {
public:
    entry_map_t entry_map;
    match_obj_map_t match_obj_map;
    cookie_map_t cookie_map;
};

TableState::TableState() : pimpl(new TableStateImpl()) { }

TableState::TableState(const TableState& ts)
    : pimpl(new TableStateImpl(*ts.pimpl)) { }

TableState::~TableState() {
    delete pimpl;
}

void TableState::diffSnapshot(const FlowEntryList& oldEntries,
                              FlowEdit& diffs) const {
    typedef std::pair<bool, FlowEntryPtr> visited_fe_t;
    typedef std::unordered_map<match_key_t, visited_fe_t> old_entry_map_t;

    diffs.edits.clear();

    old_entry_map_t old_entries;
    for (const FlowEntryPtr& fe : oldEntries) {
        match_key_t key;
        key.prio = fe->entry->priority;
        key.match = fe->entry->match;
        old_entries[key] = make_pair(false, fe);
    }

    // Add/mod any matches in the object map
    for (match_obj_map_t::value_type& e : pimpl->match_obj_map) {
        old_entry_map_t::iterator it = old_entries.find(e.first);
        if (it == old_entries.end()) {
            diffs.add(FlowEdit::ADD, e.second.front().second);
        } else {
            it->second.first = true;
            FlowEntryPtr& olde = it->second.second;
            FlowEntryPtr& newe = e.second.front().second;
            if (!newe->actionEq(olde.get())) {
                diffs.add(FlowEdit::MOD, newe);
            }
        }
    }

    // Remove unvisited entries from the old entry list
    for (old_entry_map_t::value_type& e : old_entries) {
        if (e.second.first) continue;
        diffs.add(FlowEdit::DEL, e.second.second);
    }
}

void TableState::forEachCookieMatch(cookie_callback_t& cb) const {
    for (const auto& cookies : pimpl->cookie_map) {
        for (const auto& match_key : cookies.second) {
            cb(ovs_ntohll(cookies.first), match_key.prio, match_key.match);
        }
    }
}

static void updateCookieMap(cookie_map_t& cookie_map,
                            uint64_t oldCookie, uint64_t newCookie,
                            const struct match_key_t& match) {
    if (oldCookie == newCookie)
        return;
    if (oldCookie != 0) {
        cookie_map_t::iterator it = cookie_map.find(oldCookie);
        if (it != cookie_map.end()) {
            it->second.erase(match);
            if (it->second.size() == 0)
                cookie_map.erase(it);
        }
    }

    if (newCookie != 0) {
        cookie_set_t& cset = cookie_map[newCookie];
        cset.insert(match);
    }
}

void TableState::apply(const std::string& objId,
                       FlowEntryList& newEntries,
                       /* out */ FlowEdit& diffs) {
    diffs.edits.clear();

    match_map_t new_entries;
    for (const FlowEntryPtr& fe : newEntries) {
        match_key_t key;
        key.prio = fe->entry->priority;
        key.match = fe->entry->match;
        new_entries[key].push_back(fe);
    }

    // load new entries
    for (match_map_t::value_type& e : new_entries) {
        // check if there's an overlapping match already in the table
        match_obj_map_t::iterator oit = pimpl->match_obj_map.find(e.first);

        if (oit != pimpl->match_obj_map.end()) {
            // there is an existing entry
            FlowEntryPtr& tomod = e.second.back();
            if (oit->second.front().first == objId) {
                // it's for the same object ID.  Replace it.
                updateCookieMap(pimpl->cookie_map,
                                oit->second.front().second->entry->cookie,
                                tomod->entry->cookie,
                                e.first);

                if (!oit->second.front().second->actionEq(tomod.get())) {
                    oit->second.front().second = tomod;
                    diffs.add(FlowEdit::MOD, tomod);
                }
            } else {
                // There are entries from other objects already there.
                // just add/update it in the queue but don't generate
                // diff
                obj_id_flow_vec_t::iterator fvit = oit->second.begin()+1;
                bool found = false;
                bool actionEq = true;
                while (fvit != oit->second.end()) {
                    if (fvit->first == objId) {
                        *fvit = make_pair(objId, tomod);
                        found = true;
                        break;
                    } else if (!fvit->second->actionEq(tomod.get())) {
                        actionEq = false;
                    }
                    ++fvit;
                }
                if (!found) {
                    if (!actionEq) {
                        // it's only a warning if there are duplicate
                        // matches with different actions.
                        LOG(WARNING) << "Duplicate match for "
                                     << objId << " (conflicts with "
                                     << oit->second.front().first << "): "
                                     << *tomod;
                    }

                    oit->second.push_back(make_pair(objId, tomod));
                }
            }
        } else {
            // there is no existing entry.  Add a new one
            FlowEntryPtr& toadd = e.second.back();

            updateCookieMap(pimpl->cookie_map,
                            0, toadd->entry->cookie,
                            e.first);

            pimpl->match_obj_map[e.first].push_back(make_pair(objId, toadd));
            diffs.add(FlowEdit::ADD, toadd);
        }
    }

    // check for deleted entries
    entry_map_t::iterator itr = pimpl->entry_map.find(objId);
    if (itr != pimpl->entry_map.end()) {
        for (match_map_t::value_type& e : itr->second) {
            match_map_t::iterator mit = new_entries.find(e.first);
            if (mit == new_entries.end()) {
                match_obj_map_t::iterator oit =
                    pimpl->match_obj_map.find(e.first);
                if (oit != pimpl->match_obj_map.end()) {
                    if (oit->second.front().first == objId) {
                        // this object is the one in the flow table,
                        // so remove it
                        FlowEntryPtr& todel = oit->second.front().second;

                        if (oit->second.size() == 1) {
                            // No conflicted entries queued
                            updateCookieMap(pimpl->cookie_map,
                                            todel->entry->cookie, 0,
                                            e.first);

                            diffs.add(FlowEdit::DEL, todel);
                            pimpl->match_obj_map.erase(oit);
                        } else {
                            // Need to add the next entry back to the
                            // table now that the first instance is
                            // removed
                            FlowEntryPtr& old = oit->second[0].second;
                            FlowEntryPtr& tomod = oit->second[1].second;

                            updateCookieMap(pimpl->cookie_map,
                                            old->entry->cookie,
                                            tomod->entry->cookie,
                                            e.first);

                            if (!todel->actionEq(tomod.get()))
                                diffs.add(FlowEdit::MOD, tomod);
                            oit->second.erase(oit->second.begin());
                        }
                    } else {
                        // This object is queued behind another
                        // object.  Just remove it without generating
                        // diff.
                        obj_id_flow_vec_t::iterator fvit = oit->second.begin()+1;
                        while (fvit != oit->second.end()) {
                            if (fvit->first == objId)
                                fvit = oit->second.erase(fvit);
                            else
                                ++fvit;
                        }
                    }
                }
            }
        }
    }

    if (diffs.edits.size() > 0) {
        LOG(DEBUG) << "ObjId=" << objId << ", #diffs = " << diffs.edits.size();
        for (const FlowEdit::Entry& e : diffs.edits) {
            LOG(DEBUG) << e;
        }
    }

    /* newEntries.empty() => delete */
    if (new_entries.empty() && itr != pimpl->entry_map.end()) {
        itr->second.swap(new_entries);
        pimpl->entry_map.erase(itr);
    } else {
        if (itr == pimpl->entry_map.end()) {
            pimpl->entry_map[objId].swap(new_entries);
        } else {
            itr->second.swap(new_entries);
        }
    }
}

} // namespace opflexagent
