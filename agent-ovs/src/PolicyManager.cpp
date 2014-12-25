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

#include <algorithm>
#include <boost/foreach.hpp>
#include <opflex/modb/URIBuilder.h>

#include "logging.h"
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
    : framework(framework_), domainListener(*this), contractListener(*this) {

}

PolicyManager::~PolicyManager() {

}

void PolicyManager::start() {
    LOG(DEBUG) << "Starting policy manager";

    using namespace modelgbp;
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    BridgeDomain::registerListener(framework, &domainListener);
    FloodDomain::registerListener(framework, &domainListener);
    RoutingDomain::registerListener(framework, &domainListener);
    Subnets::registerListener(framework, &domainListener);
    EpGroup::registerListener(framework, &domainListener);

    EpGroup::registerListener(framework, &contractListener);
    Contract::registerListener(framework, &contractListener);
    Subject::registerListener(framework, &contractListener);
    Rule::registerListener(framework, &contractListener);
    L24Classifier::registerListener(framework, &contractListener);
}

void PolicyManager::stop() {
    LOG(DEBUG) << "Stopping policy manager";

    using namespace modelgbp;
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    BridgeDomain::unregisterListener(framework, &domainListener);
    FloodDomain::unregisterListener(framework, &domainListener);
    RoutingDomain::unregisterListener(framework, &domainListener);
    Subnets::unregisterListener(framework, &domainListener);
    EpGroup::unregisterListener(framework, &domainListener);

    EpGroup::unregisterListener(framework, &contractListener);
    Contract::unregisterListener(framework, &contractListener);
    Subject::unregisterListener(framework, &contractListener);
    Rule::unregisterListener(framework, &contractListener);
    L24Classifier::unregisterListener(framework, &contractListener);

    lock_guard<mutex> guard(state_mutex);
    group_map.clear();
    vnid_map.clear();
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
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.routingDomain;
}

optional<shared_ptr<modelgbp::gbp::BridgeDomain> >
PolicyManager::getBDForGroup(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.bridgeDomain;
}

optional<shared_ptr<modelgbp::gbp::FloodDomain> >
PolicyManager::getFDForGroup(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.floodDomain;
}

void PolicyManager::getSubnetsForGroup(const opflex::modb::URI& eg,
                                       /* out */ subnet_vector_t& subnets) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return;
    BOOST_FOREACH(const GroupState::subnet_map_t::value_type& v,
                  it->second.subnet_map) {
        subnets.push_back(v.second);
    }
}

void PolicyManager::updateSubnetIndex(const opflex::modb::URI& subnetsUri) {
    using namespace modelgbp;
    using namespace modelgbp::gbp;

    subnet_index_t::iterator oldit = subnet_index.find(subnetsUri);
    if (oldit != subnet_index.end()) {
        const opflex::modb::URI& refUri = oldit->second.refUri;
        BOOST_FOREACH(const shared_ptr<Subnet>& snet, oldit->second.childSubnets) {
            uri_ref_map_t::iterator rit = subnet_ref_map.find(refUri);
            if (rit != subnet_ref_map.end()) {
                unordered_set<opflex::modb::URI>& refmap = rit->second;
                refmap.erase(snet->getURI());
                if (refmap.size() == 0)
                    subnet_ref_map.erase(rit);
            }
        }
        subnet_index.erase(oldit);
    }

    optional<shared_ptr<Subnets> > subnets = 
        Subnets::resolve(framework, subnetsUri);
    if (!subnets) return;

    optional<shared_ptr<SubnetsToNetworkRSrc> > ref = 
        subnets.get()->resolveGbpSubnetsToNetworkRSrc();
    if (!ref) return;
    optional<URI> uri = ref.get()->getTargetURI();
    if (!uri) return;
    
    vector<shared_ptr<Subnet> > subnet_list;
    subnets.get()->resolveGbpSubnet(subnet_list);
    
    SubnetsCacheEntry& ce = subnet_index[subnetsUri];
    ce.refUri = uri.get();
    ce.childSubnets = subnet_list;

    BOOST_FOREACH(const shared_ptr<Subnet>& subnet, subnet_list) {
        subnet_ref_map[uri.get()].insert(subnet->getURI());
    }

}

