/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "SwitchStateHandler.h"
#include "SwitchManager.h"
#include "logging.h"

#include <boost/foreach.hpp>

namespace ovsagent {

std::vector<FlowEdit>
SwitchStateHandler::reconcileFlows(std::vector<TableState> flowTables,
                                   std::vector<FlowEntryList>& recvFlows) {
    std::vector<FlowEdit> diffs(flowTables.size());
    for (size_t i = 0; i < flowTables.size(); ++i) {
        flowTables[i].DiffSnapshot(recvFlows[i], diffs[i]);
        LOG(DEBUG) << "Table=" << i << ", snapshot has "
                   << diffs[i].edits.size() << " diff(s)";
        BOOST_FOREACH(const FlowEdit::Entry& e, diffs[i].edits) {
            LOG(DEBUG) << e;
        }
    }

    return diffs;
}

GroupEdit SwitchStateHandler::reconcileGroups(GroupMap& recvGroups) {

}

} // namespace ovsagent
