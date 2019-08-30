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
     * @param snatIp the snat ip address
     * @return the object containing the detailed information, or a
     * NULL pointer if there is no such snat
     */
    std::shared_ptr<const Snat>
        getSnat(const std::string& snatIp);

    /**
     * Add endpoint to ep_map
     *
     * @param snatIp the snat ip address
     * @param uuid the uuid of the endpoint
     */
    void addEndpoint(const std::string& snatIp,
                     const std::string& uuid);

    /**
     * Delete endpoint from ep_map
     *
     * @param snatIp the snat ip address
     * @param uuid the uuid of the endpoint
     */
    void delEndpoint(const std::string& snatIp,
                     const std::string& uuid);

    /**
     * Delete endpoint from ep_map
     * search all snat ips for the ep
     * and delete the ep
     *
     * @param uuid the uuid of the endpoint
     */
    void delEndpoint(const std::string& uuid);

    /**
     * Get endpoints for snat ip
     *
     * @param snatIp the snat ip address
     * @param eps endpoints using the snat ip
     */
    void getEndpoints(const std::string& snatIp,
                      /* out */ std::unordered_set<std::string>& eps);

    /**
     * Set of <uuid, snat-ip> pairs
     */
    typedef std::set<std::pair<std::string, std::string>> snats_t;

    /**
     * Get the snats that are on a particular interface
     *
     * @param ifaceName the name of the interface
     * @param snats a set of <uuid, snat-ip> pairs using the interface
     */
    void getSnatsByIface(const std::string& ifaceName,
                         /* out */ snats_t& snats);
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
     * @param uuid of the snat that no longer exists
     */
    void removeSnat(const std::string& snatIp, const std::string& uuid);

    class SnatState {
    public:
        std::shared_ptr<const Snat> snat;
    };

    typedef std::unordered_map<std::string, SnatState> snat_map_t;
    typedef std::unordered_set<std::string> ep_set_t;
    typedef std::unordered_map<std::string, ep_set_t> ep_map_t;
    typedef std::unordered_map<std::string, snats_t> iface_snats_map_t;
    std::mutex snat_mutex;

    /**
     * Map snat-ip to snat state object
     */
    snat_map_t snat_map;

    /**
     * Map snat-ip to endpoints using the snat
     */
    ep_map_t ep_map;

    /**
     * Map snat interface to set of <uuid, snat-ip> that share the interface
     */
    iface_snats_map_t iface_snats_map;

    /**
     * The snat listeners that have been registered
     */
    std::list<SnatListener*> snatListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& snatIp, const std::string& uuid);
    void removeIfaces(const Snat& snat);

    friend class SnatSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SNATMANAGER_H */
