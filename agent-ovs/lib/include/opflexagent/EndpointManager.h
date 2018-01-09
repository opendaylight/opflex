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

#pragma once
#ifndef OPFLEXAGENT_ENDPOINTMANAGER_H
#define OPFLEXAGENT_ENDPOINTMANAGER_H

#include <opflexagent/Endpoint.h>
#include <opflexagent/EndpointListener.h>
#include <opflexagent/PolicyManager.h>

#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <modelgbp/metadata/metadata.hpp>
#include <boost/noncopyable.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <unordered_set>
#include <memory>
#include <mutex>

namespace opflexagent {

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
    std::shared_ptr<const Endpoint> getEndpoint(const std::string& uuid);

    /**
     * Get the effective default endpoint group as computed by
     * endpoint group mapping
     * @param uuid the UUID for the endpoint
     * @return the default endpoint group, or boost::none if there is
     * none
     */
    boost::optional<opflex::modb::URI> getComputedEPG(const std::string& uuid);

    /**
     * Get the set of endpoints that exist for a given endpoint group
     *
     * @param egURI the URI for the endpoint group
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsForGroup(const opflex::modb::URI& egURI,
                              /* out */ std::unordered_set<std::string>& eps);

    /**
     * Get the set of endpoints that exist for a given set of security groups
     *
     * @param secGrps the set of security groups
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsForSecGrps(const EndpointListener::uri_set_t& secGrps,
                                /* out */ std::unordered_set<std::string>& eps);

    /**
     * Check whether the given security group set contains any endpoints
     *
     * @param secGrps the security group set to check
     * @return true if the security group contains no endpoints
     */
    bool secGrpSetEmpty(const EndpointListener::uri_set_t& secGrps);

    /**
     * Get active security group sets for the given security group
     *
     * @param secGrp the security group to check
     * @param result a result set to hold the results
     */
    void getSecGrpSetsForSecGrp(const opflex::modb::URI& secGrp,
                                /* out */ std::unordered_set<EndpointListener
                                ::uri_set_t>& result);

    /**
     * Get the set of endpoints with IP address mappings mapped to the
     * given endpoint group
     *
     * @param egURI the URI for the endpoint group for the ip address
     * mappings
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsForIPMGroup(const opflex::modb::URI& egURI,
                                 /* out */ std::unordered_set<std::string>& eps);

    /**
     * Get the endpoints that are on a particular integration interface
     *
     * @param ifaceName the name of the interface
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsByIface(const std::string& ifaceName,
                             /* out */ std::unordered_set<std::string>& eps);

    /**
     * Get the endpoints that are on a particular access interface
     *
     * @param ifaceName the name of the interface
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsByAccessIface(const std::string& ifaceName,
                                   /* out */ std::unordered_set<std::string>& eps);

    /**
     * Get the endpoints that are on a particular access uplink interface
     *
     * @param ifaceName the name of the interface
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsByAccessUplink(const std::string& ifaceName,
                                    /* out */ std::unordered_set<std::string>& eps);

    /**
     * Get the endpoints that have an IP mapping next hop interface as
     * the specified interface
     *
     * @param ifaceName the name of the interface
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointsByIpmNextHopIf(const std::string& ifaceName,
                                    /* out */
                                    std::unordered_set<std::string>& eps);

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
     * Update the local endpoint entries associated with an endpoint
     * @return true if we should notify listeners
     */
    bool updateEndpointLocal(const std::string& uuid);

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

        std::shared_ptr<const Endpoint> endpoint;

        typedef std::unordered_set<opflex::modb::URI> uri_uset_t;

        /**
         * The EG URI for the endpoint as currently computed (may be
         * different from the URI in the endpoint object)
         */
        boost::optional<opflex::modb::URI> egURI;

        /**
         * The set of endpoint groups referenced by endpoint IP
         * address mappings
         */
        uri_uset_t ipMappingGroups;

        /**
         * reference to the vmep object related to this endpoint that
         * registers the VM to trigger attribute resolution
         */
        boost::optional<opflex::modb::URI> vmEP;

        // references to the modb epdr localL2 and locall3 objects
        // related to this endpoint that exist to cause policy
        // resolution
        uri_uset_t locall2EPs;
        uri_uset_t locall3EPs;

        // references to the modb epr l2ep and l3ep objects related to
        // this endpoint that will be reports to the endpoint registry
        uri_uset_t l2EPs;
        uri_uset_t l3EPs;

        /*
         * Attributes assigned to this endpoint by the endpoint
         * registry.  Attributes in the endpoint object override these
         * attributes.
         */
        Endpoint::attr_map_t epAttrs;
    };

    /**
     * Resolve the default endpoint group for an endpoint using its
     * epg mapping, if present
     */
    boost::optional<opflex::modb::URI> resolveEpgMapping(EndpointState& es);

    typedef std::unordered_map<std::string, EndpointState> ep_map_t;
    typedef std::unordered_set<std::string> str_uset_t;
    typedef std::unordered_map<opflex::modb::URI, str_uset_t> group_ep_map_t;
    typedef std::unordered_map<std::string, str_uset_t> string_ep_map_t;
    typedef std::unordered_map<EndpointListener::uri_set_t,
                               str_uset_t> secgrp_ep_map_t;

    std::mutex ep_mutex;

    /**
     * Map endpoint UUID to endpoint state object
     */
    ep_map_t ep_map;

    /**
     * Map endpoint group URI to a set of endpoint UUIDs
     */
    group_ep_map_t group_ep_map;

    /**
     * Map sets of security groups to a set of endpoint UUIDs
     */
    secgrp_ep_map_t secgrp_ep_map;

    /**
     * Map IP address mapping group URIs to a set of endpoint UUIDs
     */
    group_ep_map_t ipm_group_ep_map;

    /**
     * Map endpoint interface names to a set of endpoint UUIDs
     */
    string_ep_map_t iface_ep_map;

    /**
     * Map endpoint access interface names to a set of endpoint UUIDs
     */
    string_ep_map_t access_iface_ep_map;

    /**
     * Map endpoint access uplink interface names to a set of endpoint
     * UUIDs
     */
    string_ep_map_t access_uplink_ep_map;

    /**
     * Map ip address mapping next hop interface names to a set of
     * endpoint UUIDs
     */
    string_ep_map_t ipm_nexthop_if_ep_map;

    /**
     * Map epgmapping objects to a set of endpoint UUIDs that use them
     * for endpoint group mapping
     */
    string_ep_map_t epgmapping_ep_map;

    /**
     * The endpoint listeners that have been registered
     */
    std::list<EndpointListener*> endpointListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);
    void notifyListeners(const EndpointListener::uri_set_t& secGroups);

    /**
     * Listener for changes related to endpoint group mapping
     */
    class EPGMappingListener : public opflex::modb::ObjectListener {
    public:
        EPGMappingListener(EndpointManager& epmanager);
        virtual ~EPGMappingListener();

        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                   const opflex::modb::URI& uri);
    private:
        EndpointManager& epmanager;
    };
    EPGMappingListener epgMappingListener;

    friend class EPGMappingListener;
    friend class EndpointSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ENDPOINTMANAGER_H */
