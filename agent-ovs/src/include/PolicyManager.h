/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for policy manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <vector>
#include <list>
#include <set>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/thread.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <modelgbp/metadata/metadata.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/gbpe/L24Classifier.hpp>

#include "PolicyListener.h"

#pragma once
#ifndef OVSAGENT_POLICYMANAGER_H
#define OVSAGENT_POLICYMANAGER_H

namespace ovsagent {

/**
 * The policy manager maintains various state and indices related
 * to policy.
 */
class PolicyManager {
public:
    /**
     * Instantiate a new policy manager using the specified framework
     * instance.
     */
    PolicyManager(opflex::ofcore::OFFramework& framework_);

    /**
     * Destroy the policy manager and clean up all state
     */
    ~PolicyManager();

    /**
     * Start the policy manager
     */
    void start();

    /**
     * Stop the policy manager
     */
    void stop();

    /**
     * Register a listener for policy change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see PolicyListener
     */
    void registerListener(PolicyListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(PolicyListener* listener);

    /**
     * Get the routing domain for the specified endpoint group if it
     * exists
     *
     * @param eg the URI for the endpoint group
     * @return the routing domain or boost::none if the group or the
     * domain is not found
     */
    boost::optional<boost::shared_ptr<modelgbp::gbp::RoutingDomain> >
    getRDForGroup(const opflex::modb::URI& eg);

    /**
     * Get the bridge domain for the specified endpoint group if it
     * exists
     *
     * @param eg the URI for the endpoint group
     * @return the bridge domain or boost::none if the group or the
     * domain is not found
     */
    boost::optional<boost::shared_ptr<modelgbp::gbp::BridgeDomain> >
    getBDForGroup(const opflex::modb::URI& eg);

    /**
     * Get the flood domain for the specified endpoint group if it
     * exists
     *
     * @param eg the URI for the endpoint group
     * @return the flood domain or boost::none if the group or the
     * domain is not found
     */
    boost::optional<boost::shared_ptr<modelgbp::gbp::FloodDomain> >
    getFDForGroup(const opflex::modb::URI& eg);

    /**
     * A vector of Subnet objects
     */
    typedef std::vector<boost::shared_ptr<modelgbp::gbp::Subnet> > 
    subnet_vector_t;

    /**
     * Get all the relevent subnets for the endpoint group specified.
     * This includes any subnets that are directly referenced by the
     * group, as well as any subnets that directly reference an
     * "ancestor" forwarding domain.
     *
     * @param eg the URI for the endpoint group
     * @param subnets a vector that will receive the subnets
     */
    void getSubnetsForGroup(const opflex::modb::URI& eg,
                            /* out */ subnet_vector_t& subnets);

    /**
     * Get the subnet URIs for subnets that directly reference the
     * given network domain URI
     *
     * @param domainUri a URI referencing a network domain
     * @param uris the set of URIs that reference the domain
     */
    void getSubnetsForDomain(const opflex::modb::URI& domainUri,
                             /* out */ boost::unordered_set<opflex::modb::URI>& uris);

    /**
     * Get the virtual-network identifier (vnid) associated with the
     * specified endpoint group.
     *
     * @param eg the URI for the endpoint group
     * @return vnid of the group if group is found and its vnid is set,
     * boost::none otherwise
     */
    boost::optional<uint32_t> getVnidForGroup(const opflex::modb::URI& eg);

    /**
     * Check if an endpoint group exists
     *
     * @param eg the URI for the endpoint group to check
     * @return true if group is found, false otherwise
     */
    bool groupExists(const opflex::modb::URI& eg);

    /**
     * List of L24Classifier objects.
     */
    typedef std::list<boost::shared_ptr<modelgbp::gbpe::L24Classifier> >
        rule_list_t;

    /**
     * Set of URIs.
     */
    typedef boost::unordered_set<opflex::modb::URI> uri_set_t;

    /**
     * Get all endpoint groups that provide a contract.
     *
     * @param contractURI URI of contract to look for
     * @param epgURIs set of EPG URIs that provide the contract
     */
    void getContractProviders(const opflex::modb::URI& contractURI,
                             /* out */ uri_set_t& epgURIs);

    /**
     * Get all endpoint groups that consume a contract.
     *
     * @param contractURI URI of contract to look for
     * @param epgURIs set of EPG URIs that consume the contract
     */
    void getContractConsumers(const opflex::modb::URI& contractURI,
                             /* out */ uri_set_t& epgURIs);

    /**
     * Get an ordered list of L24Classifier objects that comprise a contract.
     *
     * @param contractURI URI of contract to look for
     * @param rules List of classifier objects in descending order
     */
    void getContractRules(const opflex::modb::URI& contractURI,
                          /* out */ rule_list_t& rules);

    /**
     * Check if a contract exists.
     *
     * @param contractURI URI of the contract to check
     * @return true if contract is found, false otherwise
     */
    bool contractExists(const opflex::modb::URI& contractURI);

private:
    opflex::ofcore::OFFramework& framework;

    /**
     * State and indices related to a given endpoint group
     */
    struct GroupState {
        boost::optional<uint32_t> vnid;
        boost::optional<boost::shared_ptr<modelgbp::gbp::RoutingDomain> > routingDomain;
        boost::optional<boost::shared_ptr<modelgbp::gbp::BridgeDomain> > bridgeDomain;
        boost::optional<boost::shared_ptr<modelgbp::gbp::FloodDomain> > floodDomain;
        typedef boost::unordered_map<opflex::modb::URI,
                                     boost::shared_ptr<modelgbp::gbp::Subnet> > subnet_map_t;
        subnet_map_t subnet_map;
    };

    typedef boost::unordered_map<opflex::modb::URI, GroupState> group_map_t;
    typedef boost::unordered_map<opflex::modb::URI,
                                 boost::unordered_set<opflex::modb::URI> > uri_ref_map_t;

    /**
     * A map from EPG URI to its state
     */ 
    group_map_t group_map;

    /**
     * A map from a forwarding domain URI to a set of subnets that
     * refer to it
     */
    uri_ref_map_t subnet_ref_map;

    boost::mutex state_mutex;

    // Listen to changes related to forwarding domains
    class DomainListener : public opflex::modb::ObjectListener {
    public:
        DomainListener(PolicyManager& pmanager);
        virtual ~DomainListener();

        virtual void objectUpdated (opflex::modb::class_id_t class_id,
                                    const opflex::modb::URI& uri);
    private:
        PolicyManager& pmanager;
    };

    DomainListener domainListener;

    friend class DomainListener;

    /**
     * Set of URIs in sorted order.
     */
    typedef std::set<opflex::modb::URI> uri_sorted_set_t;

    /**
     * Contracts provided and consumed by an endpoint group.
     */
    struct GroupContractState {
        uri_sorted_set_t contractsProvided;
        uri_sorted_set_t contractsConsumed;
    };
    typedef boost::unordered_map<opflex::modb::URI, GroupContractState>
        group_contract_map_t;

    /**
     * Map of EPG URI to contracts it is related to.
     */
    group_contract_map_t groupContractMap;

    /**
     * Information about a contract.
     */
    struct ContractState {
        uri_set_t providerGroups;
        uri_set_t consumerGroups;
        rule_list_t rules;
    };
    typedef boost::unordered_map<opflex::modb::URI, ContractState>
        contract_map_t;

    /**
     * Map of Contract URI to its state.
     */
    contract_map_t contractMap;

    /**
     * Listener for changes related to policy objects.
     */
    class ContractListener : public opflex::modb::ObjectListener {
    public:
        ContractListener(PolicyManager& pmanager);
        virtual ~ContractListener();

        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                    const opflex::modb::URI& uri);
    private:
        PolicyManager& pmanager;
    };
    ContractListener contractListener;

