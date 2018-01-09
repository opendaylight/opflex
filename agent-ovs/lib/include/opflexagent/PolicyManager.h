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

#pragma once
#ifndef OPFLEXAGENT_POLICYMANAGER_H
#define OPFLEXAGENT_POLICYMANAGER_H

#include <opflexagent/PolicyListener.h>
#include <opflexagent/Network.h>

#include <boost/noncopyable.hpp>
#include <boost/asio/ip/address.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <modelgbp/metadata/metadata.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/gbpe/L24Classifier.hpp>

#include <string>
#include <vector>
#include <list>
#include <set>
#include <limits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace opflexagent {

/**
 * Class to represent information about a rule.
 */
class PolicyRule {
public:
    /**
     * Constructor that accepts direction and L24Classifier.
     * @param dir The direction of the classifier rule
     * @param prio_ the priority for the rule
     * @param c Details of the classifier rule.  Must not be NULL.
     * @param allow_ true if the traffic should be allowed; false
     * otherwise
     * @param remoteSubnets_ remote subnets to which this rule applies
     */
    PolicyRule(const uint8_t dir,
               const uint16_t prio_,
               const std::shared_ptr<modelgbp::gbpe::L24Classifier>& c,
               bool allow_,
               const network::subnets_t& remoteSubnets_) :
        direction(dir), prio(prio_), l24Classifier(c), allow(allow_),
        remoteSubnets(remoteSubnets_) {
    }

    /**
     * Get the direction of the classifier rule.
     * @return the direction
     */
    uint8_t getDirection() const {
        return direction;
    }

    /**
     * Get the priority of the classifier rule.
     * @return the priority
     */
    uint16_t getPriority() const {
        return prio;
    }

    /**
     * Whether matching traffic should be allowed or dropped
     * @return true if traffic should be allowed, false otherwise
     */
    bool getAllow() const {
        return allow;
    }

    /**
     * Get remote subnets for this rule
     * @return the set of remote subnets
     */
    const network::subnets_t& getRemoteSubnets() const {
        return remoteSubnets;
    }

    /**
     * Get the L24Classifier object for the classifier rule.
     * @return the L24Classifier object.
     */
    const std::shared_ptr<modelgbp::gbpe::L24Classifier>&
    getL24Classifier() const {
        return l24Classifier;
    }

private:
    uint8_t direction;
    uint16_t prio;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> l24Classifier;
    bool allow;
    network::subnets_t remoteSubnets;

    friend bool operator==(const PolicyRule& lhs, const PolicyRule& rhs);
};

/**
 * PolicyRule stream insertion
 */
std::ostream& operator<<(std::ostream &os, const PolicyRule& rule);

/**
 * Check for PolicyRule equality.
 */
bool operator==(const PolicyRule& lhs, const PolicyRule& rhs);

/**
 * Check for PolicyRule inequality.
 */
bool operator!=(const PolicyRule& lhs, const PolicyRule& rhs);

/**
 * The policy manager maintains various state and indices related
 * to policy.
 */
class PolicyManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new policy manager using the specified framework
     * instance.
     * @param framework the opflex framework
     */
    PolicyManager(opflex::ofcore::OFFramework& framework);

    /**
     * Destroy the policy manager and clean up all state
     */
    ~PolicyManager();

    /**
     * Set the opflex domain for the policy manager
     *
     * @param opflexDomain the opflex domain
     */
    void setOpflexDomain(const std::string& opflexDomain) {
        this->opflexDomain = opflexDomain;
    }

    /**
     * Get the opflex domain for the policy manager
     *
     * @return opflexDomain the opflex domain
     */
    const std::string& getOpflexDomain() {
        return opflexDomain;
    }

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
    boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> >
    getRDForGroup(const opflex::modb::URI& eg);

    /**
     * Get the routing domain for the specified l3 external network if
     * it exists
     *
     * @param l3n the URI for the endpoint group
     * @return the routing domain or boost::none if the group or the
     * domain is not found
     */
    boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> >
    getRDForL3ExtNet(const opflex::modb::URI& l3n);

    /**
     * Get the bridge domain for the specified endpoint group if it
     * exists
     *
     * @param eg the URI for the endpoint group
     * @return the bridge domain or boost::none if the group or the
     * domain is not found
     */
    boost::optional<std::shared_ptr<modelgbp::gbp::BridgeDomain> >
    getBDForGroup(const opflex::modb::URI& eg);

    /**
     * Get the flood domain for the specified endpoint group if it
     * exists
     *
     * @param eg the URI for the endpoint group
     * @return the flood domain or boost::none if the group or the
     * domain is not found
     */
    boost::optional<std::shared_ptr<modelgbp::gbp::FloodDomain> >
    getFDForGroup(const opflex::modb::URI& eg);

    /**
     * Get the flood context for the specified endpoint group if it
     * exists
     *
     * @param eg the URI for the endpoint group
     * @return the flood context or boost::none if the group or the
     * domain is not found
     */
    boost::optional<std::shared_ptr<modelgbp::gbpe::FloodContext> >
    getFloodContextForGroup(const opflex::modb::URI& eg);

    /**
     * A vector of Subnet objects
     */
    typedef std::vector<std::shared_ptr<modelgbp::gbp::Subnet> >
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
     * Find the appropriate subnet for a given EP IP address given its
     * endpoint group.
     *
     * @param eg the endpoint group URI
     * @param ip the IP address for the endpoint to search for
     * @return the subnet if its found, or boost::none if there is
     * none found
     */
    boost::optional<std::shared_ptr<modelgbp::gbp::Subnet> >
    findSubnetForEp(const opflex::modb::URI& eg,
                    const boost::asio::ip::address& ip);

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
     * Get the endpoint group associated with the specified identifier
     *
     * @param vnid the VNID to look up
     * @return The endpoint group with that VNID if it is known,
     * boost::none otherwise
     */
    boost::optional<opflex::modb::URI> getGroupForVnid(uint32_t vnid);

    /**
     * Get the multicast IP group configured for an endpoint group.
     *
     * @param eg the URI for the endpoint group
     * @return Multicast IP for the group if any, boost::none otherwise
     */
    boost::optional<std::string>
    getMulticastIPForGroup(const opflex::modb::URI& eg);

    /**
     * Check if an endpoint group exists
     *
     * @param eg the URI for the endpoint group to check
     * @return true if group is found, false otherwise
     */
    bool groupExists(const opflex::modb::URI& eg);

    /**
     * List of PolicyRule objects.
     */
    typedef std::list<std::shared_ptr<PolicyRule> > rule_list_t;

    /**
     * Set of URIs.
     */
    typedef std::unordered_set<opflex::modb::URI> uri_set_t;

    /**
     * Get all known endpoint groups.
     *
     * @param epgURIs set of URIs of endpoint groups found.
     */
    void getGroups(/* out */ uri_set_t& epgURIs);

    /**
     * Get all known routing domains.
     *
     * @param rdURIs set of URIs of routing domains found.
     */
    void getRoutingDomains(/* out */ uri_set_t& rdURIs);

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
     * Get all endpoint groups that have an intra-EPG contract
     *
     * @param contractURI URI of contract to look for
     * @param epgURIs set of EPG URIs that consume the contract
     */
    void getContractIntra(const opflex::modb::URI& contractURI,
                          /* out */ uri_set_t& epgURIs);

    /**
     * Get the contracts consumed or provided by a group
     * @param egURI the group URI to look fo
     * @param contractURIs set of contract URIs that are consumed or
     * provided by the group
     */
    void getContractsForGroup(const opflex::modb::URI& egURI,
                              /* out */ uri_set_t& contractURIs);

    /**
     * Get an ordered list of PolicyRule objects that compose a
     * contract.
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

    /**
     * Get an ordered list of PolicyRule objects that compose a
     * security group.
     *
     * @param secGroupURI URI of contract to look for
     * @param rules List of classifier objects in descending order
     */
    void getSecGroupRules(const opflex::modb::URI& secGroupURI,
                          /* out */ rule_list_t& rules);


    /**
     * Get the routing-mode applicable to endpoints in specified group.
     *
     * @param egURI endpoint group to get routing mode for
     * @return a value from RoutingModeEnumT
     */
    uint8_t getEffectiveRoutingMode(const opflex::modb::URI& egURI);

    /**
     * Resolve all subnets referenced by the URI
     * @param framework the OFFramework
     * @param uri the URI of a modelgbp::gbp::Subnets object
     * @param subnets a result set for the output
     */
    static void resolveSubnets(opflex::ofcore::OFFramework& framework,
                               const boost::optional<opflex::modb::URI>& uri,
                               /* out */ network::subnets_t& subnets);

    /**
     * Get an appropriate router IP for the given subnet
     * @param subnet the subnet
     * @return a router IP for the subnet, or boost::none
     */
    static boost::optional<boost::asio::ip::address>
    getRouterIpForSubnet(modelgbp::gbp::Subnet& subnet);

    /**
     * Maximum flow priority of the entries in policy table.
     */
    static const uint16_t MAX_POLICY_RULE_PRIORITY;

