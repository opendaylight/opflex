/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for snat source
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Snat.h>

#pragma once
#ifndef OPFLEXAGENT_SNATSOURCE_H
#define OPFLEXAGENT_SNATSOURCE_H

namespace opflexagent {

class SnatManager;

/**
 * An abstract base class for a source of snat data.  A component
 * that discovers snats on the system can implement this interface
 * to inform the snat manager when snats are created or
 * modified.
 */
class SnatSource {
public:
    /**
     * Instantiate a new snat source using the snat manager
     * specified
     */
    SnatSource(SnatManager* manager);

    /**
     * Destroy the snat source and clean up all state
     */
    virtual ~SnatSource();

    /**
     * Add or update the specified snat in the snat manager.
     *
     * @param snat the snat to add/update
     */
    virtual void updateSnat(const Snat& snat);

    /**
     * Remove an snat that no longer exists from the snat
     * manager
     *
     * @param snatIp the snat that no longer exists
     * @param uuid of the snat that no longer exists
     */
    virtual void removeSnat(const std::string& snatIp,
                            const std::string& uuid);

protected:
    /**
     * The snat manager that will be updated
     */
    SnatManager* manager;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SNATSOURCE_H */
