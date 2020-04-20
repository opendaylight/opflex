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
#include <opflexagent/TaskQueue.h>

#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
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
     * @param redirect_ points to a redirect action
     * @param rDG redirect URI
     */
    PolicyRule(const uint8_t dir,
               const uint16_t prio_,
               const std::shared_ptr<modelgbp::gbpe::L24Classifier>& c,
               bool allow_,
               const network::subnets_t& remoteSubnets_,
               bool redirect_,
               const boost::optional<opflex::modb::URI>& rDG
               ) :
        direction(dir), prio(prio_), l24Classifier(c), allow(allow_),
        redirect(redirect_), remoteSubnets(remoteSubnets_),
        redirDstGrp(rDG) {
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

    /**
     * Get the RedirectDestGrpURI for the classifier rule.
     * @return the RedirectDestGrp URI.
     */
    boost::optional<opflex::modb::URI> getRedirectDestGrpURI() const
    {
        return redirDstGrp;
    }

    /**
     * Whether matching traffic should be redirected
     * @return true if traffic should be redirected, false otherwise
     */
    bool getRedirect() const {
        return redirect;
    }

private:
    uint8_t direction;
    uint16_t prio;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> l24Classifier;
    bool allow;
    bool redirect;
    network::subnets_t remoteSubnets;
    boost::optional<opflex::modb::URI> redirDstGrp;
    friend bool operator==(const PolicyRule& lhs, const PolicyRule& rhs);
};

/**
 * Class to represent information about a redirect destination.
 */
class PolicyRedirectDest {
public:
    /**
     * Constructor that accepts arguments for all  private members.
     * @param dst_ RedirectDest object
     * @param ip_ Redirect Ip address
     * @param mac_ Redirect mac address
     * @param rd_ Routing Domain object
     * @param bd_ Bridge Domain object
     */
    PolicyRedirectDest(const std::shared_ptr<modelgbp::gbp::RedirectDest> dst_,
                       const boost::asio::ip::address& ip_,
                       const opflex::modb::MAC& mac_,
                       const std::shared_ptr<modelgbp::gbp::RoutingDomain>& rd_,
                       const std::shared_ptr<modelgbp::gbp::BridgeDomain>& bd_
                       ):ip(ip_), mac(mac_), redirDst(dst_), rd(rd_), bd(bd_){}
    /**
     * Get the RoutingDomain for the redirect destination.
     * @return the RoutingDomain object.
     */
    const std::shared_ptr<modelgbp::gbp::RoutingDomain> getRD () const {
        return rd;
    }

    /**
     * Get the BridgeDomain for the redirect destination.
     * @return the BridgeDomain object.
     */
    const std::shared_ptr<modelgbp::gbp::BridgeDomain> getBD () const {
        return bd;
    }

    /**
     * Get the ip address for the redirect destination.
     * @return the ip address .
     */
    boost::asio::ip::address getIp() const {
        return ip;
    }

    /**
     * Get the mac address for the redirect destination.
     * @return the mac address .
     */
    opflex::modb::MAC getMac() const {
        return mac;
    }

private:
    boost::asio::ip::address ip;
    opflex::modb::MAC mac;
    std::shared_ptr<modelgbp::gbp::RedirectDest> redirDst;
    std::shared_ptr<modelgbp::gbp::RoutingDomain> rd;
    std::shared_ptr<modelgbp::gbp::BridgeDomain> bd;
    friend bool operator==(const PolicyRedirectDest& lhs,
                           const PolicyRedirectDest& rhs);
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
 * Class to represent a cached route.
 */
class PolicyRoute {
public:

    /**
     * Constructor that has all private fields as arguments.
     * @param rd_ routing domain
     * @param rdInst_ instance context of the routing domain
     * @param addr_ prefix of the route
     * @param pfx_len_ prefix length for the route
     * @param nh_ list of next-hops for this route
     * @param nd_ external node for this route
     */
    PolicyRoute(std::shared_ptr<modelgbp::gbp::RoutingDomain>& rd_,
                std::shared_ptr<modelgbp::gbpe::InstContext>& rdInst_,
                const boost::asio::ip::address& addr_, uint8_t pfx_len_,
                std::list<boost::asio::ip::address> &nh_,
                std::shared_ptr<modelgbp::gbp::ExternalNode> nd_=
                std::shared_ptr<modelgbp::gbp::ExternalNode>()):
                rd(rd_), rdInst(rdInst_), address(addr_),
                prefix_len(pfx_len_), nextHops(nh_), present(false),
                nd(nd_) {};

