/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_FLOW_TABLESTATE_H_
#define _OPFLEX_ENFORCER_FLOW_TABLESTATE_H_

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>

struct ofputil_flow_stats;

namespace opflex {
namespace enforcer {
namespace flow {


/**
 * Class representing an entry in a flow table.
 */
class FlowEntry {
public:
    FlowEntry();
    ~FlowEntry();

    bool MatchEq(const FlowEntry *rhs);
    bool ActionEq(const FlowEntry *rhs);

    ofputil_flow_stats *entry;
};
typedef boost::shared_ptr<FlowEntry>    FlowEntryPtr;
typedef std::vector<FlowEntryPtr>       FlowEntryList;

/**
 * Print flow entry to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const FlowEntry& fe);

/**
 * Print list of flow entries to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const FlowEntryList& el);

/**
 * Class that represents a list of table changes.
 */
class FlowEdit {
public:
    enum TYPE {add, mod, del};
    typedef std::pair<TYPE, FlowEntryPtr> Entry;

    std::vector<FlowEdit::Entry> edits;
};

/**
 * Print a flow edit to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const FlowEdit::Entry& fe);

/**
 * Print a list of flow edits to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const FlowEdit& fe);

/**
 * Class that represents a list of group table changes.
 */
class GroupEdit {
public:
    /**
     * Class that represents change to a group-table entry.
     */
    class GroupMod {
    public:
        GroupMod();
        ~GroupMod();

        ofputil_group_mod *mod;
    };
    typedef boost::shared_ptr<GroupMod>     Entry;
    typedef std::vector<GroupEdit::Entry>   EntryList;

    GroupEdit::EntryList edits;
};

/**
 * Print a group-table change to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const GroupEdit::Entry& ge);

/**
 * Class that maintains a cached version of an OpenFlow table.
 */
class TableState {
public:
    TableState() {}

    /**
     * Update cached entry-list corresponding to given object-id
     */
    void Update(const std::string& objId, FlowEntryList& el);

    /**
     * Compute the differences between existing and provided table-entries set
     * for given object-id.
     */
    void DiffEntry(const std::string& objId, const FlowEntryList& newEntries,
            FlowEdit& diffs) const;

    /**
     * Compute the differences between provided table-entries and all the
     * entries currently in the table.
     *
     * @param oldEntries the entries to compare against
     * @param diffs the differences between provided entries and the table
     */
    void DiffSnapshot(const FlowEntryList& oldEntries, FlowEdit& diffs) const;
private:
    /**
     * Compute flow-entries additions and modification between old and new
     * entries.
     *
     * @param oldEntries The old entries
     * @param newEntries The new entries
     * @param visited Boolean vector corresponding to old entries that is set
     * to true if the corresponding entry is also present in the new entries
     * @param diffs the differences between old and new entries
     */
    static void CalculateAddMod(const FlowEntryList& oldEntries,
        const FlowEntryList& newEntries,
        std::vector<bool>& visited,
        FlowEdit& diffs);

    /**
     * Compute which old entries need to be deleted.
     *
     * @param oldEntries The old entries
     * @param visited Boolean vector corresponding to old entries that should be
     * set to true if the corresponding entry should not be deleted
     * @param diffs Container to hold the deletions
     */
    static void CalculateDel(const FlowEntryList& oldEntries,
        std::vector<bool>& visited,
        FlowEdit& diffs);

    typedef boost::unordered_map<std::string, FlowEntryList> EntryMap;
    EntryMap entryMap;
};



}   // namespace flow
}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOW_TABLESTATE_H_
