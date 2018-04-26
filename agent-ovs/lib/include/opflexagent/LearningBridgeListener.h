/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for learning bridge listener
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_LEARNINGBRIDGELISTENER_H
#define OPFLEXAGENT_LEARNINGBRIDGELISTENER_H

#include <opflexagent/LearningBridgeIface.h>

namespace opflexagent {

/**
 * An abstract interface for classes interested in updates related to
 * the learning bridges
 */
class LearningBridgeListener {
public:
    /**
     * Instantiate a new learning bridge listener
     */
    LearningBridgeListener() {};

    /**
     * Destroy the learning bridge listener and clean up all state
     */
    virtual ~LearningBridgeListener() {};

    /**
     * Called when a learning bridge interface is added, updated, or
     * removed.
     *
     * @param uuid the UUID for the learning bridge interface
     */
    virtual void lbIfaceUpdated(const std::string& uuid) {};

    /**
     * Called when a learning bridge VLAN is added, updated, or
     * removed.
     *
     * @param vlan the VLANs that have been updated
     */
    virtual void lbVlanUpdated(LearningBridgeIface::vlan_range_t vlan) {};
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_LEARNINGBRIDGELISTENER_H */
