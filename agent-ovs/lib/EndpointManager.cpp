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

#include <set>
#include <algorithm>

#include <opflex/modb/Mutator.h>
#include <modelgbp/ascii/StringMatchTypeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <opflexagent/EndpointManager.h>
#include <opflexagent/Network.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using std::string;
using std::set;
using std::vector;
using std::unordered_set;
using std::shared_ptr;
using std::make_shared;
using std::unique_lock;
using std::mutex;
using std::pair;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::Mutator;
using boost::optional;
using boost::algorithm::starts_with;
using boost::algorithm::ends_with;
using boost::algorithm::contains;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
typedef EndpointListener::uri_set_t uri_set_t;

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
    secgrp_ep_map.clear();
    ipm_group_ep_map.clear();
    ipm_nexthop_if_ep_map.clear();
    iface_ep_map.clear();
    access_iface_ep_map.clear();
    access_uplink_ep_map.clear();
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
    for (EndpointListener* listener : endpointListeners) {
        listener->endpointUpdated(uuid);
    }
}

void EndpointManager::notifyListeners(const uri_set_t& secGroups) {
    unique_lock<mutex> guard(listener_mutex);
    for (EndpointListener* listener : endpointListeners) {
        listener->secGroupSetUpdated(secGroups);
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

static bool validateIp(const string& ip, bool allowLinkLocal = false) {
    boost::system::error_code ec;
    address addr = address::from_string(ip, ec);
    if (ec) return false;
    if (!allowLinkLocal && network::is_link_local(addr))
        return false;
    return true;
}

template <typename T>
static void updateEpMap(const optional<std::string>& oldVal,
                        const optional<std::string>& val,
                        T& val_map,
                        const std::string& uuid) {
    if (oldVal != val) {
        if (oldVal) {
            unordered_set<string>& eps = val_map[oldVal.get()];
            eps.erase(uuid);
            if (eps.empty())
                val_map.erase(oldVal.get());
        }
        if (val) {
            val_map[val.get()].insert(uuid);
        }
    }
}

void EndpointManager::updateEndpoint(const Endpoint& endpoint) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epdr;

    unique_lock<mutex> guard(ep_mutex);
    const string& uuid = endpoint.getUUID();
    EndpointState& es = ep_map[uuid];
    unordered_set<uri_set_t> notifySecGroupSets;

    // update security group mapping
    const set<URI>& oldSecGroups = es.endpoint->getSecurityGroups();
    const set<URI>& secGroups = endpoint.getSecurityGroups();
    if (secGroups != oldSecGroups) {
        secgrp_ep_map_t::iterator it = secgrp_ep_map.find(oldSecGroups);
        if (it != secgrp_ep_map.end()) {
            it->second.erase(uuid);

            if (it->second.empty()) {
                secgrp_ep_map.erase(it);
                notifySecGroupSets.insert(oldSecGroups);
            }
        }
    }
    str_uset_t& ep_set = secgrp_ep_map[secGroups];
    if (ep_set.find(uuid) == ep_set.end()) {
        ep_set.insert(uuid);
        notifySecGroupSets.insert(secGroups);
    }

    // update interface name to endpoint mapping
    const optional<std::string>& oldIface = es.endpoint->getInterfaceName();
    const optional<std::string>& iface = endpoint.getInterfaceName();
    updateEpMap(oldIface, iface, iface_ep_map, uuid);

    // update access interface name to endpoint mapping
    const optional<std::string>& oldAccess = es.endpoint->getAccessInterface();
    const optional<std::string>& access = endpoint.getAccessInterface();
    updateEpMap(oldAccess, access, access_iface_ep_map, uuid);

    // update access uplink interface name to endpoint mapping
    const optional<std::string>& oldUplink =
        es.endpoint->getAccessUplinkInterface();
    const optional<std::string>& uplink = endpoint.getAccessUplinkInterface();
    updateEpMap(oldUplink, uplink, access_uplink_ep_map, uuid);

    // Update IP Mapping next hop interface to endpoint mapping
    for (const Endpoint::IPAddressMapping& ipm :
             es.endpoint->getIPAddressMappings()) {
        if (!ipm.getNextHopIf()) continue;
        unordered_set<string>& eps =
            ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()];
        eps.erase(uuid);
        if (eps.empty())
            ipm_nexthop_if_ep_map.erase(ipm.getNextHopIf().get());
    }
    for (const Endpoint::IPAddressMapping& ipm :
             endpoint.getIPAddressMappings()) {
        if (!ipm.getNextHopIf()) continue;
        ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()].insert(uuid);
    }

    // update epg mapping alias to endpoint mapping
    const optional<std::string>& oldEpgmap = es.endpoint->getEgMappingAlias();
    const optional<std::string>& epgmap = endpoint.getEgMappingAlias();
    updateEpMap(oldEpgmap, epgmap, epgmapping_ep_map, uuid);

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
    for (auto& s : notifySecGroupSets) {
        notifyListeners(s);
    }
}