    friend class ContractListener;

    /**
     * The policy listeners that have been registered
     */
    std::list<PolicyListener*> policyListeners;
    boost::mutex listener_mutex;

    /**
     * Update the subnet URI map.  You must hold a state lock to call
     * this function.
     */
    void updateSubnetIndex();

    /**
     * Update the EPG domain cache information for the specified EPG
     * URI.  You must hold a state lock to call this function.
     *
     * @param egURI the URI of the EPG that should be updated
     * @param toRemove set to true is the group was removed
     * @return true if the endpoint group was updated
     */
    bool updateEPGDomains(const opflex::modb::URI& egURI, bool& toRemove);

    /**
     * Notify policy listeners about an update to the forwarding
     * domains for an endpoint group.
     *
     * @param egURI the URI of the endpoint group that has been
     * updated
     */
    void notifyEPGDomain(const opflex::modb::URI& egURI);

    /**
     * Updates groupPolicyMap and contractMap according to the
     * contracts provided and consumed by an endpoint group.
     * If provider/consumer relationship of the group w.r.t. a
     * contract has changed, adds it to 'updatedContracts'.
     *
     * @param egURI the URI of the endpoint group to update
     * @param updatedContracts Contracts whose relationship with
     * the group has changed
     */
    void updateEPGContracts(const opflex::modb::URI& egURI,
            uri_set_t& updatedContracts);

    /**
     * Update the classifier rules associated with a contract.
     *
     * @param contractURI URI of contract to update
     * @param toRemove set to true if contract was removed.
     * @return true if rules for this contract were updated or
     * contract was removed
     */
    bool updateContractRules(const opflex::modb::URI& contractURI,
            bool& toRemove);

    /**
     * Notify policy listeners about an update to a contract.
     *
     * @param contractURI the URI of the contract that has been
     * updated
     */
    void notifyContract(const opflex::modb::URI& contractURI);

};

} /* namespace ovsagent */

#endif /* OVSAGENT_POLICYMANAGER_H */
