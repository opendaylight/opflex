/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AccessFlowManager.h"

namespace ovsagent {

AccessFlowManager::AccessFlowManager(Agent& agent_,
                                     SwitchManager& switchManager_,
                                     IdGenerator& idGen_)
    : agent(agent_), switchManager(switchManager_), idGen(idGen_),
      taskQueue(agent.getAgentIOService()) {

}

void AccessFlowManager::start() {

}

void AccessFlowManager::stop() {

}

void AccessFlowManager::endpointUpdated(const std::string& uuid) {

}

} // namespace ovsagent
