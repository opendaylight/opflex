/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for snat manager
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SNATMANAGER_H
#define OPFLEXAGENT_SNATMANAGER_H

#include <opflexagent/Snat.h>
#include <opflexagent/SnatListener.h>

#include <boost/noncopyable.hpp>
#include <opflex/modb/URI.h>

#include <unordered_set>
#include <unordered_map>
#include <mutex>

namespace opflexagent {

/**
 * Manage snat endpoints
 */
class SnatManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new snat manager
     */
    SnatManager() {};

    /**
     * Register a listener for snat change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see SnatListener
     */
    void registerListener(SnatListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(SnatListener* listener);

    /**
     * Get the detailed information for an snat
     *
     * @param snatIp, the snat ip address
     * @return the object containing the detailed information, or a
     * NULL pointer if there is no such snat
     */
    std::shared_ptr<const Snat>
        getSnat(const std::string& snatIp);

private:
    /**
     * Add or update the snat state with new information about an
     * snat.
     */
    void updateSnat(const Snat& snat);

    /**
     * Remove the snat with the specified snatIp from the snat
     * manager.
     *
     * @param snatIp of the snat that no longer exists
     */
    void removeSnat(const std::string& snatIp);

    class SnatState {
    public:
        std::shared_ptr<const Snat> snat;
    };

    typedef std::unordered_map<std::string, SnatState> snat_map_t;
    std::mutex snat_mutex;

    /**
     * Map snat-ip to snat state object
     */
    snat_map_t snat_map;

    /**
     * The snat listeners that have been registered
     */
    std::list<SnatListener*> snatListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);

    friend class SnatSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SNATMANAGER_H */