bool PolicyManager::updateEPGDomains(const URI& egURI, bool& toRemove) {
    using namespace modelgbp;
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;

    GroupState& gs = group_map[egURI];

    optional<shared_ptr<EpGroup> > epg = 
        EpGroup::resolve(framework, egURI);
    if (!epg) {
        toRemove = true;
        return true;
    }
    toRemove = false;

    optional<shared_ptr<InstContext> > groupInstCtx;
    groupInstCtx = epg.get()->resolveGbpeInstContext();
    if (gs.vnid)
        vnid_map.erase(gs.vnid.get());
    if (groupInstCtx) {
        gs.vnid = groupInstCtx.get()->getEncapId();
        vnid_map.insert(std::make_pair(gs.vnid.get(), egURI));
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

boost::optional<uint32_t>
PolicyManager::getVnidForGroup(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    return it != group_map.end() ? it->second.vnid : boost::none;
}

boost::optional<opflex::modb::URI>
PolicyManager::getGroupForVnid(uint32_t vnid) {
    lock_guard<mutex> guard(state_mutex);
    vnid_map_t::iterator it = vnid_map.find(vnid);
    return it != vnid_map.end() ? optional<URI>(it->second) : boost::none;
}

bool PolicyManager::groupExists(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    return group_map.find(eg) != group_map.end();
}

void PolicyManager::getGroups(uri_set_t& epURIs) {
    lock_guard<mutex> guard(state_mutex);
    BOOST_FOREACH (const group_map_t::value_type& kv, group_map) {
        epURIs.insert(kv.first);
    }
}

void PolicyManager::notifyContract(const URI& contractURI) {
    lock_guard<mutex> guard(listener_mutex);
    BOOST_FOREACH(PolicyListener *listener, policyListeners) {
        listener->contractUpdated(contractURI);
    }
}

void PolicyManager::updateEPGContracts(const URI& egURI,
        uri_set_t& updatedContracts) {
    using namespace modelgbp::gbp;
    GroupContractState& gcs = groupContractMap[egURI];

    uri_set_t provAdded, provRemoved;
    uri_set_t consAdded, consRemoved;

    optional<shared_ptr<EpGroup> > epg =
        EpGroup::resolve(framework, egURI);
    if (!epg) {
        provRemoved.insert(gcs.contractsProvided.begin(),
                gcs.contractsProvided.end());
        consRemoved.insert(gcs.contractsConsumed.begin(),
                gcs.contractsConsumed.end());
        groupContractMap.erase(egURI);
    } else {
        uri_sorted_set_t newProvided;
        uri_sorted_set_t newConsumed;

        vector<shared_ptr<EpGroupToProvContractRSrc> > provRel;
        epg.get()->resolveGbpEpGroupToProvContractRSrc(provRel);
        vector<shared_ptr<EpGroupToConsContractRSrc> > consRel;
        epg.get()->resolveGbpEpGroupToConsContractRSrc(consRel);

        BOOST_FOREACH(shared_ptr<EpGroupToProvContractRSrc>& rel, provRel) {
            if (rel->isTargetSet()) {
                newProvided.insert(rel->getTargetURI().get());
            }
        }
        BOOST_FOREACH(shared_ptr<EpGroupToConsContractRSrc>& rel, consRel) {
            if (rel->isTargetSet()) {
                newConsumed.insert(rel->getTargetURI().get());
            }
        }

#define CALC_DIFF(olds, news, added, removed)                                  \
    std::set_difference(olds.begin(), olds.end(),                              \
            news.begin(), news.end(), inserter(removed, removed.begin()));     \
    std::set_difference(news.begin(), news.end(),                              \
            olds.begin(), olds.end(), inserter(added, added.begin()));

        CALC_DIFF(gcs.contractsProvided, newProvided, provAdded, provRemoved);
        CALC_DIFF(gcs.contractsConsumed, newConsumed, consAdded, consRemoved);
#undef CALC_DIFF
        gcs.contractsProvided.swap(newProvided);
        gcs.contractsConsumed.swap(newConsumed);
    }

#define INSERT_ALL(dst, src)    dst.insert(src.begin(), src.end());
    INSERT_ALL(updatedContracts, provAdded);
    INSERT_ALL(updatedContracts, provRemoved);
    INSERT_ALL(updatedContracts, consAdded);
    INSERT_ALL(updatedContracts, consRemoved);
#undef INSERT_ALL

    BOOST_FOREACH(const URI& u, provAdded) {
        contractMap[u].providerGroups.insert(egURI);
        LOG(DEBUG) << u << ": prov add: " << egURI;
    }
    BOOST_FOREACH(const URI& u, consAdded) {
        contractMap[u].consumerGroups.insert(egURI);
        LOG(DEBUG) << u << ": cons add: " << egURI;
    }
    BOOST_FOREACH(const URI& u, provRemoved) {
        contractMap[u].providerGroups.erase(egURI);
        LOG(DEBUG) << u << ": prov remove: " << egURI;
    }
    BOOST_FOREACH(const URI& u, consRemoved) {
        contractMap[u].consumerGroups.erase(egURI);
        LOG(DEBUG) << u << ": cons remove: " << egURI;
    }
}

/**
 * Check equality of L24Classifier objects.
 */
static bool
classifierEq(const shared_ptr<modelgbp::gbpe::L24Classifier>& lhs,
             const shared_ptr<modelgbp::gbpe::L24Classifier>& rhs) {
    if (lhs == rhs) {
        return true;
    }
    return lhs.get() && rhs.get() &&
        lhs->getURI() == rhs->getURI() &&
        lhs->getArpOpc() == rhs->getArpOpc() &&
        lhs->getConnectionTracking() == rhs->getConnectionTracking() &&
        lhs->getDFromPort() == rhs->getDFromPort() &&
        lhs->getDToPort() == rhs->getDToPort() &&
        lhs->getDirection() == rhs->getDirection() &&
        lhs->getEtherT() == rhs->getEtherT() &&
        lhs->getProt() == rhs->getProt() &&
        lhs->getSFromPort() == rhs->getSFromPort() &&
        lhs->getSToPort() == rhs->getSToPort();
}

bool PolicyManager::updateContractRules(const URI& contractURI,
        bool& notFound) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;

    optional<shared_ptr<Contract> > contract =
            Contract::resolve(framework, contractURI);
    if (!contract) {
        notFound = true;
        return false;
    }
    notFound = false;

    /* get all classifiers for this contract as an ordered-list */
    rule_list_t newRules;
    OrderComparator<shared_ptr<Rule> > ruleComp;
    OrderComparator<shared_ptr<L24Classifier> > classifierComp;
    vector<shared_ptr<Subject> > subjects;
    contract.get()->resolveGbpSubject(subjects);
    BOOST_FOREACH(shared_ptr<Subject>& sub, subjects) {
        vector<shared_ptr<Rule> > rules;
        sub->resolveGbpRule(rules);
        stable_sort(rules.begin(), rules.end(), ruleComp);

        BOOST_FOREACH(shared_ptr<Rule>& rule, rules) {
            vector<shared_ptr<L24Classifier> > classifiers;
            vector<shared_ptr<RuleToClassifierRSrc> > clsRel;
            rule->resolveGbpRuleToClassifierRSrc(clsRel);

            BOOST_FOREACH(shared_ptr<RuleToClassifierRSrc>& r, clsRel) {
                if (!r->isTargetSet()) {
                    continue;
                }
                optional<shared_ptr<L24Classifier> > cls =
                    L24Classifier::resolve(framework, r->getTargetURI().get());
                if (cls) {
                    classifiers.push_back(cls.get());
                }
            }
            stable_sort(classifiers.begin(), classifiers.end(), classifierComp);
            newRules.insert(newRules.end(), classifiers.begin(),
                    classifiers.end());
        }
    }
    ContractState& cs = contractMap[contractURI];

    rule_list_t::const_iterator li = cs.rules.begin();
    rule_list_t::const_iterator ri = newRules.begin();
    while (li != cs.rules.end() && ri != newRules.end() &&
           classifierEq(*li, *ri)) {
        ++li;
        ++ri;
    }
    bool updated = (li != cs.rules.end() || ri != newRules.end());
    if (updated) {
        cs.rules.swap(newRules);
        BOOST_FOREACH(shared_ptr<L24Classifier>& c, cs.rules) {
            LOG(DEBUG) << contractURI << " rule: " << c->getURI();
        }
    }
    return updated;
}

void PolicyManager::getContractProviders(const URI& contractURI,
                                         /* out */ uri_set_t& epgURIs) {
    lock_guard<mutex> guard(state_mutex);
    contract_map_t::const_iterator it = contractMap.find(contractURI);
    if (it != contractMap.end()) {
        epgURIs.insert(it->second.providerGroups.begin(),
                it->second.providerGroups.end());
    }
}

void PolicyManager::getContractConsumers(const URI& contractURI,
                                         /* out */ uri_set_t& epgURIs) {
    lock_guard<mutex> guard(state_mutex);
    contract_map_t::const_iterator it = contractMap.find(contractURI);
    if (it != contractMap.end()) {
        epgURIs.insert(it->second.consumerGroups.begin(),
                it->second.consumerGroups.end());
    }
}

void PolicyManager::getContractRules(const URI& contractURI,
                                     /* out */ rule_list_t& rules) {
    lock_guard<mutex> guard(state_mutex);
    contract_map_t::const_iterator it = contractMap.find(contractURI);
    if (it != contractMap.end()) {
        rules.insert(rules.end(), it->second.rules.begin(),
                it->second.rules.end());
    }
}

bool PolicyManager::contractExists(const opflex::modb::URI& cURI) {
    lock_guard<mutex> guard(state_mutex);
    return contractMap.find(cURI) != contractMap.end();
}

PolicyManager::DomainListener::DomainListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}
PolicyManager::DomainListener::~DomainListener() {}

