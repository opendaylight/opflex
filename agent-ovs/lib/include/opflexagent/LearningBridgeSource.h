/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for learning bridge source
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/LearningBridgeIface.h>

#pragma once
#ifndef OPFLEXAGENT_LEARNINGBRIDGESOURCE_H
#define OPFLEXAGENT_LEARNINGBRIDGESOURCE_H

namespace opflexagent {

class LearningBridgeManager;

/**
 * An abstract base class for a source of learning bridge data.  A
 * component that discovers learning bridges on the system can
 * implement this interface to inform the learning bridge manager when
 * learning bridges are created or modified.
 */
class LearningBridgeSource {
public:
    /**
     * Instantiate a new learning bridge source using the learning
     * bridge manager specified
     */
    LearningBridgeSource(LearningBridgeManager* manager);

    /**
     * Destroy the learning bridge source and clean up all state
     */
    virtual ~LearningBridgeSource();

    /**
     * Add or update the specified learning bridge interface in the
     * learning bridge manager.
     *
     * @param learningBridgeIface the learning bridge interface to
     * add/update
     */
    virtual void updateLBIface(const LearningBridgeIface& learningBridgeIface);

    /**
     * Remove an learning bridge interface that no longer exists from
     * the learning bridge manager
     *
     * @param uuid the learning bridge that no longer exists
     */
    virtual void removeLBIface(const std::string& uuid);

protected:
    /**
     * The learning bridge manager that will be updated
     */
    LearningBridgeManager* manager;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_LEARNINGBRIDGESOURCE_H */
