/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for EndpointManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <algorithm>

#include <opflex/modb/Mutator.h>
#include <modelgbp/ascii/StringMatchTypeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "EndpointManager.h"
#include "logging.h"

namespace ovsagent {

using std::string;
using std::vector;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::Mutator;
using boost::unique_lock;
using boost::mutex;
using boost::unordered_set;
using boost::shared_ptr;
using boost::make_shared;
using boost::optional;
using boost::algorithm::starts_with;
using boost::algorithm::ends_with;
using boost::algorithm::contains;

static const string VM_NAME_ATTR("vm-name");

EndpointManager::EndpointManager(opflex::ofcore::OFFramework& framework_,
                                 PolicyManager& policyManager_)
    : framework(framework_), policyManager(policyManager_),
      epgMappingListener(*this) {

}

EndpointManager::~EndpointManager() {

}

EndpointManager::EndpointState::EndpointState() : endpoint(new Endpoint()) {

}

void EndpointManager::start() {
    using namespace modelgbp::gbpe;

    LOG(DEBUG) << "Starting endpoint manager";

    EpgMapping::registerListener(framework, &epgMappingListener);
    EpAttributeSet::registerListener(framework, &epgMappingListener);

    policyManager.registerListener(this);
}

void EndpointManager::stop() {
    using namespace modelgbp::gbpe;

    LOG(DEBUG) << "Stopping endpoint manager";

    EpgMapping::unregisterListener(framework, &epgMappingListener);
    EpAttributeSet::unregisterListener(framework, &epgMappingListener);

    policyManager.unregisterListener(this);

    unique_lock<mutex> guard(ep_mutex);
    ep_map.clear();
    group_ep_map.clear();
    ipm_group_ep_map.clear();
    ipm_nexthop_if_ep_map.clear();
    iface_ep_map.clear();
    epgmapping_ep_map.clear();
}

void EndpointManager::registerListener(EndpointListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    endpointListeners.push_back(listener);
}

void EndpointManager::unregisterListener(EndpointListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    endpointListeners.remove(listener);
}

void EndpointManager::notifyListeners(const std::string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    BOOST_FOREACH(EndpointListener* listener, endpointListeners) {
        listener->endpointUpdated(uuid);
    }
}

shared_ptr<const Endpoint> EndpointManager::getEndpoint(const string& uuid) {
    unique_lock<mutex> guard(ep_mutex);
    ep_map_t::const_iterator it = ep_map.find(uuid);
    if (it != ep_map.end())
        return it->second.endpoint;
    return shared_ptr<const Endpoint>();
}

optional<URI> EndpointManager::getComputedEPG(const std::string& uuid) {
    unique_lock<mutex> guard(ep_mutex);
    ep_map_t::const_iterator it = ep_map.find(uuid);
    if (it != ep_map.end())
        return it->second.egURI;
    return boost::none;
}

void EndpointManager::updateEndpoint(const Endpoint& endpoint) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epdr;

    unique_lock<mutex> guard(ep_mutex);
    const string& uuid = endpoint.getUUID();
    EndpointState& es = ep_map[uuid];

    // update interface name to endpoint mapping
    const optional<std::string>& oldIface = es.endpoint->getInterfaceName();
    const optional<std::string>& iface = endpoint.getInterfaceName();
    if (oldIface != iface) {
        if (oldIface) {
            unordered_set<string>& eps = iface_ep_map[oldIface.get()];
            eps.erase(uuid);
            if (eps.size() == 0)
                iface_ep_map.erase(oldIface.get());
        }
        if (iface) {
            iface_ep_map[iface.get()].insert(uuid);
        }
    }

