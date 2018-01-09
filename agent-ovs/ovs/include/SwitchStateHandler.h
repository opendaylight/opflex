/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for switch state listener interface
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SWITCHSTATEHANDLER_H
#define OPFLEXAGENT_SWITCHSTATEHANDLER_H

#include "TableState.h"

#include <boost/noncopyable.hpp>

#include <vector>
#include <unordered_map>

namespace opflexagent {

class SwitchManager;

/**
 * Abstract base class for switch state handler that handles
 * reconciling flow and group tables in response to switch connection
 * state.
 */
class SwitchStateHandler {
public:
    /**
     * Destroy the switch state listener
     */
    virtual ~SwitchStateHandler() {};

    /**
     * Compare flows read from switch and make modification to eliminate
     * differences.
     *
     * @param flowTables the current flow table state
     * @param recvFlows the flows received from the switch to
     * reconcile against, with a flow entry list per table.  It is
     * safe to modify this vector.
     * @return the necessary edits to reconcile the flows
     */
    virtual std::vector<FlowEdit>
    reconcileFlows(std::vector<TableState> flowTables,
                   std::vector<FlowEntryList>& recvFlows);

    /**
     * A map from a group table ID to an associated group edit
     */
    typedef std::unordered_map<uint32_t, GroupEdit::Entry> GroupMap;

    /**
     * Compare group tables read from switch and make modification to
     * eliminate differences.
     *
     * @param recvGroups the groups from the switch to reconcile
     * against.  It is safe to modify this map.
     * @return the necessary edits to reconcile the groups
     */
    virtual GroupEdit reconcileGroups(GroupMap& recvGroups);

    /**
     * Called when the state sync process completes
     */
    virtual void completeSync() {}

};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SWITCHSTATEHANDLER_H */
