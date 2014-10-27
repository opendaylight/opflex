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
#include <opflex/modb/URIBuilder.h>

#include "PolicyManager.h"

namespace ovsagent {


using std::vector;
using std::string;
using opflex::ofcore::OFFramework;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::URIBuilder;
using boost::unique_lock;
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
    EpGroup::registerListener(framework, &domainListener);
}

void PolicyManager::stop() {
    using namespace modelgbp;
    using namespace modelgbp::gbp;
    BridgeDomain::unregisterListener(framework, &domainListener);
    FloodDomain::unregisterListener(framework, &domainListener);
    RoutingDomain::unregisterListener(framework, &domainListener);
    Subnets::unregisterListener(framework, &domainListener);
    EpGroup::unregisterListener(framework, &domainListener);

    lock_guard<mutex> guard(state_mutex);
    group_map.clear();
    subnet_ref_map.clear();
}

void PolicyManager::registerListener(PolicyListener* listener) {
    lock_guard<mutex> guard(listener_mutex);
    policyListeners.push_back(listener);
}

void PolicyManager::unregisterListener(PolicyListener* listener) {
    lock_guard<mutex> guard(listener_mutex);
    policyListeners.remove(listener);
}

void PolicyManager::notifyEPGDomain(const URI& egURI) {
    lock_guard<mutex> guard(listener_mutex);
    BOOST_FOREACH(PolicyListener* listener, policyListeners) {
        listener->egDomainUpdated(egURI);
    }
}

void PolicyManager::getSubnetsForDomain(const URI& domainUri,
                                        /* out */ unordered_set<URI>& uris) {
    lock_guard<mutex> guard(state_mutex);
    uri_ref_map_t::const_iterator it = subnet_ref_map.find(domainUri);
    if (it != subnet_ref_map.end()) {
        uris.insert(it->second.begin(), it->second.end());
    }
}

optional<shared_ptr<modelgbp::gbp::RoutingDomain> >
PolicyManager::getRDForGroup(const opflex::modb::URI& eg) {
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.routingDomain;
}

optional<shared_ptr<modelgbp::gbp::BridgeDomain> >
PolicyManager::getBDForGroup(const opflex::modb::URI& eg) {
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.bridgeDomain;
}

optional<shared_ptr<modelgbp::gbp::FloodDomain> >
PolicyManager::getFDForGroup(const opflex::modb::URI& eg) {
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.floodDomain;
}

void PolicyManager::getSubnetsForGroup(const opflex::modb::URI& eg,
                                       /* out */ subnet_vector_t& subnets) {
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return;
    BOOST_FOREACH(const GroupState::subnet_map_t::value_type& v,
                  it->second.subnet_map) {
        subnets.push_back(v.second);
    }
}