    /**
     * Copy constructor.
     * @param pRoute Copy source
     */
    PolicyRoute(const PolicyRoute& pRoute):
                rd(pRoute.rd), rdInst(pRoute.rdInst),
                address(pRoute.address),
                prefix_len(pRoute.prefix_len),
                nextHops(pRoute.nextHops),
                present(false), nd(pRoute.nd) {};

    /**
     * Set an internal flag while parsing the Route container's list of routes.
     */
    void setPresent(bool prst_) {
        present = prst_;
    }

    /**
     * Check whether internal flag is set on this route.
     */
    bool isPresent() const {
        return present;
    }

    /**
     * Get route details
     * @param rd_ routing domain
     * @param rdInst_ instance context of the routing domain
     * @param addr_ prefix of the route
     * @param pfx_len prefix length for the route
     * @param nhList list of next-hops for this route
     */
    void getRoute(std::shared_ptr<modelgbp::gbp::RoutingDomain> &rd_,
                  std::shared_ptr<modelgbp::gbpe::InstContext> &rdInst_,
                  boost::asio::ip::address &addr_, uint8_t &pfx_len,
                  std::list<boost::asio::ip::address> &nhList) const {
        rd_ = rd;
        rdInst_ = rdInst;
        addr_ = address;
        pfx_len = prefix_len;
        nhList = nextHops;
    }

    /**
     * Get URI of the container external node for a static route.
     * @param uri URI of the external node
     */
    void getExtNodeURI(boost::optional<opflex::modb::URI> &uri) {
        if(nd) {
            uri = nd->getURI();
        }
    }

    /**
     * Get URI of the routing domain for this route.
     * @return URI of the routing domain
     */
    opflex::modb::URI getRDURI() {
        return rd->getURI();
    }

    /**
     * Assign another policy route to this one.
     */
    PolicyRoute& operator=(const PolicyRoute& rhs);

    /**
     * Get prefix for the route
     * @return prefix
     */
    const boost::asio::ip::address& getAddress() {
        return address;
    }

    /**
     * Get prefix length.
     * @return prefix length
     */
    uint32_t getPrefixLen() const {
        return prefix_len;
    }

private:
    std::shared_ptr<modelgbp::gbp::RoutingDomain> rd;
    std::shared_ptr<modelgbp::gbpe::InstContext> rdInst;
    boost::asio::ip::address address;
    uint8_t prefix_len;
    std::list<boost::asio::ip::address> nextHops;
    bool present;
    std::shared_ptr<modelgbp::gbp::ExternalNode> nd;
    friend bool operator==(const PolicyRoute& lhs, const PolicyRoute& rhs);
};

/**
 * Check for PolicyRoute equality.
 */
bool operator==(const PolicyRoute& lhs, const PolicyRoute& rhs);


/**
 * Check for PolicyRoute inequality.
 */
bool operator!=(const PolicyRoute& lhs, const PolicyRoute& rhs);

/**
 * The policy manager maintains various state and indices related
 * to policy.
 */
class PolicyManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new policy manager using the specified framework
     * instance.
     * @param framework_ the framework object
     * @param agent_io_ an IO service object for asynchronous task
     * execution.
     */
    PolicyManager(opflex::ofcore::OFFramework& framework_,
                  boost::asio::io_service& agent_io_);

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
     * Get the sclass or source pctag for an endpoint group.
     *
     * @param eg the URI for the endpoint group
     * @return sclass for the group if any, boost::none otherwise
     */
    boost::optional<uint32_t>
    getSclassForGroup(const opflex::modb::URI& eg);

    /**
     * Get the bdvnid for an external interface.
     *
     * @param eg the URI for the external interface
     * @return bd vnid for the external interface if any, boost::none otherwise
     */
    boost::optional<uint32_t>
    getBDVnidForExternalInterface(const opflex::modb::URI& eg);

    /**
     * Get the rd for an external interface.
     *
     * @param eg the URI for the external interface
     * @return rd for the external interface if any, boost::none otherwise
     */
    boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain>>
    getRDForExternalInterface(const opflex::modb::URI& eg);

