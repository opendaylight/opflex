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
typedef std::vector<FlowEntry *>    FlowEntryList;

/**
 * Print flow entry to an output stream.
 */
std::ostream& operator<<(std::ostream &os, const FlowEntry& fe);

/**
 * Print list of flow entries to an output stream.
 */
std::ostream& operator<<(std::ostream &os, const FlowEntryList& el);

/**
 * Class that represents a list of table changes.
 */
class FlowEdit {
public:
    enum TYPE {add, mod, del};
    typedef std::pair<TYPE, FlowEntry *> Entry;

    std::vector<FlowEdit::Entry> edits;
};

/**
 * Print a flow edit to an output stream.
 */
std::ostream& operator<<(std::ostream &os, const FlowEdit::Entry& fe);

/**
 * Print a list of flow edits to an output stream.
 */
std::ostream& operator<<(std::ostream &os, const FlowEdit& fe);

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
            FlowEdit& diffs);

private:
    typedef boost::unordered_map<std::string, FlowEntryList> EntryMap;
    EntryMap entryMap;
};



}   // namespace flow
}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOW_TABLESTATE_H_