    // Update IP Mapping next hop interface to endpoint mapping
    BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
                  es.endpoint->getIPAddressMappings()) {
        if (!ipm.getNextHopIf()) continue;
        unordered_set<string>& eps =
            ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()];
        eps.erase(uuid);
        if (eps.size() == 0)
            ipm_nexthop_if_ep_map.erase(ipm.getNextHopIf().get());
    }
    BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
                  endpoint.getIPAddressMappings()) {
        if (!ipm.getNextHopIf()) continue;
        ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()].insert(uuid);
    }

    // update epg mapping alias to endpoint mapping
    const optional<std::string>& oldEpgmap = es.endpoint->getEgMappingAlias();
    const optional<std::string>& epgmap = endpoint.getEgMappingAlias();
    if (oldEpgmap != epgmap) {
        if (oldEpgmap) {
            unordered_set<string>& eps = epgmapping_ep_map[oldEpgmap.get()];
            eps.erase(uuid);
            if (eps.size() == 0)
                epgmapping_ep_map.erase(oldEpgmap.get());
        }
        if (epgmap) {
            epgmapping_ep_map[epgmap.get()].insert(uuid);
        }
    }

    // Update VMEp registration
    Mutator mutator(framework, "policyelement");
    if (es.vmEP) {
        optional<shared_ptr<VMEp> > oldvme =
            VMEp::resolve(es.vmEP.get());
        if (oldvme && oldvme.get()->getUuid("") != uuid)
            oldvme.get()->remove();
    }
    optional<shared_ptr<VMUniverse> > vmu =
        VMUniverse::resolve(framework);
    shared_ptr<VMEp> vme = vmu.get()
        ->addGbpeVMEp(uuid);
    es.vmEP = vme->getURI();
    mutator.commit();

    es.endpoint = make_shared<const Endpoint>(endpoint);

    updateEndpointLocal(uuid);
    guard.unlock();
    notifyListeners(uuid);
}

void EndpointManager::removeEndpoint(const std::string& uuid) {
    using namespace modelgbp::epdr;
    using namespace modelgbp::epr;
    using namespace modelgbp::gbpe;

    unique_lock<mutex> guard(ep_mutex);
    Mutator mutator(framework, "policyelement");
    ep_map_t::iterator it = ep_map.find(uuid);
    if (it != ep_map.end()) {
        EndpointState& es = it->second;
        // remove any associated modb entries
        BOOST_FOREACH(const URI& locall2ep, es.locall2EPs) {
            LocalL2Ep::remove(framework, locall2ep);
        }
        BOOST_FOREACH(const URI& locall3ep, es.locall3EPs) {
            LocalL3Ep::remove(framework, locall3ep);
        }
        BOOST_FOREACH(const URI& l2ep, es.l2EPs) {
            L2Ep::remove(framework, l2ep);
        }
        BOOST_FOREACH(const URI& l3ep, es.l3EPs) {
            L3Ep::remove(framework, l3ep);
        }
        EpCounter::remove(framework, uuid);
        if (es.vmEP)
            VMEp::remove(framework, es.vmEP.get());
        if (es.egURI) {
            group_ep_map_t::iterator it = group_ep_map.find(es.egURI.get());
            if (it != group_ep_map.end()) {
                group_ep_map.erase(it);
            }
        }
        BOOST_FOREACH(const URI& ipmGrp, es.ipMappingGroups) {
            group_ep_map_t::iterator it = ipm_group_ep_map.find(ipmGrp);
            if (it != ipm_group_ep_map.end()) {
                ipm_group_ep_map.erase(it);
            }
        }
        if (es.endpoint->getInterfaceName()) {
            string_ep_map_t::iterator it =
                iface_ep_map.find(es.endpoint->getInterfaceName().get());
            if (it != iface_ep_map.end()) {
                iface_ep_map.erase(it);
            }
        }
        BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
                      es.endpoint->getIPAddressMappings()) {
            if (!ipm.getNextHopIf()) continue;
            unordered_set<string>& eps =
                ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()];
            eps.erase(uuid);
            if (eps.size() == 0)
                ipm_nexthop_if_ep_map.erase(ipm.getNextHopIf().get());
        }
        if (es.endpoint->getEgMappingAlias()) {
            string_ep_map_t::iterator it =
                epgmapping_ep_map.find(es.endpoint->getEgMappingAlias().get());
            if (it != epgmapping_ep_map.end()) {
                epgmapping_ep_map.erase(es.endpoint->getEgMappingAlias().get());
            }
        }

        ep_map.erase(it);
    }
    mutator.commit();
    guard.unlock();
    notifyListeners(uuid);
}