    /**
     * Get all the relevant subnets for the external interface specified.
     *
     * @param eg the URI for the external interface
     * @param subnets a vector that will receive the subnets
     */
    void getSubnetsForExternalInterface(const opflex::modb::URI& eg,
                            /* out */ subnet_vector_t& subnets);

    /**
     * Get the sclass associated with the external BD of an
     * external interface.
     *
     * @param eg the URI for the external interface
     * @return sclass for the external interface if any,
     * boost::none otherwise
     */
    boost::optional<uint32_t>
    getSclassForExternalInterface(const opflex::modb::URI& eg);

    /**
     * Get sclass associated with an external network (external epg).
     *
     * @param en the URI for the external network
     * @return sclass for the external network, if any
     * boost::none otherwise
     */
    boost::optional<uint32_t> getSclassForExternalNet(
        const opflex::modb::URI& en);

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

    /**
     * List of entries in a redirect dest group entry.
     */
    typedef std::list<std::shared_ptr<PolicyRedirectDest>> redir_dest_list_t;

    /**
     * Get the redirectdestgroup contents for the given URI
     * @param redirDstURI URI for the redirectdestgroup
     * @param redirList list of dests for the redirectdestgroup
     * @param hashParam whether hashing should be non-symmetric(1)
     * @param hashOpt hash function value HashProfOptionEnumT
     * @return whether the redirectdestgroup is valid
     */
    bool getPolicyDestGroup(const opflex::modb::URI& redirDstURI,
                            redir_dest_list_t &redirList, uint8_t &hashParam,
                            uint8_t &hashOpt);

    /**
     * Get route details for the given URI
     * @param route_type class id of the route object
     * @param rtURI URI for the route
     * @param self_tep address of self-tep
     * @param rd_ routing domain
     * @param rdInst_ instance context of the routing domain
     * @param addr_ prefix of the route
     * @param pfx_len prefix length for the route
     * @param nhList list of next-hops for this route
     * @param are_nhs_remote are next-hops for this route in overlay
     * @param sclass sclass for this route
     */
    bool getRoute(opflex::modb::class_id_t route_type,
                  const opflex::modb::URI & rtURI,
                  const boost::asio::ip::address &self_tep,
		  std::shared_ptr<modelgbp::gbp::RoutingDomain> &rd_,
                  std::shared_ptr<modelgbp::gbpe::InstContext> &rdInst_,
                  boost::asio::ip::address &addr_, uint8_t &pfx_len,
                  std::list<boost::asio::ip::address> &nhList,
                  bool &are_nhs_remote,
                  boost::optional<uint32_t> &sclass);
    /**
     * Manage subnets URI to a set of routing domains URIs map
     */
    void addRoutingDomainToSubnets(const opflex::modb::URI& subnets,
                                   const opflex::modb::URI& rd);
    /**
     * Handle routing domain deletion
     */
    void deleteRoutingDomain(const opflex::modb::URI& rd);
    /**
     * Handle Subnets deletion
     */
    void deleteSubnets(const opflex::modb::URI& subnets);
private:
    opflex::ofcore::OFFramework& framework;
    std::string opflexDomain;
    TaskQueue taskQueue;
    typedef std::unordered_map<opflex::modb::URI,
                std::shared_ptr<modelgbp::gbp::Subnet> > subnet_map_t;
    typedef std::unordered_map<opflex::modb::URI,
                std::shared_ptr<modelgbp::gbp::ExternalSubnet> >
                    ext_subnet_map_t;
    /**
     * State and indices related to a given group
     */
    struct GroupState {
        boost::optional<std::shared_ptr<modelgbp::gbp::EpGroup> > epGroup;
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instContext;
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instBDContext;
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instRDContext;
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > routingDomain;
        boost::optional<std::shared_ptr<modelgbp::gbp::BridgeDomain> > bridgeDomain;
        boost::optional<std::shared_ptr<modelgbp::gbp::FloodDomain> > floodDomain;
        boost::optional<std::shared_ptr<modelgbp::gbpe::FloodContext> > floodContext;
        boost::optional<std::shared_ptr<modelgbp::gbpe::EndpointRetention> > l2EpRetPolicy;
        boost::optional<std::shared_ptr<modelgbp::gbpe::EndpointRetention> > l3EpRetPolicy;
        subnet_map_t subnet_map;
    };

