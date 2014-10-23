/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PolicyManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>

#include "PolicyManager.h"

namespace ovsagent {

using std::vector;
using opflex::ofcore::OFFramework;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using boost::lock_guard;
using boost::mutex;
using boost::shared_ptr;
using boost::optional;
using boost::unordered_set;

PolicyManager::PolicyManager(OFFramework& framework_)
    : framework(framework_), domainListener(*this) {

}

PolicyManager::~PolicyManager() {

}

void PolicyManager::start() {
    using namespace modelgbp;
    using namespace modelgbp::gbp;
    BridgeDomain::registerListener(framework, &domainListener);
    FloodDomain::registerListener(framework, &domainListener);
    RoutingDomain::registerListener(framework, &domainListener);
    Subnets::registerListener(framework, &domainListener);
}

void PolicyManager::stop() {
    using namespace modelgbp;
    using namespace modelgbp::gbp;
    BridgeDomain::unregisterListener(framework, &domainListener);
    FloodDomain::unregisterListener(framework, &domainListener);
    RoutingDomain::unregisterListener(framework, &domainListener);
    Subnets::unregisterListener(framework, &domainListener);
}

void PolicyManager::getSubnetsForDomain(const URI& domainUri,
                                        /* out */ unordered_set<URI>& uris) {
    lock_guard<mutex> guard(state_mutex);
    uri_ref_map_t::const_iterator it = subnet_ref_map.find(domainUri);
    if (it != subnet_ref_map.end()) {
        uris.insert(it->second.begin(), it->second.end());
    }
}

void PolicyManager::updateSubnetIndex() {
    using namespace modelgbp;
    using namespace modelgbp::gbp;

    lock_guard<mutex> guard(state_mutex);
    subnet_ref_map.clear();

    optional<shared_ptr<policy::Universe> > universe = 
        policy::Universe::resolve(framework);
    if (!universe) return;

    vector<shared_ptr<policy::Space> > space_list;
    universe.get()->resolvePolicySpace(space_list);
    BOOST_FOREACH(const shared_ptr<policy::Space>& space, space_list) {
        vector<shared_ptr<Subnets> > subnets_list;
        space->resolveGbpSubnets(subnets_list);

        BOOST_FOREACH(const shared_ptr<Subnets>& subnets, subnets_list) {
            optional<shared_ptr<SubnetsToNetworkRSrc> > ref = 
                subnets->resolveGbpSubnetsToNetworkRSrc();
            if (!ref) continue;
            optional<URI> uri = ref.get()->getTargetURI();
            if (!uri) continue;

            vector<shared_ptr<Subnet> > subnet_list;
            subnets->resolveGbpSubnet(subnet_list);

            BOOST_FOREACH(const shared_ptr<Subnet>& subnet, subnet_list) {
                subnet_ref_map[uri.get()].insert(subnet->getURI());
            }            
        }
    }
}

void PolicyManager::updateEPGDomains(const opflex::modb::URI& uri) {
    lock_guard<mutex> guard(state_mutex);
}

PolicyManager::DomainListener::DomainListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}
PolicyManager::DomainListener::~DomainListener() {}

void PolicyManager::DomainListener::objectUpdated(class_id_t class_id,
                                                  const URI& uri) {
    pmanager.updateSubnetIndex();
}

} /* namespace ovsagent */
