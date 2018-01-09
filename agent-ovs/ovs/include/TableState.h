/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TABLESTATE_H_
#define OPFLEXAGENT_TABLESTATE_H_

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <utility>
#include <memory>
#include <functional>
#include <boost/noncopyable.hpp>

struct ofputil_flow_stats;
struct ofputil_group_mod;
struct match;

namespace opflexagent {

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
    bool matchEq(const FlowEntry *rhs);

    /**
     * Check whether the given action entry is equal to this one
     * @param rhs the entry to compare against
     * @return true if the action entry is equal
     */
    bool actionEq(const FlowEntry *rhs);

    /**
     * The flow entry
     */
    struct ofputil_flow_stats* entry;
};
/**
 * A shared pointer to a flow entry
 */
typedef std::shared_ptr<FlowEntry> FlowEntryPtr;
/**
 * A vector of flow entry pointers
 */
typedef std::vector<FlowEntryPtr> FlowEntryList;

/**
 * Print flow entry to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const FlowEntry& fe);

/**
 * Print a flow stats to an output stream
 */
std::ostream& operator<<(std::ostream& os, const struct ofputil_flow_stats& fs);

/**
 * Print a match to an output stream
 */
std::ostream& operator<<(std::ostream& os, const struct match& m);

/**
 * Class that represents a list of table changes.
 */
class FlowEdit {
public:
    /**
     * The type of edit to make
     */
    enum type {
        /**
         * Add new flows
         */
        ADD,
        /**
         * Modify existing flows
         */
        MOD,
        /**
         * Delete flows
         */
        DEL
    };

    /**
     * An edit entry is a flow entry pairs with the operation to
     * perform
     */
    typedef std::pair<type, FlowEntryPtr> Entry;

    /**
     * The edits that need to be made
     */
    std::vector<FlowEdit::Entry> edits;

    /**
     * Add a new edit
     * @param t the type of the edit
     * @param fe the flow entry associated with the edit
     */
    void add(type t, FlowEntryPtr fe);

};

/**
 * Print a flow edit to an output stream.
 */
std::ostream& operator<<(std::ostream& os, const FlowEdit::Entry& fe);

/**
 * Compare two flow edit entries
 */
bool operator<(const FlowEdit::Entry& f1, const FlowEdit::Entry& f2);

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
        struct ofputil_group_mod* mod;
    };
    /**
     * A group edit entry
     */
    typedef std::shared_ptr<GroupMod>     Entry;
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
    static bool groupEq(const GroupEdit::Entry& lhs,
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
    TableState();
    /**
     * Copy constructor
     * @param ts the object to copy from
     */
    TableState(const TableState& ts);
    ~TableState();

    /**
     * Update cached entry-list corresponding to given object-id
     */
    void apply(const std::string& objId,
               FlowEntryList& el,
               /* out */ FlowEdit& diffs);

    /**
     * Compute the differences between provided table-entries and all the
     * entries currently in the table.
     *
     * @param oldEntries the entries to compare against
     * @param diffs the differences between provided entries and the table
     */
    void diffSnapshot(const FlowEntryList& oldEntries, FlowEdit& diffs) const;

    /**
     * A callback that can be passed to forEachCookieMatch.
     * Parameters are the cookie value, the match priority, and the
     * match.
     */
    typedef std::function<void (uint64_t, uint16_t, const struct match&)>
    cookie_callback_t;

    /**
     * Call the callback synchronously for each unique cookie and flow
     * table match in the flow table.
     *
     * @param cb the callback to call
     */
    void forEachCookieMatch(cookie_callback_t& cb) const;

private:
    class TableStateImpl;
    TableStateImpl* pimpl;
    friend class TableStateImpl;

};

} // namespace opflexagent

#endif // OPFLEXAGENT_TABLESTATE_H_
