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

#include <stdint.h>

namespace ovsagent {
namespace flowutils {

/**
 * Add a match against source/destination group ID in REG0 and REG2
 *
 * @param fe the flow entry
 * @param prio the priority
 * @param svnid the source group ID, or zero for any
 * @param dvnide the dest group ID, or zero for any
 */
void set_match_group(FlowEntry& fe, uint16_t prio,
                     uint32_t svnid, uint32_t dvnid);

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
 * Create flow entries for the classifier specified and append them
 * to the provided list.
 *
 * @param classifier Classifier object to get matching rules from
 * @param allow true if the traffic should be allowed, false otherwise
 * @param nextTable the table to send to if the traffic is allowed
 * @param priority Priority of the entry created
 * @param cookie Cookie of the entry created
 * @param svnid VNID of the source endpoint group for the entry
 * @param dvnid VNID of the destination endpoint group for the entry
 * @param entries List to append entry to
 */

void add_classifier_entries(modelgbp::gbpe::L24Classifier *clsfr, bool allow,
                            uint8_t nextTable, uint16_t priority,
                            uint64_t cookie, uint32_t svnid, uint32_t dvnid,
                            /* out */ FlowEntryList& entries);
} // namespace flowutils
} // namespace ovsagent

#endif /* OVSAGENT_FLOWUTILS_H */
