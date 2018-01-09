/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TEST_MOCKFLOWEXECUTOR_H_
#define OPFLEXAGENT_TEST_MOCKFLOWEXECUTOR_H_

#include "FlowExecutor.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_set>

namespace opflexagent {

/**
 * Mock flow executor for tests
 */
class MockFlowExecutor : public FlowExecutor {
public:
    MockFlowExecutor();
    virtual ~MockFlowExecutor() {}

    virtual bool Execute(const FlowEdit& flowEdits);
    virtual bool Execute(const GroupEdit& groupEdits);
    virtual void Expect(FlowEdit::type mod, const std::string& fe);
    virtual void Expect(FlowEdit::type mod, const std::vector<std::string>& fe);
    virtual void ExpectGroup(FlowEdit::type mod, const std::string& ge);
    virtual void IgnoreFlowMods();
    virtual void IgnoreGroupMods();
    virtual bool IsEmpty();
    virtual bool IsGroupEmpty();
    virtual void Clear();

    typedef std::pair<FlowEdit::type, std::string> mod_t;
    std::list<mod_t> flowMods;
    std::list<std::string> groupMods;
    bool ignoreFlowMods;
    std::unordered_set<int> ignoredFlowMods;
    bool ignoreGroupMods;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MOCKPORTMAPPER_H_ */