void EndpointManager::removeEndpoint(const std::string& uuid) {
    using namespace modelgbp::epdr;
    using namespace modelgbp::epr;
    using namespace modelgbp::gbpe;

    unique_lock<mutex> guard(ep_mutex);
    Mutator mutator(framework, "policyelement");
    unordered_set<uri_set_t> notifySecGroupSets;

    ep_map_t::iterator it = ep_map.find(uuid);
    if (it != ep_map.end()) {
        EndpointState& es = it->second;
        // remove any associated modb entries
        for (const URI& locall2ep : es.locall2EPs) {
            LocalL2Ep::remove(framework, locall2ep);
        }
        for (const URI& locall3ep : es.locall3EPs) {
            LocalL3Ep::remove(framework, locall3ep);
        }
        for (const URI& l2ep : es.l2EPs) {
            L2Ep::remove(framework, l2ep);
        }
        for (const URI& l3ep : es.l3EPs) {
            L3Ep::remove(framework, l3ep);
        }
        EpCounter::remove(framework, uuid);
        if (es.vmEP)
            VMEp::remove(framework, es.vmEP.get());
        if (es.egURI) {
            group_ep_map_t::iterator it = group_ep_map.find(es.egURI.get());
            if (it != group_ep_map.end()) {
                it->second.erase(uuid);
                if (it->second.empty()) {
                    group_ep_map.erase(it);
                }
            }
        }

        {
            const set<URI>& secGroups = es.endpoint->getSecurityGroups();
            secgrp_ep_map_t::iterator sgit = secgrp_ep_map.find(secGroups);
            if (sgit != secgrp_ep_map.end())
                sgit->second.erase(uuid);

            if (sgit->second.empty()) {
                secgrp_ep_map.erase(sgit);
                notifySecGroupSets.insert(secGroups);
            }
        }

        for (const URI& ipmGrp : es.ipMappingGroups) {
            group_ep_map_t::iterator it = ipm_group_ep_map.find(ipmGrp);
            if (it != ipm_group_ep_map.end()) {
                it->second.erase(uuid);
                if (it->second.empty()) {
                    ipm_group_ep_map.erase(it);
                }
            }
        }

        updateEpMap(es.endpoint->getInterfaceName(), boost::none,
                    iface_ep_map, uuid);
        updateEpMap(es.endpoint->getAccessInterface(), boost::none,
                    access_iface_ep_map, uuid);
        updateEpMap(es.endpoint->getAccessUplinkInterface(), boost::none,
                    access_uplink_ep_map, uuid);

        for (const Endpoint::IPAddressMapping& ipm :
                 es.endpoint->getIPAddressMappings()) {
            if (!ipm.getNextHopIf()) continue;
            unordered_set<string>& eps =
                ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()];
            eps.erase(uuid);
            if (eps.empty())
                ipm_nexthop_if_ep_map.erase(ipm.getNextHopIf().get());
        }

        updateEpMap(es.endpoint->getEgMappingAlias(), boost::none,
                    epgmapping_ep_map, uuid);

        ep_map.erase(it);
    }
    mutator.commit();
    guard.unlock();
    notifyListeners(uuid);
    for (auto& s : notifySecGroupSets) {
        notifyListeners(s);
    }
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

    for (shared_ptr<AttributeMappingRule>& rule : rules) {
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
            if (eps.empty())
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

            vector<shared_ptr<EndPointToSecGroupRSrc> > oldSecGrps;
            l2e->resolveEpdrEndPointToSecGroupRSrc(oldSecGrps);
            const set<URI>& secGrps = es.endpoint->getSecurityGroups();
            for (const shared_ptr<EndPointToSecGroupRSrc>& og :
                     oldSecGrps) {
                optional<URI> targ = og->getTargetURI();
                if (!targ || secGrps.find(targ.get()) == secGrps.end())
                    og->remove();
            }
            for (const URI& sg : secGrps) {
                l2e->addEpdrEndPointToSecGroupRSrc(sg.toString());
            }

            // Update LocalL2 objects in the MODB corresponding to
            // floating IP endpoints
            for (const Endpoint::IPAddressMapping& ipm :
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
            for (const string& ip : es.endpoint->getIPs()) {
                if (!validateIp(ip)) continue;
                shared_ptr<LocalL3Ep> l3e = l3d.get()
                    ->addEpdrLocalL3Ep(uuid);
                l3e->setIp(ip)
                    .setMac(mac.get());
                newlocall3eps.insert(l3e->getURI());
            }
        }
    }

    // remove any stale local EPs
    for (const URI& locall2ep : es.locall2EPs) {
        if (newlocall2eps.find(locall2ep) == newlocall2eps.end()) {
            LocalL2Ep::remove(framework, locall2ep);
        }
    }
    es.locall2EPs = newlocall2eps;
    for (const URI& locall3ep : es.locall3EPs) {
        if (newlocall3eps.find(locall3ep) == newlocall3eps.end()) {
            LocalL3Ep::remove(framework, locall3ep);
        }
    }
    es.locall3EPs = newlocall3eps;

    // Update IP address mapping group map
    for (const URI& ipmGrp : newipmgroups) {
        ipm_group_ep_map[ipmGrp].insert(uuid);
    }
    for (const URI& ipmGrp : es.ipMappingGroups) {
        if (newipmgroups.find(ipmGrp) == newipmgroups.end()) {
            unordered_set<string>& eps = ipm_group_ep_map[ipmGrp];
            eps.erase(uuid);
            if (eps.empty())
                ipm_group_ep_map.erase(ipmGrp);
        }
    }
    es.ipMappingGroups = newipmgroups;

    mutator.commit();

    updated |= updateEndpointReg(uuid);

    return updated;
}