void PolicyManager::DomainListener::objectUpdated(class_id_t class_id,
                                                  const URI& uri) {

    unique_lock<mutex> guard(pmanager.state_mutex);

    if (class_id == modelgbp::gbp::EpGroup::CLASS_ID) {
        pmanager.group_map[uri];
    } else if (class_id == modelgbp::gbp::Subnets::CLASS_ID) {
        pmanager.updateSubnetIndex(uri);
    }
    unordered_set<URI> notify;
    for (PolicyManager::group_map_t::iterator itr = pmanager.group_map.begin();
         itr != pmanager.group_map.end(); ) {
        bool toRemove = false;
        if (pmanager.updateEPGDomains(itr->first, toRemove)) {
            notify.insert(itr->first);
        }
        itr = (toRemove ? pmanager.group_map.erase(itr) : ++itr);
    }
    guard.unlock();
    BOOST_FOREACH(const URI& u, notify) {
        pmanager.notifyEPGDomain(u);
    }
}

PolicyManager::ContractListener::ContractListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}

PolicyManager::ContractListener::~ContractListener() {}

void PolicyManager::ContractListener::objectUpdated(class_id_t classId,
                                                    const URI& uri) {
    LOG(DEBUG) << "ContractListener update for URI " << uri;
    unique_lock<mutex> guard(pmanager.state_mutex);

    uri_set_t contractsToNotify;
    if (classId == modelgbp::gbp::EpGroup::CLASS_ID) {
        pmanager.updateEPGContracts(uri, contractsToNotify);
    } else {
        if (classId == modelgbp::gbp::Contract::CLASS_ID) {
            pmanager.contractMap[uri];
        }
        /* recompute the rules for all contracts if a policy object changed */
        for (PolicyManager::contract_map_t::iterator itr =
                pmanager.contractMap.begin();
             itr != pmanager.contractMap.end(); ) {
            bool notFound = false;
            if (pmanager.updateContractRules(itr->first, notFound)) {
                contractsToNotify.insert(itr->first);
            }
            /*
             * notFound == true may happen if the contract was removed or there
             * is a reference from a group to a contract that has not been
             * received yet. The URI given to the listener callback should
             * match the map entry in the first case.
             */
            if (notFound && itr->first == uri) {
                contractsToNotify.insert(itr->first);
                itr = pmanager.contractMap.erase(itr);
            } else {
                ++itr;
            }
        }
    }
    guard.unlock();

    BOOST_FOREACH(const URI& u, contractsToNotify) {
        pmanager.notifyContract(u);
    }
}

} /* namespace ovsagent */
