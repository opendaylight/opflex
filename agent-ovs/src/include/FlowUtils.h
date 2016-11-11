/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Utility functions for flow tables
 *
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_FLOWUTILS_H
#define OVSAGENT_FLOWUTILS_H

#include "TableState.h"

#include <modelgbp/gbpe/L24Classifier.hpp>

#include <boost/optional.hpp>

#include <stdint.h>

namespace ovsagent {
namespace flowutils {

/**
 * Type representing a subnet IP addres string and prefix length
 */
typedef std::pair<std::string, uint8_t> subnet_t;

} // namespace flowutils
} // namespace ovsagent

namespace std {

/**
 * Template specialization for std::hash<subnet_t>, making
 * it suitable as a key in a std::unordered_map
 */
template<> struct hash<ovsagent::flowutils::subnet_t> {
    /**
     * Hash the subnet_t
     */
    std::size_t operator()(const ovsagent::flowutils::subnet_t& u) const;
};

} /* namespace std */

namespace ovsagent {

class FlowBuilder;
class ActionBuilder;

namespace flowutils {

/**
 * A set of subnets
 */
typedef std::unordered_set<subnet_t> subnets_t;

/**
 * subnet_t stream insertion
 */
std::ostream& operator<<(std::ostream &os, const subnet_t& subnet);

/**
 * subnets_t stream insertion
 */
std::ostream& operator<<(std::ostream &os, const subnets_t& subnets);

/**
 * Add a match against source/destination group ID in REG0 and REG2
 *
 * @param f the flow builder
 * @param prio the priority
 * @param svnid the source group ID, or zero for any
 * @param dvnide the dest group ID, or zero for any
 */
void match_group(FlowBuilder& f, uint16_t prio, uint32_t svnid, uint32_t dvnid);

/**
 * Get a flow entry for the output table that will output to the port
 * in REG7
 *
 * @return the new flow entry
 */
FlowEntryPtr default_out_flow();

/**
 * Maximum flow priority of the entries in policy table.
 */
extern const uint16_t MAX_POLICY_RULE_PRIORITY;

/**
 * Actions to take for classifier entries in add_classifier_entries
 */
enum ClassAction {
    /**
     * Drop the flow
     */
    CA_DENY,
    /**
     * Allow the flow by going to nextTable
     */
    CA_ALLOW,
    /**
     * Allow the flow by going to nextTable and committing a
     * connection tracking flow
     */
    CA_REFLEX_FWD,
    /**
     * Match against empty conntrack state and send to conntrack table,
     * recirculating the flow to nextTable
     */
    CA_REFLEX_REV,
    /**
     * Match against established conntrack state and go to nextTable
     */
    CA_REFLEX_REV_ALLOW,
};

/**
 * Create flow entries for the classifier specified and append them
 * to the provided list.
 *
 * @param classifier Classifier object to get matching rules from
 * @param act an action to take for the flows
 * @param sourceSub A set of source networks to which the rule should apply
 * @param destSub A set of dest networks to which the rule should apply
 * @param nextTable the table to send to if the traffic is allowed
 * @param priority Priority of the entry created
 * @param cookie Cookie of the entry created
 * @param svnid VNID of the source endpoint group for the entry
 * @param dvnid VNID of the destination endpoint group for the entry
 * @param entries List to append entry to
 */
void add_classifier_entries(modelgbp::gbpe::L24Classifier& clsfr,
                            ClassAction act,
                            boost::optional<const subnets_t&> sourceSub,
                            boost::optional<const subnets_t&> destSub,
                            uint8_t nextTable, uint16_t priority,
                            uint64_t cookie, uint32_t svnid, uint32_t dvnid,
                            /* out */ FlowEntryList& entries);
} // namespace flowutils
} // namespace ovsagent

#endif /* OVSAGENT_FLOWUTILS_H */