optional<URI> EndpointManager::resolveEpgMapping(EndpointState& es) {
    using namespace modelgbp::gbpe;
    using namespace modelgbp::ascii;

    const optional<string>& mappingAlias = es.endpoint->getEgMappingAlias();
    if (!mappingAlias) return boost::none;
    optional<shared_ptr<EpgMapping> > mapping =
        EpgMapping::resolve(framework, mappingAlias.get());
    if (!mapping) return boost::none;

    vector<shared_ptr<AttributeMappingRule> > rules;
    mapping.get()->resolveGbpeAttributeMappingRule(rules);

    OrderComparator<shared_ptr<AttributeMappingRule> > ruleComp;
    stable_sort(rules.begin(), rules.end(), ruleComp);

    BOOST_FOREACH(shared_ptr<AttributeMappingRule>& rule, rules) {
        optional<const string&> attrName = rule->getAttributeName();
        optional<const string&> matchString = rule->getMatchString();
        if (!attrName || !matchString) continue;
        uint8_t type = rule->getMatchType(StringMatchTypeEnumT::CONST_EQUALS);
        bool negated = 0 != rule->getNegated(0);

        // Get value of attribute from endpoint index
        string attrValue;
        Endpoint::attr_map_t::const_iterator it =
            es.endpoint->getAttributes().find(attrName.get());
        if (it != es.endpoint->getAttributes().end())
            attrValue = it->second;
        else {
            it = es.epAttrs.find(attrName.get());
            if (it != es.epAttrs.end())
                attrValue = it->second;
        }

        // apply the match
        bool matches = false;
        switch (type) {
        case StringMatchTypeEnumT::CONST_CONTAINS:
            matches = contains(attrValue, matchString.get());
            break;
        case StringMatchTypeEnumT::CONST_STARTSWITH:
            matches = starts_with(attrValue, matchString.get());
            break;
        case StringMatchTypeEnumT::CONST_ENDSWITH:
            matches = ends_with(attrValue, matchString.get());
            break;
        case StringMatchTypeEnumT::CONST_EQUALS:
            matches = (attrValue == matchString.get());
            break;
        default:
            // unknown match always fails
            matches = negated;
            break;
        }
        matches = (matches != negated);

        if (matches) {
            optional<shared_ptr<MappingRuleToGroupRSrc> > egSrc =
                rule->resolveGbpeMappingRuleToGroupRSrc();
            if (egSrc && egSrc.get()->isTargetSet())
                return egSrc.get()->getTargetURI().get();
        }
    }

    // No matching rule, use default mapping
    optional<shared_ptr<EpgMappingToDefaultGroupRSrc> > egSrc =
        mapping.get()->resolveGbpeEpgMappingToDefaultGroupRSrc();
    if (egSrc && egSrc.get()->isTargetSet())
        return egSrc.get()->getTargetURI().get();

    return boost::none;
}

