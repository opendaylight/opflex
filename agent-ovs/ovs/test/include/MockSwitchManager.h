/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_TEST_MOCKSWITCHMANAGER_H_
#define OPFLEXAGENT_TEST_MOCKSWITCHMANAGER_H_

#include "SwitchManager.h"
#include "MockSwitchConnection.h"
#include "MockPortMapper.h"
#include "MockFlowExecutor.h"
#include "MockFlowReader.h"

namespace opflexagent {

/**
 * Mock switch manager object useful for tests
 */
class MockSwitchManager : public SwitchManager {
public:
    MockSwitchManager(Agent& agent,
                      FlowExecutor& flowExecutor,
                      FlowReader& flowReader,
                      PortMapper& portMapper) :
        SwitchManager(agent, flowExecutor, flowReader, portMapper) {}

    virtual void start(const std::string& swName) {
        connection.reset(new MockSwitchConnection());
    }
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MOCKSWITCHMANAGER_H_ */
