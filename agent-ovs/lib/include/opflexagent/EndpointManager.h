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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <opflexagent/Endpoint.h>
#include <opflexagent/EndpointListener.h>
#include <opflexagent/PolicyManager.h>
#ifdef HAVE_PROMETHEUS_SUPPORT
#include <opflexagent/PrometheusManager.h>
#endif

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

class Agent;

typedef std::unordered_map<std::string,
                        std::shared_ptr<const Endpoint>> ip_ep_map_t;

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
#ifdef HAVE_PROMETHEUS_SUPPORT
    EndpointManager(Agent& agent,
                    opflex::ofcore::OFFramework& framework,
                    PolicyManager& policyManager,
                    PrometheusManager& prometheusManager);
#else
    EndpointManager(Agent& agent,
                    opflex::ofcore::OFFramework& framework,
                    PolicyManager& policyManager);
#endif

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
     * Get all endpoints
     *
     * @param eps a set that will be filled with the UUIDs of matching
     * endpoints.
     */
    void getEndpointUUIDs( /* out */ std::unordered_set<std::string>& eps);

    /**
     * Get IP to local EP map
     *
     * @return IP to local EP map
     */
    const ip_ep_map_t& getIPLocalEpMap(void);

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
     * Get all the known local external domains
     *
     * @param domain local external domain set to populate
     */
    void getLocalExternalDomains(std::unordered_set<opflex::modb::URI>& domain);

    /**
     * Get opflex agent instance
     *
     * @return instance of opflex agent
     */
    Agent& getAgent (void);

   /**
    * Callback to receive updates on changes to Platform/Config
    *
    * @param uri string uri reference to the platform/config object that changed.
    */
    void configUpdated(const opflex::modb::URI& uri);

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

    virtual void externalInterfaceUpdated(const opflex::modb::URI& extIntURI);
    /**
     * Get the adjacency(mac, interface) for a given L3 destination
     *
     * @param rdURI the URI of the RD where adjacency is needed
     * @param address string form of ipv4/6 address for adjacency
     * @param ep Endpoint structure corresponding to RD/address
     * @return whether adjacency was successfully retrieved
     */
    bool getAdjacency(const opflex::modb::URI& rdURI,
                      const std::string& address,
                      std::shared_ptr<const Endpoint> &ep);
    /**
     * Get whether the given local external domain URI is still
     * being referenced by any local EP.
     *
     * @param epgURI the URI of the local external domain
     * @return whether local external domain is still being referenced
     */
    bool localExternalDomainExists(const opflex::modb::URI& epgURI);

    /**
     * Get the external encap value for the given ext domain egURI
     *
     * @param epgURI the URI of the local external domain
     * @return external encap id
     */
    uint32_t getExtEncapId(const opflex::modb::URI& epgURI);

private:
    /**
     * Add or update the endpoint state with new information about an
     * endpoint.
     */
    void updateEndpoint(const Endpoint& endpoint);

    /**
     * Update the local endpoint entries associated with an endpoint
     * @param uuid uuid of the endpoint
     * @param extDomSet set of updated external domains
     * @return true if we should notify listeners
     */
    bool updateEndpointLocal(const std::string& uuid,
            const boost::optional<EndpointListener::uri_set_t &> extDomSet = boost::none);

    /**
     * Update the remote endpoint entries associated with an endpoint
     */
    void updateEndpointRemote(const opflex::modb::URI& uri);

    /**
     * Update the external endpoint entries associated with an endpoint
     */
    void updateEndpointExternal(const Endpoint& endpoint);

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

    /**
     * Remove the external endpoint with the specified UUID from the endpoint
     * manager.
     *
     * @param uuid the UUID of the endpoint that no longer exists
     */
    void removeEndpointExternal(const std::string& uuid);

    Agent& agent;
    opflex::ofcore::OFFramework& framework;
    PolicyManager& policyManager;
#ifdef HAVE_PROMETHEUS_SUPPORT
    PrometheusManager& prometheusManager;
#endif

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
    typedef std::unordered_map<std::string, opflex::modb::URI> ep_group_map_t;
    typedef std::unordered_map<opflex::modb::URI, std::string> ep_uuid_map_t;
    typedef std::unordered_map<std::string, str_uset_t> string_ep_map_t;
    typedef std::unordered_map<EndpointListener::uri_set_t,
                               str_uset_t> secgrp_ep_map_t;
    //typedef std::unordered_map<opflex::modb::URI, str_uset_t> ingress_uuid_map_t;
    //typedef std::unordered_map<opflex::modb::URI, str_uset_t> egress_uuid_map_t;
    typedef std::unordered_map<std::string,
                            std::shared_ptr<const Endpoint>> ipmac_map_t;
    typedef std::unordered_map<opflex::modb::URI, ipmac_map_t> adj_ep_map_t;
    typedef std::unordered_map<opflex::modb::URI, uint32_t> local_ext_dom_map_t;

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
     * Map IPs to local endpoints
     */
    ip_ep_map_t ip_local_ep_map;

    /**
     * Map remote endpoint URI to remote endpoint uuid
     */
    ep_uuid_map_t remote_ep_uuid_map;

    /**
     * Map endpoint group URI to a set of remote endpoint UUIDs
     */
    group_ep_map_t group_remote_ep_map;

    /**
     * Map remote endpoint UUID to endpoint group URI
     */
    ep_group_map_t remote_ep_group_map;

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
     * Map endpoint UUID to endpoint state object
     */
    ep_map_t ext_ep_map;

    /**
     * Map Ip address to mac address for external EPs
     */
    adj_ep_map_t adj_ep_map;
    /**
     * Set of all known local external domains
     */
    local_ext_dom_map_t local_ext_dom_map;

    /**
     * The endpoint listeners that have been registered
     */
    std::list<EndpointListener*> endpointListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);
    void notifyRemoteListeners(const std::string& uuid);
    void notifyListeners(const EndpointListener::uri_set_t& secGroups);
    void notifyExternalEndpointListeners(const std::string& uuid);
    void notifyLocalExternalDomainListeners(const opflex::modb::URI& uri);
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

    friend class EndpointSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ENDPOINTMANAGER_H */
