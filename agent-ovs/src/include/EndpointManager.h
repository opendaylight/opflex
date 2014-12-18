/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>
#include <boost/noncopyable.hpp>

#include "Endpoint.h"
#include "EndpointListener.h"
#include "PolicyManager.h"

#pragma once
#ifndef OVSAGENT_ENDPOINTMANAGER_H
#define OVSAGENT_ENDPOINTMANAGER_H

namespace ovsagent {

/**
 * The endpoint manager is responsible for maintaining the state
 * related to endpoints.  It discovers new endpoints on the system and
 * write the appropriate objects and references into the endpoint
 * registry.  It also indexes endpoints in various useful ways and
 * exposes events related to endpoint updates that are useful for
 * compiling policy into local system configuration.
 */
class EndpointManager : public PolicyListener, 
                        private boost::noncopyable {
public:
    /**
     * Instantiate a new endpoint manager using the specified framework
     * instance.
     */
    EndpointManager(opflex::ofcore::OFFramework& framework,
                    PolicyManager& policyManager);

    /**
     * Destroy the endpoint manager and clean up all state
     */
    virtual ~EndpointManager();

    /**
     * Start the endpoint manager
     */
    void start();

    /**
     * Stop the endpoint manager
     */
    void stop();

    /**
     * Register a listener for endpoint change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see EndpointListener
     */
    void registerListener(EndpointListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(EndpointListener* listener);

    /**
     * Get the detailed endpoint information for an endpoint
     *
     * @param uuid the UUID for the endpoint
     * @return the endpoint object containing the detailed endpoint
     * information, or a NULL pointer if there is no such endpoint
     */
    boost::shared_ptr<const Endpoint> getEndpoint(const std::string& uuid);

    /**
     * Get the set of endpoints that exist for a given endpoint group
     * 
     * @param egURI the URI for the endpoint group
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsForGroup(const opflex::modb::URI& egURI,
                              /* out */ boost::unordered_set<std::string>& eps);

    /**
     * Get the endpoints that are on a particular interface
     *
     * @param ifaceName the name of the interface
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsByIface(const std::string& ifaceName,
                             /* out */ boost::unordered_set<std::string>& eps);
    
    /**
     * Counter values for endpoint stats
     */
    struct EpCounters {
        /**
         * Number of packets sent
         */
        uint64_t txPackets;

        /**
         * Number of packets received
         */
        uint64_t rxPackets;

        /**
         * the number of dropped packets sent
         */
        uint64_t txDrop;

        /**
         * the number of dropped packets received
         */
        uint64_t rxDrop;

        /**
         * the number of multicast packets sent
         */
        uint64_t txMulticast;

        /**
         * the number of multicast packets received
         */
        uint64_t rxMulticast;

        /**
         * the number of broadcast packets sent
         */
        uint64_t txBroadcast;

        /**
         * the number of broadcast packets received
         */
        uint64_t rxBroadcast;

        /**
         * the number of unicast packets sent
         */
        uint64_t txUnicast;

        /**
         * the number of unicast packets received
         */
        uint64_t rxUnicast;

        /**
         * the number of bytes sent
         */
        uint64_t txBytes;

        /**
         * the number of bytes received
         */
        uint64_t rxBytes;
    };

    /**
     * Update the counters for an endpoint to the specified values
     *
     * @param uuid the UUID of the endpoint to update
     * @param newVals the new counter values
     */
    void updateEndpointCounters(const std::string& uuid,
                                EpCounters& newVals);

    // see PolicyListener
    virtual void egDomainUpdated(const opflex::modb::URI& egURI);

private:
    /**
     * Add or update the endpoint state with new information about an
     * endpoint.
     */
    void updateEndpoint(const Endpoint& endpoint);

    /**
     * Update the endpoint registry entries associated with an endpoint
     * @return true if we should notify listeners
     */
    bool updateEndpointReg(const std::string& uuid);

    /**
     * Remove the endpoint with the specified UUID from the endpoint
     * manager.
     *
     * @param uuid the UUID of the endpoint that no longer exists
     */
    void removeEndpoint(const std::string& uuid);

    opflex::ofcore::OFFramework& framework;
    PolicyManager& policyManager;

    class EndpointState {
    public:
        EndpointState();

        boost::shared_ptr<const Endpoint> endpoint;
        
        typedef boost::unordered_set<opflex::modb::URI> uri_set_t;
        // references to the modb epdr localL2 and locall3 objects
        // related to this endpoint that exist to cause policy
        // resolution
        uri_set_t locall2EPs;
        uri_set_t locall3EPs;

        // references to the modb epr l2ep and l3ep objects related to
        // this endpoint that will be reports to the endpoint registry
        uri_set_t l2EPs;
        uri_set_t l3EPs;
    };

    typedef boost::unordered_map<std::string, EndpointState> ep_map_t;
    typedef boost::unordered_map<opflex::modb::URI, 
                                 boost::unordered_set<std::string> > group_ep_map_t;
    typedef boost::unordered_map<std::string, 
                                 boost::unordered_set<std::string> > iface_ep_map_t;

    boost::mutex ep_mutex;

    /**
     * Map endpoint UUID to endpoint state object
     */
    ep_map_t ep_map;

    /**
     * Map endpoint group URI to a set of endpoint UUIDs
     */
    group_ep_map_t group_ep_map;

    /**
     * Map endpoint interface names to a set of endpoint UUIDs
     */
    iface_ep_map_t iface_ep_map;

    /**
     * The endpoint listeners that have been registered
     */
    std::list<EndpointListener*> endpointListeners;
    boost::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);

    friend class EndpointSource;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINTMANAGER_H */
