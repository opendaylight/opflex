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

#include <opflex/modb/URI.h>

struct ofputil_flow_stats;

namespace opflex {
namespace enforcer {
namespace flow {

/**
 * Base class for an Entry in OpenFlow table.
 */
class Entry {
public:
	/**
	 * Return true if "match" part of entry are equal
	 */
    virtual bool MatchEq(const Entry *) = 0;

    /**
     * Return true if "action" part of entry are equal
     */
    virtual bool ActionEq(const Entry *) = 0;
};
typedef std::vector<Entry *>    EntryList;

/**
 * Class representing an entry in a flow table.
 */
class FlowEntry : public Entry {
public:
    FlowEntry();
    ~FlowEntry();

    /* Interface: Entry */
    bool MatchEq(const Entry *rhs);
    bool ActionEq(const Entry *rhs);

    ofputil_flow_stats *entry;
};

/**
 * Class that represents a list of table changes.
 */
class FlowEdit {
public:
    enum TYPE {add, mod, del};
    typedef std::pair<TYPE, Entry *> EntryEdit;

    std::vector<EntryEdit> edits;
};


/**
 * Class that maintains a cached version of an OpenFlow table.
 */
class TableState {
public:
    enum TABLE_TYPE {PORT_SECURITY, SOURCE_FILTER, DESTINATION_FILTER,
    	POLICY, GROUP, METER};
    TableState(TABLE_TYPE tt) : type(tt) {};

    /**
     * Update cached entry corresponding to given URI
     */
    void Update(const modb::URI& uri, EntryList& el);

    /**
     * Compute the differences between existing and provided table-entries set
     * for a given URI.
     */
    void DiffEntry(const modb::URI& uri, const EntryList& newEntries,
    		FlowEdit& diffs);

    TABLE_TYPE GetType() {
        return type;
    }

private:
    TABLE_TYPE type;
    typedef boost::unordered_map<opflex::modb::URI, EntryList> EntryMap;
    EntryMap entryMap;
};



}   // namespace flow
}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOW_TABLESTATE_H_
