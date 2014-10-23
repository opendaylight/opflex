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
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/thread.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <modelgbp/metadata/metadata.hpp>
#include <modelgbp/dmtree/Root.hpp>

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
     * Get the subnet URIs for subnets that reference the given
     * network domain URI
     *
     * @param domainUri a URI referencing a network domain
     * @param uris the set of URIs that reference the domain
     */
    void getSubnetsForDomain(const opflex::modb::URI& domainUri,
                             /* out */ boost::unordered_set<opflex::modb::URI>& uris);

private:
    opflex::ofcore::OFFramework& framework;

    /**
     * State and indices related to a given endpoint group
     */
    struct GroupState {
        boost::shared_ptr<modelgbp::gbp::RoutingDomain> routingDomain;
        boost::shared_ptr<modelgbp::gbp::BridgeDomain> bridgeDomain;
        boost::shared_ptr<modelgbp::gbp::FloodDomain> floodDomain;
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
     * Update the subnet URI map 
     */
    void updateSubnetIndex();

    /**
     * Update the EPG domain cache information for the specified EPG
     * URI
     */
    void updateEPGDomains(const opflex::modb::URI& uri);
};

} /* namespace ovsagent */

#endif /* OVSAGENT_POLICYMANAGER_H */