static shared_ptr<modelgbp::epr::L2Ep>
populateL2E(shared_ptr<modelgbp::epr::L2Universe>& l2u,
            shared_ptr<const Endpoint>& ep,
            const std::string& uuid,
            shared_ptr<modelgbp::gbp::BridgeDomain>& bd,
            const URI& egURI,
            const std::set<opflex::modb::URI>& secGroups) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epr;

    shared_ptr<L2Ep> l2e =
        l2u->addEprL2Ep(bd.get()->getURI().toString(),
                        ep->getMAC().get());
    l2e->setUuid(uuid);
    l2e->setGroup(egURI.toString());

    for (const opflex::modb::URI& secGroup : secGroups) {
        l2e->addEprSecurityGroupContext(secGroup.toString());
    }

    if (ep->getAccessInterface())
        l2e->setInterfaceName(ep->getAccessInterface().get());
    else if (ep->getInterfaceName())
        l2e->setInterfaceName(ep->getInterfaceName().get());
    const Endpoint::attr_map_t& attr_map = ep->getAttributes();
    shared_ptr<ReportedEpAttributeSet> epas =
        l2e->addGbpeReportedEpAttributeSet();
    for (const pair<const string, string>& ap : attr_map) {
        shared_ptr<ReportedEpAttribute> epa =
            epas->addGbpeReportedEpAttribute(ap.first);
        epa->setName(ap.first);
        epa->setValue(ap.second);
        if (VM_NAME_ATTR == ap.first)
            l2e->setVmName(ap.second);
    }

    for (const Endpoint::Attestation& a : ep->getAttestations()) {
        if (!a.getValidator() || !a.getValidatorMac())
            continue;
        l2e->addGbpeAttestation(a.getName())
            ->setValidator(a.getValidator().get())
            .setValidatorMac(a.getValidatorMac().get());
    }

    return l2e;
}
static shared_ptr<modelgbp::epr::L3Ep>
populateL3E(shared_ptr<modelgbp::epr::L3Universe>& l3u,
            shared_ptr<const Endpoint>& ep,
            const std::string& uuid,
            shared_ptr<modelgbp::gbp::RoutingDomain>& rd,
            const string& ip,
            const URI& egURI,
            const std::set<opflex::modb::URI>& secGroups) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    shared_ptr<L3Ep> l3e =
        l3u.get()->addEprL3Ep(rd.get()->getURI().toString(), ip);
    l3e->setMac(ep->getMAC().get())
        .setGroup(egURI.toString())
        .setUuid(uuid);

    for (const opflex::modb::URI& secGroup : secGroups) {
        l3e->addEprSecurityGroupContext(secGroup.toString());
    }

    for (const Endpoint::Attestation& a : ep->getAttestations()) {
        if (!a.getValidator() || !a.getValidatorMac())
            continue;
        l3e->addGbpeAttestation(a.getName())
            ->setValidator(a.getValidator().get())
            .setValidatorMac(a.getValidatorMac().get());
    }

    return l3e;
}

