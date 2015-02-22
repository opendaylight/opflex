/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_TABLESTATE_H_
#define OVSAGENT_TABLESTATE_H_

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>

struct ofputil_flow_stats;

namespace ovsagent {

/**
 * Class representing an entry in a flow table.
 */
class FlowEntry : private boost::noncopyable {
public:
    FlowEntry();
    ~FlowEntry();

    /**
     * Check whether the given flow entry is equal to this one
     * @param rhs the entry to compare against
     * @return true if the match entry is equal
     */
    bool MatchEq(const FlowEntry *rhs);

    /**
     * Check whether the given action entry is equal to this one
     * @param rhs the entry to compare against
     * @return true if the action entry is equal
     */
    bool ActionEq(const FlowEntry *rhs);

    /**
     * The flow entry
     */
    ofputil_flow_stats *entry;
};
/**
 * A shared pointer to a flow entry
 */
typedef boost::shared_ptr<FlowEntry>    FlowEntryPtr;
/**
 * A vector of flow entry pointers
 */
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
    /**
     * The type of edit to make
     */
    enum TYPE {
        /**
         * Add new flows
         */
        add,
        /**
         * Modify existing flows
         */
        mod,
        /**
         * Delete flows
         */
        del
    };

    /**
     * An edit entry is a flow entry pairs with the operation to
     * perform
     */
    typedef std::pair<TYPE, FlowEntryPtr> Entry;

    /**
     * The edits that need to be made
     */
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

        /**
         * The group mod
         */
        ofputil_group_mod *mod;
    };
    /**
     * A group edit entry
     */
    typedef boost::shared_ptr<GroupMod>     Entry;
    /**
     * A vector of group edits
     */
    typedef std::vector<GroupEdit::Entry>   EntryList;

    /**
     * Check if two groups are the same excluding the group
     * modification command.
     *
     * @param lhs One of the groups to compare
     * @param rhs The other group to compare
     * @return true if groups are equal
     */
    static bool GroupEq(const GroupEdit::Entry& lhs,
            const GroupEdit::Entry& rhs);

    /**
     * The group edits that need to be made
     */
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

} // namespace ovsagent

#endif // OVSAGENT_TABLESTATE_H_
