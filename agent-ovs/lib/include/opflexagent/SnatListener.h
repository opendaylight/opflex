/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for snat listener
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SNATLISTENER_H
#define OPFLEXAGENT_SNATLISTENER_H

namespace opflexagent {

/**
 * An abstract interface for classes interested in updates related to
 * the snats
 */
class SnatListener {
public:
    /**
     * Instantiate a new snat listener
     */
    SnatListener() {};

    /**
     * Destroy the snat listener and clean up all state
     */
    virtual ~SnatListener() {};

    /**
     * Called when a snat is added, updated, or removed.
     *
     * @param snatIp the SNAT IP for the snat
     * @param uuid the uuid of the snat object
     */
    virtual void snatUpdated(const std::string& snatIp,
                             const std::string& uuid) = 0;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SNATLISTENER_H */