    struct ExternalInterfaceState {
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instContext;
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instRDContext;
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > routingDomain;
        boost::optional<std::shared_ptr<modelgbp::gbp::ExternalL3BridgeDomain> > bridgeDomain;
        boost::optional<std::shared_ptr<modelgbp::gbp::L3ExternalDomain> > extDomain;
        subnet_map_t subnet_map;
    };

    struct L3NetworkState {
        boost::optional<std::shared_ptr<modelgbp::gbp::L3ExternalNetwork>> extNet;
        boost::optional<std::shared_ptr<modelgbp::gbpe::InstContext> > instContext;
        boost::optional<std::shared_ptr<modelgbp::gbp::RoutingDomain> > routingDomain;
        boost::optional<opflex::modb::URI> natEpg;
        ext_subnet_map_t subnet_map;
    };

    typedef std::unordered_map<opflex::modb::URI, std::shared_ptr<PolicyRoute>>
        route_map_t;

    struct RoutingDomainState {
        std::unordered_set<opflex::modb::URI> extNets;
        uri_set_t remote_routes;
    };

    struct ExternalNodeState {
        uri_set_t static_routes;
    };

    route_map_t static_route_map;
    route_map_t remote_route_map;

    typedef std::unordered_map<opflex::modb::URI, GroupState> group_map_t;
    typedef std::unordered_map<uint32_t, opflex::modb::URI> vnid_map_t;
    typedef std::unordered_map<opflex::modb::URI, RoutingDomainState> rd_map_t;
    typedef std::unordered_map<opflex::modb::URI, L3NetworkState> l3n_map_t;
    typedef std::unordered_map<opflex::modb::URI, uri_set_t> uri_ref_map_t;
    typedef std::unordered_map<opflex::modb::URI, ExternalInterfaceState> \
    ext_int_map_t;
    typedef std::unordered_map<opflex::modb::URI, ExternalNodeState> \
    ext_node_map_t;
    typedef std::unordered_map<opflex::modb::URI, uri_set_t> subnets_rd_map_t;
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
     * A map from external interface URI to its state
     */
    ext_int_map_t ext_int_map;

    /**
     * A map from external interface URI to its state
     */
    ext_node_map_t ext_node_map;

    /**
     * A map from ep group URI to a set of l3 domains with l3 external
     * networks that reference it as a nat EPG
     */
    uri_ref_map_t nat_epg_l3_ext;

    /**
     * A map from subnets URI to a set of routing domain URIs
     */
    subnets_rd_map_t subnets_rd_map;

    std::mutex state_mutex;
    std::mutex subnets_rd_mutex;

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

