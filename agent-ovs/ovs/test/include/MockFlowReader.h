/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TEST_MOCKFLOWREADER_H_
#define OPFLEXAGENT_TEST_MOCKFLOWREADER_H_

#include "FlowReader.h"

namespace opflexagent {

/**
 * Mock flow reader object useful for tests
 */
class MockFlowReader : public FlowReader {
public:
    MockFlowReader() {}

    virtual bool getFlows(uint8_t tableId, const FlowReader::FlowCb& cb) {
        FlowEntryList res;
        for (size_t i = 0; i < flows.size(); ++i) {
            if (flows[i]->entry->table_id == tableId) {
                res.push_back(flows[i]);
            }
        }
        cb(res, true);
        return true;
    }
    virtual bool getGroups(const FlowReader::GroupCb& cb) {
        cb(groups, true);
        return true;
    }

    FlowEntryList flows;
    GroupEdit::EntryList groups;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MOCKFLOWREADER_ */