bool EndpointManager::updateEndpointLocal(const std::string& uuid) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epdr;

    ep_map_t::iterator it = ep_map.find(uuid);
    if (it == ep_map.end()) return false;
    bool updated = false;

    EndpointState& es = it->second;

    // update endpoint group to endpoint mapping
    const optional<URI>& oldEgURI = es.egURI;

    // attempt to get endpoint group from a directly-configured group
    optional<URI> egURI = es.endpoint->getEgURI();
    if (!egURI) {
        // fall back to computing endpoint group from epg mapping
        egURI = resolveEpgMapping(es);
    }

    if (oldEgURI != egURI) {
        if (oldEgURI) {
            unordered_set<string>& eps = group_ep_map[oldEgURI.get()];
            eps.erase(uuid);
            if (eps.size() == 0)
                group_ep_map.erase(oldEgURI.get());
        }
        if (egURI) {
            group_ep_map[egURI.get()].insert(uuid);
        }
        es.egURI = egURI;
        updated = true;
    }

    unordered_set<URI> newlocall3eps;
    unordered_set<URI> newlocall2eps;
    unordered_set<URI> newipmgroups;

    Mutator mutator(framework, "policyelement");

    const optional<MAC>& mac = es.endpoint->getMAC();

    if (mac) {
        // Update LocalL2 objects in the MODB, which will trigger
        // resolution of the endpoint group and/or epg mapping, if
        // needed.
        optional<shared_ptr<L2Discovered> > l2d =
            L2Discovered::resolve(framework);
        if (l2d) {
            shared_ptr<LocalL2Ep> l2e = l2d.get()
                ->addEpdrLocalL2Ep(uuid);
            l2e->setMac(mac.get());
            if (egURI) {
                l2e->addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(egURI.get());
            } else {
                optional<shared_ptr<EndPointToGroupRSrc> > ctx =
                    l2e->resolveEpdrEndPointToGroupRSrc();
                if (ctx)
                    ctx.get()->remove();
            }

            const optional<string>& epgMapping =
                es.endpoint->getEgMappingAlias();
            if (epgMapping) {
                l2e->addGbpeEpgMappingCtx()
                    ->addGbpeEpgMappingCtxToEpgMappingRSrc()
                    ->setTargetEpgMapping(epgMapping.get());
                l2e->addGbpeEpgMappingCtx()
                    ->addGbpeEpgMappingCtxToAttrSetRSrc()
                    ->setTargetEpAttributeSet(uuid);
            } else {
                optional<shared_ptr<EpgMappingCtx> > ctx =
                    l2e->resolveGbpeEpgMappingCtx();
                if (ctx)
                    ctx.get()->remove();
            }

            newlocall2eps.insert(l2e->getURI());

            // Update LocalL2 objects in the MODB corresponding to
            // floating IP endpoints
            BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
                          es.endpoint->getIPAddressMappings()) {
                if (!ipm.getMappedIP() || !ipm.getEgURI())
                    continue;

                newipmgroups.insert(ipm.getEgURI().get());

                shared_ptr<LocalL2Ep> fl2e = l2d.get()
                    ->addEpdrLocalL2Ep(ipm.getUUID());
                fl2e->setMac(mac.get());
                fl2e->addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(ipm.getEgURI().get());
                newlocall2eps.insert(fl2e->getURI());
            }
        }

        // Update LocalL3 objects in the MODB, which, though it won't
        // cause any improved functionality, may cause overall
        // happiness in the universe to increase.
        optional<shared_ptr<L3Discovered> > l3d =
            L3Discovered::resolve(framework);
        if (l3d) {
            BOOST_FOREACH(const string& ip, es.endpoint->getIPs()) {
                shared_ptr<LocalL3Ep> l3e = l3d.get()
                    ->addEpdrLocalL3Ep(uuid);
                l3e->setIp(ip)
                    .setMac(mac.get());
                newlocall3eps.insert(l3e->getURI());
            }
        }
    }

    // remove any stale local EPs
    BOOST_FOREACH(const URI& locall2ep, es.locall2EPs) {
        if (newlocall2eps.find(locall2ep) == newlocall2eps.end()) {
            LocalL2Ep::remove(framework, locall2ep);
        }
    }
    es.locall2EPs = newlocall2eps;
    BOOST_FOREACH(const URI& locall3ep, es.locall3EPs) {
        if (newlocall3eps.find(locall3ep) == newlocall3eps.end()) {
            LocalL3Ep::remove(framework, locall3ep);
        }
    }
    es.locall3EPs = newlocall3eps;

    // Update IP address mapping group map
    BOOST_FOREACH(const URI& ipmGrp, newipmgroups) {
        ipm_group_ep_map[ipmGrp].insert(uuid);
    }
    BOOST_FOREACH(const URI& ipmGrp, es.ipMappingGroups) {
        if (newipmgroups.find(ipmGrp) == newipmgroups.end()) {
            unordered_set<string>& eps = ipm_group_ep_map[ipmGrp];
            eps.erase(uuid);
            if (eps.size() == 0)
                ipm_group_ep_map.erase(ipmGrp);
        }
    }
    es.ipMappingGroups = newipmgroups;

    mutator.commit();

    updated |= updateEndpointReg(uuid);

    return updated;
}

static shared_ptr<modelgbp::epr::L2Ep>
populateL2E(shared_ptr<modelgbp::epr::L2Universe> l2u,
            shared_ptr<const Endpoint>& ep,
            const std::string& uuid,
            shared_ptr<modelgbp::gbp::BridgeDomain>& bd,
            const URI& egURI) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    shared_ptr<L2Ep> l2e =
        l2u->addEprL2Ep(bd.get()->getURI().toString(),
                        ep->getMAC().get());
    l2e->setUuid(uuid);
    l2e->setGroup(egURI.toString());

    if (ep->getInterfaceName())
        l2e->setInterfaceName(ep->getInterfaceName().get());
    const Endpoint::attr_map_t& attr_map = ep->getAttributes();
    Endpoint::attr_map_t::const_iterator vi = attr_map.find("vm-name");
    if (vi != attr_map.end())
        l2e->setVmName(vi->second);

    return l2e;
}

