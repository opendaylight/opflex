/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for service manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SERVICEMANAGER_H
#define OPFLEXAGENT_SERVICEMANAGER_H

#include <opflexagent/Service.h>
#include <opflexagent/ServiceListener.h>

#include <boost/noncopyable.hpp>
#include <opflex/modb/URI.h>

#include <unordered_set>
#include <unordered_map>
#include <mutex>

namespace opflexagent {

/**
 * Manage service endpoints
 */
class ServiceManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new service manager
     */
    ServiceManager();

    /**
     * Register a listener for service change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see ServiceListener
     */
    void registerListener(ServiceListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(ServiceListener* listener);

    /**
     * Get the detailed information for an service
     *
     * @param uuid the UUID for the service
     * @return the object containing the detailed information, or a
     * NULL pointer if there is no such service
     */
    std::shared_ptr<const Service>
        getService(const std::string& uuid);

    /**
     * Get the services that are on a particular interface
     *
     * @param ifaceName the name of the interface
     * @param servs a set that will be filled with the UUIDs of
     * matching services.
     */
    void getServicesByIface(const std::string& ifaceName,
                            /* out */ std::unordered_set<std::string>& servs);

    /**
     * Get the services associated with a particular routing
     * domain
     *
     * @param domain the URI of the routing domain
     * @param servs a set that will be filled with the UUIDs of
     * matching services.
     */
    void getServicesByDomain(const opflex::modb::URI& domain,
                             /* out */ std::unordered_set<std::string>& servs);

private:
    /**
     * Add or update the service state with new information about an
     * service.
     */
    void updateService(const Service& service);

    /**
     * Remove the service with the specified UUID from the service
     * manager.
     *
     * @param uuid the UUID of the service that no longer exists
     */
    void removeService(const std::string& uuid);

    class ServiceState {
    public:
        std::shared_ptr<const Service> service;
    };

    typedef std::unordered_map<std::string, ServiceState> aserv_map_t;
    typedef std::unordered_map<std::string,
                               std::unordered_set<std::string> > string_serv_map_t;
    typedef std::unordered_map<opflex::modb::URI,
                               std::unordered_set<std::string> > uri_serv_map_t;

    std::mutex serv_mutex;

    /**
     * Map service UUID to service state object
     */
    aserv_map_t aserv_map;

    /**
     * Map interface names to a set of service UUIDs
     */
    string_serv_map_t iface_aserv_map;

    /**
     * Map domain URIs to a set of service UUIDs
     */
    uri_serv_map_t domain_aserv_map;

    /**
     * The service listeners that have been registered
     */
    std::list<ServiceListener*> serviceListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);
    void removeIfaces(const Service& service);
    void removeDomains(const Service& service);

    friend class ServiceSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SERVICEMANAGER_H */
