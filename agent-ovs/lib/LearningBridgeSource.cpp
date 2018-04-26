/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for LearningBridgeSource class.
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/LearningBridgeSource.h>
#include <opflexagent/LearningBridgeManager.h>

namespace opflexagent {

LearningBridgeSource::LearningBridgeSource(LearningBridgeManager* manager_)
    : manager(manager_) {}

LearningBridgeSource::~LearningBridgeSource() {}

void LearningBridgeSource::
updateLBIface(const LearningBridgeIface& learningBridgeIface) {
    manager->updateLBIface(learningBridgeIface);
}

void LearningBridgeSource::removeLBIface(const std::string& uuid) {
    manager->removeLBIface(uuid);
}

} /* namespace opflexagent */
