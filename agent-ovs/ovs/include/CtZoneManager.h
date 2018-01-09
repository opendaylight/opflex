/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Definition of CtZoneManager class
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_CTZONEMANAGER_H_
#define OPFLEXAGENT_CTZONEMANAGER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <stdint.h>
#include <boost/noncopyable.hpp>

namespace opflexagent {

class IdGenerator;

/**
 * Manage IDs associated with connection tracking zones in the linux
 * kernel.  When an ID is allocated, we ensure that no stale
 * connection state exists for that zone in the connection tracking
 * table.
 */
class CtZoneManager : private boost::noncopyable {
public:
    /**
     * Allocate a new CtZoneManager
     *
     * @param gen the IdGenerator to use
     * the IdGenerator
     */
    CtZoneManager(IdGenerator& gen);
    ~CtZoneManager();

    /**
     * Set the range of connection tracking zone IDs to use
     * @param min the minimum ID to use (min 1)
     * @param max the maximum ID to use (max 65534)
     */
    void setCtZoneRange(uint16_t min, uint16_t max);

    /**
     * Enable or disable netlink
     * @param useNetLink if true, manage zones in the kernel using
     * netlink.  Otherwise just allocate the zones and don't manage
     * their state.
     */
    void enableNetLink(bool useNetLink = true);

    /**
     * Initialize the CtZoneManager
     * @param nmspc the name of the namespace to use for ct zones in
     */
    virtual void init(const std::string& nmspc);

    /**
     * Allocate a connection tracking zone for the given ID string.
     *
     * @param str the zone ID string
     * @return the zone ID
     */
    uint16_t getId(const std::string& str);

    /**
     * Erase a connection tracking zone
     * @param str the zone ID string
     */
    void erase(const std::string& str);

private:
    uint16_t minId;
    uint16_t maxId;
    bool useNetLink;

    IdGenerator& gen;
    std::string nmspc;

    bool ctZoneAllocHook(const std::string&, uint32_t);
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_CTZONEMANAGER_H_ */