bool EndpointManager::updateEndpointReg(const std::string& uuid) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    ep_map_t::iterator it = ep_map.find(uuid);
    if (it == ep_map.end()) return false;

    EndpointState& es = it->second;
    const optional<URI>& egURI = es.egURI;
    const optional<MAC>& mac = es.endpoint->getMAC();
    unordered_set<URI> newl3eps;
    unordered_set<URI> newl2eps;
    optional<shared_ptr<RoutingDomain> > rd;
    optional<shared_ptr<BridgeDomain> > bd;

    if (egURI) {
        // check whether the l2 and l3 routing contexts are already
        // resolved.
        rd = policyManager.getRDForGroup(egURI.get());
        bd = policyManager.getBDForGroup(egURI.get());
    }

    Mutator mutator(framework, "policyelement");

    optional<shared_ptr<L2Universe> > l2u =
        L2Universe::resolve(framework);
    if (l2u && bd && mac) {
        // If the bridge domain is known, we can register the l2
        // endpoint
        {
            shared_ptr<L2Ep> l2e =
                populateL2E(l2u.get(), es.endpoint, uuid,
                            bd.get(), egURI.get());

            newl2eps.insert(l2e->getURI());
        }

        BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
                      es.endpoint->getIPAddressMappings()) {
            if (!ipm.getFloatingIP() || !ipm.getMappedIP() || !ipm.getEgURI())
                continue;
            // don't declare endpoints if there's a next hop
            if (ipm.getNextHopIf())
                continue;

            optional<shared_ptr<BridgeDomain> > fbd =
                policyManager.getBDForGroup(ipm.getEgURI().get());
            if (!fbd) continue;

            shared_ptr<L2Ep> fl2e =
                populateL2E(l2u.get(), es.endpoint, ipm.getUUID(), fbd.get(),
                            ipm.getEgURI().get());
            newl2eps.insert(fl2e->getURI());
        }
    }

    optional<shared_ptr<L3Universe> > l3u =
        L3Universe::resolve(framework);
    if (l3u && bd && rd && mac) {
        uint8_t routingMode =
            bd.get()->getRoutingMode(RoutingModeEnumT::CONST_ENABLED);

        if (routingMode == RoutingModeEnumT::CONST_ENABLED) {
            // If the routing domain is known, we can register the l3
            // endpoints in the endpoint registry
            BOOST_FOREACH(const string& ip, es.endpoint->getIPs()) {
                shared_ptr<L3Ep> l3e = l3u.get()
                    ->addEprL3Ep(rd.get()->getURI().toString(), ip);
                l3e->setMac(mac.get());
                l3e->setGroup(egURI->toString());
                l3e->setUuid(uuid);
                newl3eps.insert(l3e->getURI());
            }

            BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
                          es.endpoint->getIPAddressMappings()) {
                if (!ipm.getFloatingIP() || !ipm.getMappedIP() || !ipm.getEgURI())
                    continue;
                // don't declare endpoints if there's a next hop
                if (ipm.getNextHopIf())
                    continue;

                optional<shared_ptr<RoutingDomain> > frd =
                    policyManager.getRDForGroup(ipm.getEgURI().get());
                if (!frd) continue;

                shared_ptr<L3Ep> fl3e = l3u.get()
                    ->addEprL3Ep(frd.get()->getURI().toString(),
                                 ipm.getFloatingIP().get());
                fl3e->setMac(mac.get());
                fl3e->setGroup(ipm.getEgURI().get().toString());
                fl3e->setUuid(ipm.getUUID());
                newl3eps.insert(fl3e->getURI());
            }
        }
    }

    // remove any stale endpoint registry objects
    BOOST_FOREACH(const URI& l2ep, es.l2EPs) {
        if (newl2eps.find(l2ep) == newl2eps.end()) {
            L2Ep::remove(framework, l2ep);
        }
    }
    es.l2EPs = newl2eps;
    BOOST_FOREACH(const URI& l3ep, es.l3EPs) {
        if (newl3eps.find(l3ep) == newl3eps.end()) {
            L3Ep::remove(framework, l3ep);
        }
    }
    es.l3EPs = newl3eps;

    mutator.commit();
    return true;
}

void EndpointManager::egDomainUpdated(const URI& egURI) {
    unordered_set<string> notify;
    unique_lock<mutex> guard(ep_mutex);

    group_ep_map_t::const_iterator it = group_ep_map.find(egURI);
    if (it != group_ep_map.end()) {
        BOOST_FOREACH(const std::string& uuid, it->second) {
            if (updateEndpointReg(uuid))
                notify.insert(uuid);
        }
    }

    it = ipm_group_ep_map.find(egURI);
    if (it != ipm_group_ep_map.end()) {
        BOOST_FOREACH(const std::string& uuid, it->second) {
            if (updateEndpointReg(uuid))
                notify.insert(uuid);
        }
    }
    guard.unlock();

    BOOST_FOREACH(const std::string& uuid, notify) {
        notifyListeners(uuid);
    }
}