void PolicyManager::updateSubnetIndex() {
    using namespace modelgbp;
    using namespace modelgbp::gbp;

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

bool PolicyManager::updateEPGDomains(const URI& egURI) {
    using namespace modelgbp;
    using namespace modelgbp::gbp;

    GroupState& gs = group_map[egURI];

    optional<shared_ptr<EpGroup> > epg = 
        EpGroup::resolve(framework, egURI);
    if (!epg) {
        group_map.erase(egURI);
        return true;
    }

    optional<shared_ptr<RoutingDomain> > newrd;
    optional<shared_ptr<BridgeDomain> > newbd;
    optional<shared_ptr<FloodDomain> > newfd;
    GroupState::subnet_map_t newsmap;

    optional<class_id_t> domainClass;
    optional<URI> domainURI;
    optional<shared_ptr<EpGroupToNetworkRSrc> > ref = 
        epg.get()->resolveGbpEpGroupToNetworkRSrc();
    if (ref) {
        domainClass = ref.get()->getTargetClass();
        domainURI = ref.get()->getTargetURI();
    }

    // walk up the chain of domains
    while (domainURI && domainClass) {
        URI du = domainURI.get();
        optional<class_id_t> ndomainClass;
        optional<URI> ndomainURI;

        // Update the subnet map with all the subnets that reference
        // this domain
        uri_ref_map_t::const_iterator it =
            subnet_ref_map.find(du);
        if (it != subnet_ref_map.end()) {
            BOOST_FOREACH(const URI& ru, it->second) {
                optional<shared_ptr<Subnet> > subnet = 
                    Subnet::resolve(framework, ru);
                if (subnet)
                    newsmap[ru] = subnet.get();
            }
        }

        switch (domainClass.get()) {
        case RoutingDomain::CLASS_ID:
            {
                newrd = RoutingDomain::resolve(framework, du);
                ndomainClass = boost::none;
                ndomainURI = boost::none;
            }
            break;
        case BridgeDomain::CLASS_ID:
            {
                newbd = BridgeDomain::resolve(framework, du);
                if (newbd) {
                    optional<shared_ptr<BridgeDomainToNetworkRSrc> > dref = 
                        newbd.get()->resolveGbpBridgeDomainToNetworkRSrc();
                    if (dref) {
                        ndomainClass = dref.get()->getTargetClass();
                        ndomainURI = dref.get()->getTargetURI();
                    }
                }
            }
            break;
        case FloodDomain::CLASS_ID:
            {
                newfd = FloodDomain::resolve(framework, du);
                if (newfd) {
                    optional<shared_ptr<FloodDomainToNetworkRSrc> > dref = 
                        newfd.get()->resolveGbpFloodDomainToNetworkRSrc();
                    if (dref) {
                        ndomainClass = dref.get()->getTargetClass();
                        ndomainURI = dref.get()->getTargetURI();
                    }
                }
            }
            break;
        case Subnets::CLASS_ID:
            {
                optional<shared_ptr<Subnets> > subnets =
                    Subnets::resolve(framework, du);
                if (subnets) {
                    optional<shared_ptr<SubnetsToNetworkRSrc> > dref = 
                        subnets.get()->resolveGbpSubnetsToNetworkRSrc();
                    if (dref) {
                        ndomainClass = dref.get()->getTargetClass();
                        ndomainURI = dref.get()->getTargetURI();
                    }
                }
            }
            break;
        case Subnet::CLASS_ID:
            {
                optional<shared_ptr<Subnet> > subnet = 
                    Subnet::resolve(framework, du);
                if (subnet) {
                    // XXX - TODO - I'd like to not have to do this.
                    // should add a resolve parent to modb or make it
                    // so epgs can't reference an individual parent.
                    vector<string> elements;
                    du.getElements(elements);
                    URIBuilder ub;
                    // strip last 2 elements off subnet URI
                    for (int i = 0; i < elements.size()-2; ++i) {
                        ub.addElement(elements[i]);
                    }
                    class_id_t scid = Subnets::CLASS_ID;
                    ndomainClass = scid;
                    ndomainURI = ub.build();
                }
            }
            break;
        }
        
        domainClass = ndomainClass;
        domainURI = ndomainURI;
    }

    bool updated = false;
    if (newfd != gs.floodDomain ||
        newbd != gs.bridgeDomain ||
        newrd != gs.routingDomain ||
        newsmap != gs.subnet_map)
        updated = true;
    
    gs.floodDomain = newfd;
    gs.bridgeDomain = newbd;
    gs.routingDomain = newrd;
    gs.subnet_map = newsmap;

    return updated;
}

PolicyManager::DomainListener::DomainListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}
PolicyManager::DomainListener::~DomainListener() {}

void PolicyManager::DomainListener::objectUpdated(class_id_t class_id,
                                                  const URI& uri) {

    unique_lock<mutex> guard(pmanager.state_mutex);
    pmanager.updateSubnetIndex();

    if (class_id == modelgbp::gbp::EpGroup::CLASS_ID) {
        pmanager.group_map[uri];
    }
    unordered_set<URI> notify;
    BOOST_FOREACH(const group_map_t::value_type& v, pmanager.group_map) {
        if (pmanager.updateEPGDomains(v.first))
            notify.insert(v.first);
    }
    guard.unlock();
    BOOST_FOREACH(const URI& u, notify) {
        pmanager.notifyEPGDomain(u);
    }
}

} /* namespace ovsagent */
