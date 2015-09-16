/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for extra config listener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_EXTRACONFIGLISTENER_H
#define OVSAGENT_EXTRACONFIGLISTENER_H

namespace ovsagent {

/**
 * An abstract interface for classes interested in updates related to
 * the extraConfigs
 */
class ExtraConfigListener {
public:
    /**
     * Instantiate a new extraConfig listener
     */
    ExtraConfigListener() {};

    /**
     * Destroy the extraConfig listener and clean up all state
     */
    virtual ~ExtraConfigListener() {};

    /**
     * Called when a routing domain config object is updated
     *
     * @param domainURI the URI for the associated routing domain
     */
    virtual void rdConfigUpdated(const opflex::modb::URI& domainURI) = 0;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_EXTRACONFIGLISTENER_H */