void EndpointManager::getEndpointsForGroup(const URI& egURI,
                                           /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    group_ep_map_t::const_iterator it = group_ep_map.find(egURI);
    if (it != group_ep_map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

void EndpointManager::getEndpointsForIPMGroup(const URI& egURI,
                                              /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    group_ep_map_t::const_iterator it = ipm_group_ep_map.find(egURI);
    if (it != ipm_group_ep_map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

void EndpointManager::getEndpointsByIface(const std::string& ifaceName,
                                          /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    string_ep_map_t::const_iterator it = iface_ep_map.find(ifaceName);
    if (it != iface_ep_map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

void EndpointManager::getEndpointsByIpmNextHopIf(const std::string& ifaceName,
                                                 /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    string_ep_map_t::const_iterator it = ipm_nexthop_if_ep_map.find(ifaceName);
    if (it != ipm_nexthop_if_ep_map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

void EndpointManager::updateEndpointCounters(const std::string& uuid,
                                             EpCounters& newVals) {
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<EpStatUniverse> > su =
        EpStatUniverse::resolve(framework);
    if (su) {
        su.get()->addGbpeEpCounter(uuid)
            ->setRxPackets(newVals.rxPackets)
            .setTxPackets(newVals.txPackets)
            .setRxDrop(newVals.rxDrop)
            .setTxDrop(newVals.txDrop)
            .setRxBroadcast(newVals.rxBroadcast)
            .setTxBroadcast(newVals.txBroadcast)
            .setRxMulticast(newVals.rxMulticast)
            .setTxMulticast(newVals.txMulticast)
            .setRxUnicast(newVals.rxUnicast)
            .setTxUnicast(newVals.txUnicast)
            .setRxBytes(newVals.rxBytes)
            .setTxBytes(newVals.txBytes);
    }

    mutator.commit();
}

EndpointManager::EPGMappingListener::EPGMappingListener(EndpointManager& epmanager_)
    : epmanager(epmanager_) {}

EndpointManager::EPGMappingListener::~EPGMappingListener() {}

void EndpointManager::EPGMappingListener::objectUpdated(class_id_t classId,
                                                        const URI& uri) {
    using namespace modelgbp::gbpe;
    unique_lock<mutex> guard(epmanager.ep_mutex);

    if (classId == EpAttributeSet::CLASS_ID) {
        optional<shared_ptr<EpAttributeSet> > attrSet =
            EpAttributeSet::resolve(epmanager.framework, uri);
        if (!attrSet) return;

        optional<const std::string&> uuid = attrSet.get()->getUuid();
        if (!uuid) return;

        ep_map_t::iterator it = epmanager.ep_map.find(uuid.get());
        if (it == epmanager.ep_map.end()) return;

        EndpointState& es = it->second;
        es.epAttrs.clear();

        vector<shared_ptr<EpAttribute> > attrs;
        attrSet.get()->resolveGbpeEpAttribute(attrs);
        BOOST_FOREACH(shared_ptr<EpAttribute>& attr, attrs) {
            optional<const std::string&> name = attr->getName();
            optional<const std::string&> value = attr->getValue();

            if (!name) continue;
            if (value)
                es.epAttrs[name.get()] = value.get();
            else
                es.epAttrs[name.get()] = "";
        }

        if (epmanager.updateEndpointLocal(uuid.get())) {
            guard.unlock();
            epmanager.notifyListeners(uuid.get());
        }
    } else if (classId == EpgMapping::CLASS_ID) {
        optional<shared_ptr<EpgMapping> > epgMapping =
            EpgMapping::resolve(epmanager.framework, uri);
        if (!epgMapping) return;

        optional<const std::string&> name = epgMapping.get()->getName();
        if (!name) return;

        string_ep_map_t::iterator it =
            epmanager.epgmapping_ep_map.find(name.get());
        if (it == epmanager.epgmapping_ep_map.end()) return;

        unordered_set<string> notify;
        BOOST_FOREACH(const std::string& uuid, it->second) {
            if (epmanager.updateEndpointLocal(uuid)) {
                notify.insert(uuid);
            }
        }

        guard.unlock();
        BOOST_FOREACH(const std::string& uuid, notify) {
            epmanager.notifyListeners(uuid);
        }
    }
}

} /* namespace ovsagent */