bool EndpointManager::updateEndpointReg(const std::string& uuid) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    ep_map_t::iterator it = ep_map.find(uuid);
    if (it == ep_map.end()) return false;

    EndpointState& es = it->second;
    const optional<URI>& egURI = es.egURI;
    const optional<MAC>& mac = es.endpoint->getMAC();
    const std::set<opflex::modb::URI>& secGroups =
        es.endpoint->getSecurityGroups();
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
                            bd.get(), egURI.get(), secGroups);

            newl2eps.insert(l2e->getURI());
        }

        for (const Endpoint::IPAddressMapping& ipm :
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
                            ipm.getEgURI().get(),
                            secGroups);
            newl2eps.insert(fl2e->getURI());
        }
    }

    optional<shared_ptr<L3Universe> > l3u =
        L3Universe::resolve(framework);
    if (l3u && bd && rd && mac) {
        uint8_t routingMode =
            policyManager.getEffectiveRoutingMode(egURI.get());

        if (routingMode == RoutingModeEnumT::CONST_ENABLED) {
            // If the routing domain is known, we can register the l3
            // endpoints in the endpoint registry
            for (const string& ip : es.endpoint->getIPs()) {
                if (!validateIp(ip)) continue;
                shared_ptr<L3Ep> l3e =
                    populateL3E(l3u.get(), es.endpoint, uuid,
                                rd.get(), ip, egURI.get(),
                                secGroups);
                newl3eps.insert(l3e->getURI());
            }

            for (const Endpoint::IPAddressMapping& ipm :
                     es.endpoint->getIPAddressMappings()) {
                if (!ipm.getFloatingIP() || !ipm.getMappedIP() ||
                    !ipm.getEgURI())
                    continue;
                // don't declare endpoints if there's a next hop
                if (ipm.getNextHopIf())
                    continue;

                optional<shared_ptr<RoutingDomain> > frd =
                    policyManager.getRDForGroup(ipm.getEgURI().get());
                if (!frd) continue;

                shared_ptr<L3Ep> fl3e =
                    populateL3E(l3u.get(), es.endpoint, ipm.getUUID(),
                                frd.get(), ipm.getFloatingIP().get(),
                                ipm.getEgURI().get(),
                                secGroups);
                newl3eps.insert(fl3e->getURI());
            }
        }
    }

    // remove any stale endpoint registry objects
    for (const URI& l2ep : es.l2EPs) {
        if (newl2eps.find(l2ep) == newl2eps.end()) {
            L2Ep::remove(framework, l2ep);
        }
    }
    es.l2EPs = newl2eps;
    for (const URI& l3ep : es.l3EPs) {
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
        for (const std::string& uuid : it->second) {
            if (updateEndpointReg(uuid))
                notify.insert(uuid);
        }
    }

    it = ipm_group_ep_map.find(egURI);
    if (it != ipm_group_ep_map.end()) {
        for (const std::string& uuid : it->second) {
            if (updateEndpointReg(uuid))
                notify.insert(uuid);
        }
    }
    guard.unlock();

    for (const std::string& uuid : notify) {
        notifyListeners(uuid);
    }
}

template <typename K, typename M>
static void getEps(const K& key, const M& map,
                   /* out */ unordered_set<string>& eps) {
    typename M::const_iterator it = map.find(key);
    if (it != map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

void EndpointManager::getEndpointsForGroup(const URI& egURI,
                                           /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(egURI, group_ep_map, eps);
}

void EndpointManager::getEndpointsForSecGrps(const uri_set_t& secGrps,
                                             unordered_set<std::string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(secGrps, secgrp_ep_map, eps);
}

bool EndpointManager::secGrpSetEmpty(const uri_set_t& secGrps) {
    unique_lock<mutex> guard(ep_mutex);
    return secgrp_ep_map.find(secGrps) == secgrp_ep_map.end();
}

void EndpointManager::
getSecGrpSetsForSecGrp(const URI& secGrp,
                       /* out */ unordered_set<uri_set_t>& result) {
    unique_lock<mutex> guard(ep_mutex);
    for (const secgrp_ep_map_t::value_type& v : secgrp_ep_map) {
        if (v.first.find(secGrp) != v.first.end())
            result.insert(v.first);
    }
}

void EndpointManager::getEndpointsForIPMGroup(const URI& egURI,
                                              unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(egURI, ipm_group_ep_map, eps);
}

void EndpointManager::getEndpointsByIface(const std::string& ifaceName,
                                          /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, iface_ep_map, eps);
}

void EndpointManager::getEndpointsByAccessIface(const std::string& ifaceName,
                                                /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, access_iface_ep_map, eps);
}

void EndpointManager::getEndpointsByAccessUplink(const std::string& ifaceName,
                                                 /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, access_uplink_ep_map, eps);
}

void EndpointManager::getEndpointsByIpmNextHopIf(const std::string& ifaceName,
                                                 /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, ipm_nexthop_if_ep_map, eps);
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

EndpointManager::EPGMappingListener::EPGMappingListener(EndpointManager& epmgr_)
    : epmanager(epmgr_) {}

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
        for (shared_ptr<EpAttribute>& attr : attrs) {
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
        for (const std::string& uuid : it->second) {
            if (epmanager.updateEndpointLocal(uuid)) {
                notify.insert(uuid);
            }
        }

        guard.unlock();
        for (const std::string& uuid : notify) {
            epmanager.notifyListeners(uuid);
        }
    }
}

} /* namespace opflexagent */
