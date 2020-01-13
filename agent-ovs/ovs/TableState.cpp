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
#include <openvswitch/ofp-flow.h>
#include <openvswitch/list.h>
#include <openflow/openflow-common.h>

extern "C" {
#include <openvswitch/dynamic-string.h>
}


namespace opflexagent {

struct match_key_t {
    uint16_t prio;
    struct match match;
};

struct tlv_key_t {
     uint16_t option_class;
     uint16_t option_type;
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

namespace std {

template<> struct hash<opflexagent::tlv_key_t> {
    size_t operator()(const opflexagent::tlv_key_t& tlv_key) const noexcept {
        size_t hashv = 0;
        boost::hash_combine(hashv, tlv_key.option_class);
        boost::hash_combine(hashv, tlv_key.option_type);
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
    ofputil_flow_stats_format(str.get(), (ofputil_flow_stats*)&fs,
                              NULL, NULL, true);
    os << (const char*)(ds_cstr(str.get())); // trim space
    return os;
}

ostream & operator<<(ostream& os, const match& m) {
    DsP str;
    match_format(&m, NULL, str.get(), OFP_DEFAULT_PRIORITY);
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
typedef std::vector<TlvEntryPtr> tlv_vec_t;
typedef std::pair<std::string, TlvEntryPtr> obj_id_tlv_t;
typedef std::vector<obj_id_tlv_t> obj_id_tlv_vec_t;
typedef std::unordered_map<tlv_key_t, tlv_vec_t> match_tlv_opt_map_t;
typedef std::unordered_map<std::string, match_tlv_opt_map_t> tlv_entry_map_t;
typedef std::unordered_map<tlv_key_t, obj_id_tlv_vec_t> match_obj_tlv_map_t;

bool operator==(const match_key_t& lhs, const match_key_t& rhs) {
    return lhs.prio == rhs.prio && match_equal(&lhs.match, &rhs.match);
}
bool operator!=(const match_key_t& lhs, const match_key_t& rhs) {
    return !(lhs == rhs);
}

bool operator==(const tlv_key_t& lhs, const tlv_key_t& rhs) {
    return ((lhs.option_class == rhs.option_class) &&
    (lhs.option_type == rhs.option_type));
}
bool operator!=(const tlv_key_t& lhs, const tlv_key_t& rhs) {
    return !(lhs == rhs);
}

class TableState::TableStateImpl {
public:
    entry_map_t entry_map;
    match_obj_map_t match_obj_map;
    cookie_map_t cookie_map;
    tlv_entry_map_t tlv_entry_map;
    match_obj_tlv_map_t match_obj_tlv_map;
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

void TableState::diffSnapshot(const TlvEntryList& oldEntries,
                              TlvEdit& diffs) const {
    typedef std::pair<bool, TlvEntryPtr> visited_te_t;
    typedef std::unordered_map<tlv_key_t, visited_te_t> old_entry_map_t;

    diffs.edits.clear();

    old_entry_map_t old_entries;
    for (const TlvEntryPtr& te : oldEntries) {
        tlv_key_t key;
        key.option_class = te->entry->option_class;
        key.option_type = te->entry->option_type;
        old_entries[key] = make_pair(false, te);
    }

    // Add/mod any matches in the object map
    for (match_obj_tlv_map_t::value_type& e : pimpl->match_obj_tlv_map) {
        old_entry_map_t::iterator it = old_entries.find(e.first);
        if (it == old_entries.end()) {
            diffs.add(TlvEdit::ADD, e.second.front().second);
        } else {
            it->second.first = true;
            TlvEntryPtr& olde = it->second.second;
            TlvEntryPtr& newe = e.second.front().second;
            if (!olde->tlvEq(*newe)) {
                diffs.add(TlvEdit::DEL, olde);
                diffs.add(TlvEdit::ADD, newe);
            }
        }
    }

    // Remove unvisited entries from the old entry list
    for (old_entry_map_t::value_type& e : old_entries) {
        if (e.second.first) continue;
        diffs.add(TlvEdit::DEL, e.second.second);
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

TlvEntry::TlvEntry(struct ofputil_tlv_map *tlv_map) {
    entry = (ofputil_tlv_map *)calloc(1, sizeof(struct ofputil_tlv_map));
    entry->option_class = tlv_map->option_class;
    entry->option_type = tlv_map->option_type;
    entry->option_len = tlv_map->option_len;
    entry->index = tlv_map->index;
}

TlvEntry::TlvEntry() {
    entry = (ofputil_tlv_map *)calloc(1, sizeof(struct ofputil_tlv_map));
}

TlvEntry::~TlvEntry() {
    if (entry) {
        free(entry);
    }
    entry = NULL;
}

bool TlvEntry::tlvEq(const TlvEntry& rhs) {
    return (
    (entry->option_class == rhs.entry->option_class) &&
    (entry->option_type == rhs.entry->option_type) &&
    (entry->option_len == rhs.entry->option_len) &&
    (entry->index == rhs.entry->index));
}

ostream & operator<<(ostream& os, const TlvEntryPtr& te) {
    os << "{class=" << (uint32_t)te->entry->option_class << ",type="
       << (uint32_t)te->entry->option_type << ",len="
       << (uint32_t)te->entry->option_len << "}->tun_metadata"
       << (uint32_t)te->entry->index;
    return os;
}

ostream & operator<<(ostream& os, const struct ofputil_tlv_map& fs) {
    DsP str;
    print_tlv_map(str.get(), &fs);
    os << (const char*)(ds_cstr(str.get())); // trim space
    return os;
}

ostream & operator<<(ostream& os, const TlvEdit::Entry& te) {
    std::vector<std::string> op{"ADD", "DEL", "CLR"};
    os << op[te.first] << "|" << te.second;
    return os;
}

/** TlvEdit **/
void TlvEdit::add(TlvEdit::type t, TlvEntryPtr te) {
    edits.push_back(std::make_pair(t, te));
}

void TableState::apply(const std::string& objId,
                       TlvEntryList& newEntries,
                       /* out */ TlvEdit& diffs) {
    diffs.edits.clear();

    match_tlv_opt_map_t new_entries;
    for (const TlvEntryPtr& te : newEntries) {

        tlv_key_t key;
        key.option_class = te->entry->option_class;
        key.option_type = te->entry->option_type;
        new_entries[key].push_back(te);
    }

    // load new entries
    for (match_tlv_opt_map_t::value_type& e : new_entries) {
        // check if there's an overlapping match already in the table
        match_obj_tlv_map_t::iterator oit =
                pimpl->match_obj_tlv_map.find(e.first);

        if (oit != pimpl->match_obj_tlv_map.end()) {
            // there is an existing entry
            TlvEntryPtr& tomod = e.second.back();
            if (oit->second.front().first == objId) {

                if ((oit->second.front().second->entry->option_len !=
                        e.second.back()->entry->option_len) ||
                        (oit->second.front().second->entry->index !=
                                e.second.back()->entry->index)) {
                    diffs.add(TlvEdit::DEL, oit->second.front().second);
                    oit->second.front().second = tomod;
                    diffs.add(TlvEdit::ADD, tomod);
                }
            } else {
                // There are entries from other objects already there.
                // just add/update it in the queue but don't generate
                // diff
                obj_id_tlv_vec_t::iterator fvit = oit->second.begin()+1;
                bool found = false;
                bool actionEq = true;
                while (fvit != oit->second.end()) {
                    if (fvit->first == objId) {
                        *fvit = make_pair(objId, tomod);
                        found = true;
                        break;
                    } else if ((fvit->second->entry->option_len !=
                            tomod->entry->option_len) ||
                            (fvit->second->entry->index !=
                                    tomod->entry->index)) {
                        actionEq = false;
                    }
                    ++fvit;
                }
                if (!found) {
                    if (!actionEq) {
                        // it's only a warning if there are duplicate
                        // matches with different actions.
                        LOG(WARNING) << "Duplicate TLV for "
                                     << objId << " (conflicts with "
                                     << oit->second.front().first << "): "
                                     << tomod;
                    }

                    oit->second.push_back(make_pair(objId, tomod));
                }
            }
        } else {
            // there is no existing entry.  Add a new one
            TlvEntryPtr& toadd = e.second.back();

            pimpl->match_obj_tlv_map[e.first].push_back(make_pair(objId, toadd));
            diffs.add(TlvEdit::ADD, toadd);
        }
    }

    // check for deleted entries
    tlv_entry_map_t::iterator itr = pimpl->tlv_entry_map.find(objId);
    if (itr != pimpl->tlv_entry_map.end()) {
        for (match_tlv_opt_map_t::value_type& e : itr->second) {
            match_tlv_opt_map_t::iterator mit = new_entries.find(e.first);
            if (mit == new_entries.end()) {
                match_obj_tlv_map_t::iterator oit =
                    pimpl->match_obj_tlv_map.find(e.first);
                if (oit != pimpl->match_obj_tlv_map.end()) {
                    if (oit->second.front().first == objId) {
                        // this object is the one in the flow table,
                        // so remove it
                        TlvEntryPtr& todel = oit->second.front().second;

                        if (oit->second.size() == 1) {
                            diffs.add(TlvEdit::DEL, todel);
                            pimpl->match_obj_tlv_map.erase(oit);
                        } else {
                            // Need to add the next entry back to the
                            // table now that the first instance is
                            // removed
                            TlvEntryPtr& tomod = oit->second[1].second;


                            if ((todel->entry->option_len != tomod->entry->option_len)
                                ||(todel->entry->index != tomod->entry->index)) {
                                diffs.add(TlvEdit::DEL, todel);
                                diffs.add(TlvEdit::ADD, tomod);
                            }
                            oit->second.erase(oit->second.begin());
                        }
                    } else {
                        // This object is queued behind another
                        // object.  Just remove it without generating
                        // diff.
                        obj_id_tlv_vec_t::iterator fvit = oit->second.begin()+1;
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
        for (const TlvEdit::Entry& e : diffs.edits) {
            LOG(DEBUG) << e;
        }
    }

    /* newEntries.empty() => delete */
    if (new_entries.empty() && itr != pimpl->tlv_entry_map.end()) {
        itr->second.swap(new_entries);
        pimpl->tlv_entry_map.erase(itr);
    } else {
        if (itr == pimpl->tlv_entry_map.end()) {
            pimpl->tlv_entry_map[objId].swap(new_entries);
        } else {
            itr->second.swap(new_entries);
        }
    }
}

} // namespace opflexagent
