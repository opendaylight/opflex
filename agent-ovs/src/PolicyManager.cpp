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
#include "Packets.h"

namespace ovsagent {

using std::vector;
using std::string;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::URIBuilder;
using boost::unique_lock;
using boost::lock_guard;
using boost::mutex;
using boost::shared_ptr;
using boost::make_shared;
using boost::optional;
using boost::unordered_set;
using boost::asio::ip::address;

PolicyManager::PolicyManager(OFFramework& framework_)
    : framework(framework_), opflexDomain("default"),
      domainListener(*this), contractListener(*this),
      configListener(*this) {

}

PolicyManager::~PolicyManager() {

}

void PolicyManager::start() {
    LOG(DEBUG) << "Starting policy manager";

    using namespace modelgbp;
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;

    platform::Config::registerListener(framework, &configListener);

    BridgeDomain::registerListener(framework, &domainListener);
    FloodDomain::registerListener(framework, &domainListener);
    FloodContext::registerListener(framework, &domainListener);
    RoutingDomain::registerListener(framework, &domainListener);
    Subnets::registerListener(framework, &domainListener);
    Subnet::registerListener(framework, &domainListener);
    InstContext::registerListener(framework, &domainListener);
    EpGroup::registerListener(framework, &domainListener);
    L3ExternalNetwork::registerListener(framework, &domainListener);

    EpGroup::registerListener(framework, &contractListener);
    L3ExternalNetwork::registerListener(framework, &contractListener);
    RoutingDomain::registerListener(framework, &contractListener);
    Contract::registerListener(framework, &contractListener);
    Subject::registerListener(framework, &contractListener);
    Rule::registerListener(framework, &contractListener);
    L24Classifier::registerListener(framework, &contractListener);

    // resolve platform config
    Mutator mutator(framework, "init");
    optional<shared_ptr<dmtree::Root> > root(dmtree::Root::resolve(URI::ROOT));
    if (root)
        root.get()->addDomainConfig()
            ->addDomainConfigToConfigRSrc()
            ->setTargetConfig(opflexDomain);
    mutator.commit();
}

void PolicyManager::stop() {
    LOG(DEBUG) << "Stopping policy manager";

    using namespace modelgbp;
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    BridgeDomain::unregisterListener(framework, &domainListener);
    FloodDomain::unregisterListener(framework, &domainListener);
    FloodContext::unregisterListener(framework, &domainListener);
    RoutingDomain::unregisterListener(framework, &domainListener);
    Subnets::unregisterListener(framework, &domainListener);
    Subnet::unregisterListener(framework, &domainListener);
    InstContext::unregisterListener(framework, &domainListener);
    EpGroup::unregisterListener(framework, &domainListener);
    L3ExternalNetwork::unregisterListener(framework, &domainListener);

    EpGroup::unregisterListener(framework, &contractListener);
    L3ExternalNetwork::unregisterListener(framework, &contractListener);
    RoutingDomain::unregisterListener(framework, &contractListener);
    Contract::unregisterListener(framework, &contractListener);
    Subject::unregisterListener(framework, &contractListener);
    Rule::unregisterListener(framework, &contractListener);
    L24Classifier::unregisterListener(framework, &contractListener);

    lock_guard<mutex> guard(state_mutex);
    group_map.clear();
    vnid_map.clear();
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

void PolicyManager::notifyDomain(class_id_t cid, const URI& domURI) {
    lock_guard<mutex> guard(listener_mutex);
    BOOST_FOREACH(PolicyListener* listener, policyListeners) {
        listener->domainUpdated(cid, domURI);
    }
}

void PolicyManager::notifyContract(const URI& contractURI) {
    lock_guard<mutex> guard(listener_mutex);
    BOOST_FOREACH(PolicyListener *listener, policyListeners) {
        listener->contractUpdated(contractURI);
    }
}

void PolicyManager::notifyConfig(const URI& configURI) {
    lock_guard<mutex> guard(listener_mutex);
    BOOST_FOREACH(PolicyListener *listener, policyListeners) {
        listener->configUpdated(configURI);
    }
}

optional<shared_ptr<modelgbp::gbp::RoutingDomain> >
PolicyManager::getRDForGroup(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.routingDomain;
}

optional<shared_ptr<modelgbp::gbp::RoutingDomain> >
PolicyManager::getRDForL3ExtNet(const opflex::modb::URI& l3n) {
    lock_guard<mutex> guard(state_mutex);
    l3n_map_t::iterator it = l3n_map.find(l3n);
    if (it == l3n_map.end()) return boost::none;
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

optional<shared_ptr<modelgbp::gbpe::FloodContext> >
PolicyManager::getFloodContextForGroup(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    return it->second.floodContext;
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

optional<shared_ptr<modelgbp::gbp::Subnet> >
PolicyManager::findSubnetForEp(const opflex::modb::URI& eg,
                                const address& ip) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    if (it == group_map.end()) return boost::none;
    boost::system::error_code ec;
    BOOST_FOREACH(const GroupState::subnet_map_t::value_type& v,
                  it->second.subnet_map) {
        if (!v.second->isAddressSet() || !v.second->isPrefixLenSet())
            continue;
        address netAddr =
            address::from_string(v.second->getAddress().get(), ec);
        uint8_t prefixLen = v.second->getPrefixLen().get();

        if (netAddr.is_v4() != ip.is_v4()) continue;

        if (netAddr.is_v4()) {
            if (prefixLen > 32) prefixLen = 32;

            uint32_t mask = (prefixLen != 0)
                ? (~((uint32_t)0) << (32 - prefixLen))
                : 0;
            uint32_t net_addr = netAddr.to_v4().to_ulong() & mask;
            uint32_t ip_addr = ip.to_v4().to_ulong() & mask;

            if (net_addr == ip_addr)
                return v.second;
        } else {
            if (prefixLen > 128) prefixLen = 128;

            struct in6_addr mask;
            struct in6_addr net_addr;
            struct in6_addr ip_addr;
            memcpy(&ip_addr, ip.to_v6().to_bytes().data(), sizeof(ip_addr));
            packets::compute_ipv6_subnet(netAddr.to_v6(), prefixLen,
                                         &mask, &net_addr);

            ((uint64_t*)&ip_addr)[0] &= ((uint64_t*)&mask)[0];
            ((uint64_t*)&ip_addr)[1] &= ((uint64_t*)&mask)[1];

            if (((uint64_t*)&ip_addr)[0] == ((uint64_t*)&net_addr)[0] &&
                ((uint64_t*)&ip_addr)[1] == ((uint64_t*)&net_addr)[1])
                return v.second;
        }
    }
    return boost::none;
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

    optional<shared_ptr<InstContext> > newInstCtx =
        epg.get()->resolveGbpeInstContext();
    if (gs.instContext && gs.instContext.get()->getEncapId()) {
        vnid_map.erase(gs.instContext.get()->getEncapId().get());
    }
    if (newInstCtx && newInstCtx.get()->getEncapId()) {
        vnid_map.insert(
            std::make_pair(newInstCtx.get()->getEncapId().get(), egURI));
    }

    optional<shared_ptr<RoutingDomain> > newrd;
    optional<shared_ptr<BridgeDomain> > newbd;
    optional<shared_ptr<FloodDomain> > newfd;
    optional<shared_ptr<FloodContext> > newfdctx;
    GroupState::subnet_map_t newsmap;

    optional<class_id_t> domainClass;
    optional<URI> domainURI;
    optional<shared_ptr<EpGroupToNetworkRSrc> > ref =
        epg.get()->resolveGbpEpGroupToNetworkRSrc();
    if (ref) {
        domainClass = ref.get()->getTargetClass();
        domainURI = ref.get()->getTargetURI();
    }

    // Update the subnet map for the group with the subnets directly
    // referenced by the group.
    optional<shared_ptr<EpGroupToSubnetsRSrc> > egSns =
        epg.get()->resolveGbpEpGroupToSubnetsRSrc();
    if (egSns && egSns.get()->isTargetSet()) {
        optional<shared_ptr<Subnets> > sns =
            Subnets::resolve(framework,
                             egSns.get()->getTargetURI().get());
        if (sns) {
            vector<shared_ptr<Subnet> > csns;
            sns.get()->resolveGbpSubnet(csns);
            BOOST_FOREACH(shared_ptr<Subnet>& csn, csns)
                newsmap[csn->getURI()] = csn;
        }
    }

    // walk up the chain of forwarding domains
    while (domainURI && domainClass) {
        URI du = domainURI.get();
        optional<class_id_t> ndomainClass;
        optional<URI> ndomainURI;

        optional<shared_ptr<ForwardingBehavioralGroupToSubnetsRSrc> > fwdSns;
        switch (domainClass.get()) {
        case RoutingDomain::CLASS_ID:
            {
                newrd = RoutingDomain::resolve(framework, du);
                ndomainClass = boost::none;
                ndomainURI = boost::none;
                if (newrd) {
                    fwdSns = newrd.get()->
                        resolveGbpForwardingBehavioralGroupToSubnetsRSrc();
                }
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
                    fwdSns = newbd.get()->
                        resolveGbpForwardingBehavioralGroupToSubnetsRSrc();
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
                    newfdctx = newfd.get()->resolveGbpeFloodContext();
                    fwdSns = newfd.get()->
                        resolveGbpForwardingBehavioralGroupToSubnetsRSrc();
                }
            }
            break;
        }

        // Update the subnet map for the group with all the subnets it
        // could access.
        if (fwdSns && fwdSns.get()->isTargetSet()) {
            optional<shared_ptr<Subnets> > sns =
                Subnets::resolve(framework,
                                 fwdSns.get()->getTargetURI().get());
            if (sns) {
                vector<shared_ptr<Subnet> > csns;
                sns.get()->resolveGbpSubnet(csns);
                BOOST_FOREACH(shared_ptr<Subnet>& csn, csns)
                    newsmap[csn->getURI()] = csn;
            }
        }

        domainClass = ndomainClass;
        domainURI = ndomainURI;
    }

    bool updated = false;
    if (newInstCtx != gs.instContext ||
        newfd != gs.floodDomain ||
        newfdctx != gs.floodContext ||
        newbd != gs.bridgeDomain ||
        newrd != gs.routingDomain ||
        newsmap != gs.subnet_map)
        updated = true;

    gs.instContext = newInstCtx;
    gs.floodDomain = newfd;
    gs.floodContext = newfdctx;
    gs.bridgeDomain = newbd;
    gs.routingDomain = newrd;
    gs.subnet_map = newsmap;

    return updated;
}

boost::optional<uint32_t>
PolicyManager::getVnidForGroup(const opflex::modb::URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    return it != group_map.end() && it->second.instContext &&
           it->second.instContext.get()->getEncapId()
           ? it->second.instContext.get()->getEncapId().get()
           : optional<uint32_t>();
}

boost::optional<opflex::modb::URI>
PolicyManager::getGroupForVnid(uint32_t vnid) {
    lock_guard<mutex> guard(state_mutex);
    vnid_map_t::iterator it = vnid_map.find(vnid);
    return it != vnid_map.end() ? optional<URI>(it->second) : boost::none;
}

optional<string> PolicyManager::getMulticastIPForGroup(const URI& eg) {
    lock_guard<mutex> guard(state_mutex);
    group_map_t::iterator it = group_map.find(eg);
    return it != group_map.end() && it->second.instContext &&
           it->second.instContext.get()->getMulticastGroupIP()
           ? it->second.instContext.get()->getMulticastGroupIP().get()
           : optional<string>();
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

void PolicyManager::getRoutingDomains(uri_set_t& rdURIs) {
    lock_guard<mutex> guard(state_mutex);
    BOOST_FOREACH (const rd_map_t::value_type& kv, rd_map) {
        rdURIs.insert(kv.first);
    }
}

void PolicyManager::updateGroupContracts(class_id_t groupType,
                                         const URI& groupURI,
                                         uri_set_t& updatedContracts) {
    using namespace modelgbp::gbp;
    GroupContractState& gcs = groupContractMap[groupURI];

    uri_set_t provAdded, provRemoved;
    uri_set_t consAdded, consRemoved;

    uri_sorted_set_t newProvided;
    uri_sorted_set_t newConsumed;

    bool remove = true;
    if (groupType == EpGroup::CLASS_ID) {
        optional<shared_ptr<EpGroup> > epg =
            EpGroup::resolve(framework, groupURI);
        if (epg) {
            remove = false;
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
        }
    } else if (groupType == L3ExternalNetwork::CLASS_ID) {
        optional<shared_ptr<L3ExternalNetwork> > l3n =
            L3ExternalNetwork::resolve(framework, groupURI);
        if (l3n) {
            remove = false;
            vector<shared_ptr<L3ExternalNetworkToProvContractRSrc> > provRel;
            l3n.get()->resolveGbpL3ExternalNetworkToProvContractRSrc(provRel);
            vector<shared_ptr<L3ExternalNetworkToConsContractRSrc> > consRel;
            l3n.get()->resolveGbpL3ExternalNetworkToConsContractRSrc(consRel);

            BOOST_FOREACH(shared_ptr<L3ExternalNetworkToProvContractRSrc>& rel,
                          provRel) {
                if (rel->isTargetSet()) {
                    newProvided.insert(rel->getTargetURI().get());
                }
            }
            BOOST_FOREACH(shared_ptr<L3ExternalNetworkToConsContractRSrc>& rel,
                          consRel) {
                if (rel->isTargetSet()) {
                    newConsumed.insert(rel->getTargetURI().get());
                }
            }
        }
    }
    if (remove) {
        provRemoved.insert(gcs.contractsProvided.begin(),
                gcs.contractsProvided.end());
        consRemoved.insert(gcs.contractsConsumed.begin(),
                gcs.contractsConsumed.end());
        groupContractMap.erase(groupURI);
    } else {

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
        contractMap[u].providerGroups.insert(groupURI);
        LOG(DEBUG) << u << ": prov add: " << groupURI;
    }
    BOOST_FOREACH(const URI& u, consAdded) {
        contractMap[u].consumerGroups.insert(groupURI);
        LOG(DEBUG) << u << ": cons add: " << groupURI;
    }
    BOOST_FOREACH(const URI& u, provRemoved) {
        contractMap[u].providerGroups.erase(groupURI);
        LOG(DEBUG) << u << ": prov remove: " << groupURI;
    }
    BOOST_FOREACH(const URI& u, consRemoved) {
        contractMap[u].consumerGroups.erase(groupURI);
        LOG(DEBUG) << u << ": cons remove: " << groupURI;
    }
}

/**
 * Check equality of L24Classifier objects.
 */
static bool
ruleEq(const shared_ptr<PolicyRule>& lhsObj,
       const shared_ptr<PolicyRule>& rhsObj) {
    using namespace modelgbp::gbpe;
    if (lhsObj == rhsObj) {
        return true;
    }
    if (lhsObj.get() == NULL || rhsObj.get() == NULL) {
        return false;
    }
    const shared_ptr<L24Classifier>& lhs = lhsObj->getL24Classifier();
    const shared_ptr<L24Classifier>& rhs = rhsObj->getL24Classifier();

    return lhsObj->getDirection() == rhsObj->getDirection() &&
        lhsObj->getAllow() == rhsObj->getAllow() &&
        lhs.get() && rhs.get() &&
        lhs->getURI() == rhs->getURI() &&
        lhs->getArpOpc() == rhs->getArpOpc() &&
        lhs->getConnectionTracking() == rhs->getConnectionTracking() &&
        lhs->getDFromPort() == rhs->getDFromPort() &&
        lhs->getDToPort() == rhs->getDToPort() &&
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
            if (!rule->isDirectionSet()) {
                continue;       // ignore rules with no direction
            }
            uint8_t dir = rule->getDirection().get();
            vector<shared_ptr<L24Classifier> > classifiers;
            vector<shared_ptr<RuleToClassifierRSrc> > clsRel;
            rule->resolveGbpRuleToClassifierRSrc(clsRel);

            BOOST_FOREACH(shared_ptr<RuleToClassifierRSrc>& r, clsRel) {
                if (!r->isTargetSet() ||
                    r->getTargetClass().get() != L24Classifier::CLASS_ID) {
                    continue;
                }
                optional<shared_ptr<L24Classifier> > cls =
                    L24Classifier::resolve(framework, r->getTargetURI().get());
                if (cls) {
                    classifiers.push_back(cls.get());
                }
            }
            stable_sort(classifiers.begin(), classifiers.end(), classifierComp);

            vector<shared_ptr<RuleToActionRSrc> > actRel;
            rule->resolveGbpRuleToActionRSrc(actRel);
            bool ruleAllow = true;
            uint32_t minOrder = UINT32_MAX;
            BOOST_FOREACH(shared_ptr<RuleToActionRSrc>& r, actRel) {
                if (!r->isTargetSet() ||
                    r->getTargetClass().get() != AllowDenyAction::CLASS_ID) {
                    continue;
                }
                optional<shared_ptr<AllowDenyAction> > act =
                    AllowDenyAction::resolve(framework, r->getTargetURI().get());
                if (act) {
                    if (act.get()->getOrder(UINT32_MAX-1) < minOrder) {
                        minOrder = act.get()->getOrder(UINT32_MAX-1);
                        ruleAllow = act.get()->getAllow(0) != 0;
                    }
                }
            }

            BOOST_FOREACH (const shared_ptr<L24Classifier>& c, classifiers) {
                newRules.push_back(make_shared<PolicyRule>(dir, c, ruleAllow));
            }
        }
    }
    ContractState& cs = contractMap[contractURI];

    rule_list_t::const_iterator li = cs.rules.begin();
    rule_list_t::const_iterator ri = newRules.begin();
    while (li != cs.rules.end() && ri != newRules.end() &&
           ruleEq(*li, *ri)) {
        ++li;
        ++ri;
    }
    bool updated = (li != cs.rules.end() || ri != newRules.end());
    if (updated) {
        cs.rules.swap(newRules);
        BOOST_FOREACH(shared_ptr<PolicyRule>& c, cs.rules) {
            LOG(DEBUG) << contractURI << " rule: "
                << c->getL24Classifier()->getURI();
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

void PolicyManager::getContractsForGroup(const URI& eg,
                                         /* out */ uri_set_t& contractURIs) {
    using namespace modelgbp::gbp;
    optional<shared_ptr<EpGroup> > epg = EpGroup::resolve(framework, eg);
    if (!epg) return;

    vector<shared_ptr<EpGroupToProvContractRSrc> > provRel;
    epg.get()->resolveGbpEpGroupToProvContractRSrc(provRel);
    vector<shared_ptr<EpGroupToConsContractRSrc> > consRel;
    epg.get()->resolveGbpEpGroupToConsContractRSrc(consRel);

    BOOST_FOREACH(shared_ptr<EpGroupToProvContractRSrc>& rel, provRel) {
        if (rel->isTargetSet()) {
            contractURIs.insert(rel->getTargetURI().get());
        }
    }
    BOOST_FOREACH(shared_ptr<EpGroupToConsContractRSrc>& rel, consRel) {
        if (rel->isTargetSet()) {
            contractURIs.insert(rel->getTargetURI().get());
        }
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

void PolicyManager::updateL3Nets(const opflex::modb::URI& rdURI,
                                 uri_set_t& contractsToNotify) {
    using namespace modelgbp::gbp;
    RoutingDomainState& rds = rd_map[rdURI];
    optional<shared_ptr<RoutingDomain > > rd =
        RoutingDomain::resolve(framework, rdURI);

    if (rd) {
        vector<shared_ptr<L3ExternalDomain> > extDoms;
        vector<shared_ptr<L3ExternalNetwork> > extNets;
        rd.get()->resolveGbpL3ExternalDomain(extDoms);
        BOOST_FOREACH(shared_ptr<L3ExternalDomain>& extDom, extDoms) {
            extDom->resolveGbpL3ExternalNetwork(extNets);
        }

        unordered_set<URI> newNets;
        BOOST_FOREACH(shared_ptr<L3ExternalNetwork> net, extNets) {
            newNets.insert(net->getURI());

            L3NetworkState& l3s = l3n_map[net->getURI()];
            if (l3s.routingDomain && l3s.natEpg) {
                uri_ref_map_t::iterator it =
                    nat_epg_l3_ext.find(l3s.natEpg.get());
                if (it != nat_epg_l3_ext.end()) {
                    it->second.erase(net->getURI());
                    if (it->second.size() == 0)
                        nat_epg_l3_ext.erase(it);
                }
            }

            l3s.routingDomain = rd;

            optional<shared_ptr<L3ExternalNetworkToNatEPGroupRSrc> > natRef =
                net->resolveGbpL3ExternalNetworkToNatEPGroupRSrc();
            if (natRef) {
                optional<URI> natEpg = natRef.get()->getTargetURI();
                if (natEpg) {
                    l3s.natEpg = natEpg.get();
                    uri_set_t& s = nat_epg_l3_ext[l3s.natEpg.get()];
                    s.insert(net->getURI());
                }
            } else {
                l3s.natEpg = boost::none;
            }

            updateGroupContracts(L3ExternalNetwork::CLASS_ID,
                                 net->getURI(), contractsToNotify);
        }
        BOOST_FOREACH(const URI& net, rds.extNets) {
            if (newNets.find(net) == newNets.end()) {
                l3n_map_t::iterator lit = l3n_map.find(net);
                if (lit != l3n_map.end()) {
                    if (lit->second.natEpg) {
                        uri_ref_map_t::iterator git =
                            nat_epg_l3_ext.find(lit->second.natEpg.get());
                        if (git != nat_epg_l3_ext.end()) {
                            git->second.erase(net);
                            if (git->second.size() == 0)
                                nat_epg_l3_ext.erase(git);
                        }
                    }
                }

                l3n_map.erase(lit);

                updateGroupContracts(L3ExternalNetwork::CLASS_ID,
                                     net, contractsToNotify);
            }
        }
        rds.extNets = newNets;
    } else {
        BOOST_FOREACH(const URI& net, rds.extNets) {
            l3n_map.erase(net);
            updateGroupContracts(L3ExternalNetwork::CLASS_ID,
                                 net, contractsToNotify);
        }
        rd_map.erase(rdURI);
    }
}

PolicyManager::DomainListener::DomainListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}
PolicyManager::DomainListener::~DomainListener() {}

void PolicyManager::DomainListener::objectUpdated(class_id_t class_id,
                                                  const URI& uri) {
    using namespace modelgbp::gbp;
    unique_lock<mutex> guard(pmanager.state_mutex);

    uri_set_t notifyGroups;
    uri_set_t notifyRds;

    if (class_id == modelgbp::gbp::EpGroup::CLASS_ID) {
        pmanager.group_map[uri];
    }
    for (PolicyManager::group_map_t::iterator itr = pmanager.group_map.begin();
         itr != pmanager.group_map.end(); ) {
        bool toRemove = false;
        if (pmanager.updateEPGDomains(itr->first, toRemove)) {
            notifyGroups.insert(itr->first);
        }
        itr = (toRemove ? pmanager.group_map.erase(itr) : ++itr);
    }
    // Determine routing-domains that may be affected by changes to NAT EPG
    BOOST_FOREACH(const URI& u, notifyGroups) {
        uri_ref_map_t::iterator it = pmanager.nat_epg_l3_ext.find(u);
        if (it != pmanager.nat_epg_l3_ext.end()) {
            BOOST_FOREACH(const URI& extNet, it->second) {
                l3n_map_t::iterator it2 = pmanager.l3n_map.find(extNet);
                if (it2 != pmanager.l3n_map.end()) {
                    notifyRds.insert(it2->second.routingDomain.get()->getURI());
                }
            }
        }
    }
    notifyRds.erase(uri);   // Avoid updating twice
    guard.unlock();

    BOOST_FOREACH(const URI& u, notifyGroups) {
        pmanager.notifyEPGDomain(u);
    }
    if (class_id != modelgbp::gbp::EpGroup::CLASS_ID &&
        class_id != modelgbp::gbpe::InstContext::CLASS_ID) {
        pmanager.notifyDomain(class_id, uri);
    }
    BOOST_FOREACH(const URI& rd, notifyRds) {
        pmanager.notifyDomain(RoutingDomain::CLASS_ID, rd);
    }
}

PolicyManager::ContractListener::ContractListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}

PolicyManager::ContractListener::~ContractListener() {}

void PolicyManager::ContractListener::objectUpdated(class_id_t classId,
                                                    const URI& uri) {
    using namespace modelgbp::gbp;

    LOG(DEBUG) << "ContractListener update for URI " << uri;
    unique_lock<mutex> guard(pmanager.state_mutex);

    uri_set_t contractsToNotify;
    if (classId == EpGroup::CLASS_ID) {
        pmanager.updateGroupContracts(classId, uri, contractsToNotify);
    } else if (classId == L3ExternalNetwork::CLASS_ID) {
        pmanager.updateGroupContracts(classId, uri, contractsToNotify);
    } else if (classId == RoutingDomain::CLASS_ID) {
        pmanager.updateL3Nets(uri, contractsToNotify);
    } else {
        if (classId == Contract::CLASS_ID) {
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

PolicyManager::ConfigListener::ConfigListener(PolicyManager& pmanager_)
    : pmanager(pmanager_) {}

PolicyManager::ConfigListener::~ConfigListener() {}

void PolicyManager::ConfigListener::objectUpdated(class_id_t classId,
                                                  const URI& uri) {
    pmanager.notifyConfig(uri);
}

} /* namespace ovsagent */