    /**
     * Listener for changes related to routes.
     */
    class RouteListener : public opflex::modb::ObjectListener {
    public:
        RouteListener(PolicyManager& pmanager);
        virtual ~RouteListener();

        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                    const opflex::modb::URI& uri);
    private:
        PolicyManager& pmanager;
    };
    RouteListener routeListener;

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
     * Notify policy listeners about an update to an external interface.
     *
     * @param extIntfURI the URI of the endpoint group that has been
     * updated
     */
    void notifyExternalInterface(const opflex::modb::URI& extIntfURI);

    /**
     * Notify policy listeners about an update to a static route.
     *
     * @staticRtURI the URI of the static route that has been updated
     */
    void notifyStaticRoute(const opflex::modb::URI& staticRtURI);

    /**
     * Notify policy listeners about an update to a remote route.
     *
     * @remoteRtURI the URI of the remote route that has been updated
     */
    void notifyRemoteRoute(const opflex::modb::URI& remoteRtURI);

    /**
     * Notify policy listeners about an update to a local route.
     *
     * @localRtURI the URI of the local route that has been updated
     */
    void notifyLocalRoute(const opflex::modb::URI& localRtURI);

    /**
     * Update the L3 network cache information for all L3 networks
     * associated with the given routing domain URI
     *
     * @param rdURI the URI of the routing domain containing the L3
     * networks
     * @param contractsToNotify contracts that need to be updated
     * @param notifyLocalRoutes localRoutes that need to be updated
     */
    void updateL3Nets(const opflex::modb::URI& rdURI,
                      uri_set_t& contractsToNotify,
                      uri_set_t& notifyLocalRoutes);

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
     * Recompute contract rules and notify listeners as needed
     */
    void updateContracts();

    /**
     * Recompute security group rules and notify listeners as needed
     */
    void updateSecGrps();

    /**
     * Update state for the domain listener
     */
    void updateDomain(opflex::modb::class_id_t class_id,
                      const opflex::modb::URI& uri);

    /**
     * Execute a function and notify contract listeners
     * @param func the function to execute
     */
    void executeAndNotifyContract(const std::function<void(uri_set_t&)>& func);

    /**
     * Execute a function and notify contract and route listeners
     * @param func the function to execute
     */
    void executeAndNotifyContractAndRoute(const std::function<
                                          void(uri_set_t&, uri_set_t&)>& func);

    /**
     * Execute a function and notify route listeners.
     * @param func the function to execute
     */
    void executeAndNotifyRoutes(bool, const std::function<
                                 void(uri_set_t&, uri_set_t&)>& func);

    /**
     * Notify policy listeners about an update to a contract.s
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

    struct RedirectDestGrpState {
        uint8_t resilientHashEnabled;
        uint8_t hashAlgo;
        redir_dest_list_t redirDstList;
        uri_set_t ctrctSet;
    };
    /**
     * Map of redirect dest group URI to its state
     */
    typedef std::unordered_map<opflex::modb::URI, RedirectDestGrpState> \
    redir_dst_grp_map_t;
    redir_dst_grp_map_t redirGrpMap;

    /**
     * Update the redirect destination cache information for the
     * specified redirect destination.  You must hold a state
     * lock to call this function.
     *
     * @param uri URI of the redirect dest group that was updated.
     * @param notify the URI set of contracts that were updated.
     */
    void updateRedirectDestGroup(const opflex::modb::URI& uri,
                                 uri_set_t &notify);
    /**
     * Update the redirect destination cache information for all
     * the known redirect destinations.  You must hold a state
     * lock to call this function.
     *
     * @param notify the URI set of contracts that were updated.
     */
    void updateRedirectDestGroups(uri_set_t &notify);

    /**
     * Update the external interface cache information for the
     * specified external interface URI.  You must hold a state
     * lock to call this function.
     *
     * @param extIntURI the URI of the external interface that should
     * be updated
     * @return true if the external interface was updated
     */
    bool updateExternalInterface(const opflex::modb::URI& extIntURI,
                                 bool &toRemove);
    /**
     * Update the route cache information for the specified external node.
     * You must hold a state lock to call this function.
     *
     * @param uri URI of the external node that should be updated
     * @param notifyStaticRoutes set of static route URIs that were updated
     * @param notifyLocalRoutes set of local route URIs that were updated
     */
    void updateExternalNode(const opflex::modb::URI& uri,
                            uri_set_t &notifyStaticRoutes,
                            uri_set_t &notifyLocalRoutes);
    /**
     * Update the route cache information for the trigger.
     * You must hold a state lock to call this function.
     *
     * @param classId class id of the object that triggered the update
     * @param uri URI of the object that that triggered the update
     * @param notifyStaticRoutes set of static route URIs that were updated
     * @param notifyLocalRoutes set of local route URIs that were updated
     */
    void updateStaticRoutes(opflex::modb::class_id_t classId,
                            const opflex::modb::URI& uri,
                            uri_set_t &notifyStaticRoutes,
                            uri_set_t &notifyLocalRoutes);
    /**
     * Update the route cache information for the specified remote route.
     * You must hold a state lock to call this function.
     *
     * @param uri URI of the object that should be updated
     * @param notifyRemoteRoutes set of static route URIs that were updated
     * @param notifyLocalRoutes set of local route URIs that were updated
     */
    void updateRemoteRoutes(const opflex::modb::URI& uri,
                            uri_set_t &notifyRemoteRoutes,
                            uri_set_t &notifyLocalRoutes);
    /**
     * Update the static route cache information for the trigger.
     * You must hold a state lock to call this function.
     *
     * @param classId class id of the object that triggered the update
     * @param uri URI of the object that that triggered the update
     * @param notifyStaticRoutes set of static route URIs that were updated
     * @param notifyLocalRoutes set of local route URIs that were updated
     */
    void updateStaticRoute(opflex::modb::class_id_t classId,
                           const opflex::modb::URI& uri,
                           uri_set_t &notifyStaticRoutes,
                           uri_set_t &notifyLocalRoutes);
    /**
     * Update the route cache information for the specified remote route.
     * You must hold a state lock to call this function.
     *
     * @param classId class id of the object that triggered the update
     * @param uri URI of the object that that triggered the update
     * @param notifyRemoteRoutes set of remote route URIs that were updated
     * @param notifyLocalRoutes set of local route URIs that were updated
     */
    void updateRemoteRoute(opflex::modb::class_id_t classId,
                           const opflex::modb::URI& uri,
                           uri_set_t &notifyRemoteRoutes,
                           uri_set_t &notifyLocalRoutes);

    /**
     * Update the route cache information for the specified external network.
     * You must hold a state lock to call this function.
     *
     * @param uri URI of the external network that that triggered the update
     * @param notifyRemoteRoutes set of remote route URIs that were updated
     * @param notifyLocalRoutes set of local route URIs that were updated
     */
    void updateExternalNetworkPrefixes(const opflex::modb::URI& uri,
                                       uri_set_t &notifyRemoteRoutes,
                                       uri_set_t &notifyLocalRoutes);

    /**
     * Get the best policy prefix for the given prefix.
     *
     * @param rdURI routing domain URI
     * @param pfx route prefix
     * @param pfxLen route prefix length
     * @param newNet matching external network
     * @param nextExtSub matching external subnet
     */
    void getBestPolicyPrefix(
             const opflex::modb::URI& rdURI,
             const std::string &pfx,
             const uint32_t pfxLen,
             boost::optional<
                 std::shared_ptr<modelgbp::gbp::L3ExternalNetwork>> &newNet,
             boost::optional<
                 std::shared_ptr<modelgbp::gbp::ExternalSubnet>> &newExtSub);
    /**
     * Get the best remote route for the given prefix.
     *
     * @param rdURI routing domain URI
     * @param pfx route prefix
     * @param pfxLen route prefix length
     * @param newRemoteRt matching remote route
     */
    void getBestRemoteRoute(
             const opflex::modb::URI& rdURI,
             const std::string &pfx,
             const uint32_t pfxLen,
             boost::optional<opflex::modb::URI> &newRemoteRt);

    /**
     * Update route for policy prefix children of the route.
     *
     * @param rdUri routing domain URI
     * @param routeUri older remote route
     * @param pfx route prefix
     * @param pfxLen route prefix length
     * @param parentRemoteRt new remote route
     * @param notifyLocalRoutes URI set of local routes to be notified
     */
    void updatePolicyPrefixChildrenForRemoteRoute(
             const opflex::modb::URI& rdUri,
             const boost::optional<opflex::modb::URI> &routeUri,
             const std::string &pfx,
             const uint32_t pfxLen,
             const boost::optional<opflex::modb::URI> &parentRemoteRt,
             uri_set_t &notifyLocalRoutes);
    /**
     * Update policy prefix for remote route children of the prefix.
     *
     * @param rdURI routing domain URI
     * @param extNetURI older external network
     * @param extSubURI older external subnet
     * @param pfx route prefix
     * @param pfxLen route prefix length
     * @param newNet new external network
     * @param nextExtSub new external subnet
     */
    void updateRemoteRouteChildrenForPolicyPrefix(
             const opflex::modb::URI& rdURI,
             const opflex::modb::URI& extNetURI,
             const opflex::modb::URI& extSubURI,
             const std::string &pfx,
             const uint32_t  pfxLen,
             boost::optional<
                 std::shared_ptr<modelgbp::gbp::L3ExternalNetwork>> newNet,
             boost::optional<
                 std::shared_ptr<modelgbp::gbp::ExternalSubnet>> newExtSub,
             uri_set_t& notifyLocalRoutes);
    /**
     * Check to see if the local route can be deleted.
     *
     * @param localRoute  LocalRoute object
     * @return true if local route can be deleted, false otherwise
     */
    bool isLocalRouteDeletable(
             std::shared_ptr<modelgbp::epdr::LocalRoute> &localRoute);
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