private:
    opflex::ofcore::OFFramework& framework;
    std::string opflexDomain;

    /**
     * State and indices related to a given group
     */
    struct GroupState {
        boost::optional<std::shared_ptr<modelgbp::gbp::EpGroup> > epGroup;
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instContext;
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > routingDomain;
        boost::optional<std::shared_ptr<modelgbp::gbp::BridgeDomain> > bridgeDomain;
        boost::optional<std::shared_ptr<modelgbp::gbp::FloodDomain> > floodDomain;
        boost::optional<std::shared_ptr<modelgbp::gbpe::FloodContext> > floodContext;
        typedef std::unordered_map<opflex::modb::URI,
                                     std::shared_ptr<modelgbp::gbp::Subnet> > subnet_map_t;
        subnet_map_t subnet_map;
    };

    struct L3NetworkState {
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > routingDomain;
        boost::optional<opflex::modb::URI> natEpg;
    };

    struct RoutingDomainState {
        std::unordered_set<opflex::modb::URI> extNets;
    };

    typedef std::unordered_map<opflex::modb::URI, GroupState> group_map_t;
    typedef std::unordered_map<uint32_t, opflex::modb::URI> vnid_map_t;
    typedef std::unordered_map<opflex::modb::URI, RoutingDomainState> rd_map_t;
    typedef std::unordered_map<opflex::modb::URI, L3NetworkState> l3n_map_t;
    typedef std::unordered_map<opflex::modb::URI, uri_set_t> uri_ref_map_t;

    /**
     * A map from EPG URI to its state
     */
    group_map_t group_map;

    /**
     * A map from EPG vnid to EPG URI
     */
    vnid_map_t vnid_map;

    /**
     * A map from routing domain URI to its state
     */
    rd_map_t rd_map;

    /**
     * A map from l3 network URI to its state
     */
    l3n_map_t l3n_map;

    /**
     * A map from ep group URI to a set of l3 domains with l3 external
     * networks that reference it as a nat EPG
     */
    uri_ref_map_t nat_epg_l3_ext;

    std::mutex state_mutex;

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
        uri_sorted_set_t contractsIntra;
    };
    typedef std::unordered_map<opflex::modb::URI, GroupContractState>
        group_contract_map_t;

    /**
     * Map of EPG or network URI to contracts it is related to.
     */
    group_contract_map_t groupContractMap;

    /**
     * Information about a contract.
     */
    struct ContractState {
        uri_set_t providerGroups;
        uri_set_t consumerGroups;
        uri_set_t intraGroups;
        rule_list_t rules;
    };
    typedef std::unordered_map<opflex::modb::URI, ContractState>
        contract_map_t;

    /**
     * Map of Contract URI to its state.
     */
    contract_map_t contractMap;

    typedef std::unordered_map<opflex::modb::URI, rule_list_t> secgrp_map_t;

    /**
     * Map of security group URI to its rules
     */
    secgrp_map_t secGrpMap;

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
     * Listener for changes related to policy objects.
     */
    class SecGroupListener : public opflex::modb::ObjectListener {
    public:
        SecGroupListener(PolicyManager& pmanager);
        virtual ~SecGroupListener();

        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                    const opflex::modb::URI& uri);
    private:
        PolicyManager& pmanager;
    };
    SecGroupListener secGroupListener;

    friend class SecGroupListener;

    /**
     * Listener for changes related to plaform config.
     */
    class ConfigListener : public opflex::modb::ObjectListener {
    public:
        ConfigListener(PolicyManager& pmanager);
        virtual ~ConfigListener();

        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                    const opflex::modb::URI& uri);
    private:
        PolicyManager& pmanager;
    };
    ConfigListener configListener;

    friend class ConfigListener;

    /**
     * The policy listeners that have been registered
     */
    std::list<PolicyListener*> policyListeners;
    std::mutex listener_mutex;

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
     * Update the L3 network cache information for all L3 networks
     * associated with the given routing domain URI
     *
     * @param rdURI the URI of the routing domain containing the L3
     * networks
     * @param contractsToNotify contracts that need to be updated
     */
    void updateL3Nets(const opflex::modb::URI& rdURI,
                      uri_set_t& contractsToNotify);

    /**
     * Notify policy listeners about an update to a forwarding
     * domain.
     *
     * @param domURI the URI of the domain object that has been
     * updated
     */
    void notifyDomain(opflex::modb::class_id_t cid,
                      const opflex::modb::URI& domURI);

    /**
     * Updates groupPolicyMap and contractMap according to the
     * contracts provided and consumed by an endpoint group or
     * network.  If provider/consumer relationship of the group
     * w.r.t. a contract has changed, adds it to 'updatedContracts'.
     *
     * @param groupType the class ID for the contract consumer/provier
     * @param groupURI the URI of the endpoint group to update
     * @param updatedContracts Contracts whose relationship with
     * the group has changed
     */
    void updateGroupContracts(opflex::modb::class_id_t groupType,
                              const opflex::modb::URI& groupURI,
                              uri_set_t& updatedContracts);

    /**
     * Update the classifier rules associated with a contract.
     *
     * @param contractURI URI of contract to update
     * @param notFound set to true if contract could not be resolved
     * @return true if rules for this contract were updated
     */
    bool updateContractRules(const opflex::modb::URI& contractURI,
            bool& notFound);

    bool updateSecGrpRules(const opflex::modb::URI& secGrpURI,
                           bool& notFound);

    /**
     * Notify policy listeners about an update to a contract.
     *
     * @param contractURI the URI of the contract that has been
     * updated
     */
    void notifyContract(const opflex::modb::URI& contractURI);

    /**
     * Notify policy listeners about an update to a security group.
     *
     * @param secGroupURI the URI of the security group that has been
     * updated
     */
    void notifySecGroup(const opflex::modb::URI& secGroupURI);

    /**
     * Notify policy listeners about an update to the platform
     * configuration.
     *
     * @param configURI the URI of the updated platform config object
     */
    void notifyConfig(const opflex::modb::URI& configURI);

    /**
     * Remove index entry for a contract if it doesn't exist anymore
     * and if it is not referenced by any group or external network.
     * NOTE: Caller must hold lock state_mutex.
     *
     * @param contractURI the URI of the contract to check
     * @return true if the contract entry was removed
     */
    bool removeContractIfRequired(const opflex::modb::URI& contractURI);
};

/**
 * Comparator for sorting objects in ascending order of their
 * "order" member.
 */
template<typename T>
struct OrderComparator {
    /**
     * Implement the comparison
     *
     * @param lhs the left side of the comparison
     * @param rhs the right side of the comparison
     * @return true if the left side is has a lower order than the
     * right side
     */
    bool operator()(const T& lhs, const T& rhs) {
        return lhs->getOrder(std::numeric_limits<uint32_t>::max()) <
            rhs->getOrder(std::numeric_limits<uint32_t>::max());
    }
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_POLICYMANAGER_H */
