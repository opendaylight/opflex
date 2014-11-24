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

#include <opflex/modb/Mutator.h>
#include <boost/foreach.hpp>

#include "EndpointManager.h"
#include "logging.h"

namespace ovsagent {

using std::string;
using opflex::modb::URI;
using opflex::modb::Mutator;
using boost::unique_lock;
using boost::mutex;
using boost::unordered_set;
using boost::shared_ptr;
using boost::optional;

EndpointManager::EndpointManager(opflex::ofcore::OFFramework& framework_,
                                 PolicyManager& policyManager_)
    : framework(framework_), policyManager(policyManager_) {

}

EndpointManager::~EndpointManager() {

}

void EndpointManager::start() {
    LOG(DEBUG) << "Starting endpoint manager";

    policyManager.registerListener(this);
}

void EndpointManager::stop() {
    LOG(DEBUG) << "Stopping endpoint manager";

    policyManager.unregisterListener(this);

    unique_lock<mutex> guard(ep_mutex);
    ep_map.clear();
    group_ep_map.clear();
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

optional<const Endpoint&> EndpointManager::getEndpoint(const string& uuid) {
    unique_lock<mutex> guard(ep_mutex);
    ep_map_t::const_iterator it = ep_map.find(uuid);
    if (it != ep_map.end())
        return it->second.endpoint;
    return boost::none;
}

void EndpointManager::updateEndpoint(const Endpoint& endpoint) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epdr;

    unique_lock<mutex> guard(ep_mutex);
    const string& uuid = endpoint.getUUID();

    EndpointState& es = ep_map[uuid];

    const optional<URI>& oldEgURI = es.endpoint.getEgURI();
    const optional<URI>& egURI = endpoint.getEgURI();

    // update endpoint group to endpoint mapping
    if (es.endpoint.getEgURI() != egURI) {
        if (oldEgURI) {
            unordered_set<string> eps = group_ep_map[oldEgURI.get()];
            eps.erase(uuid);
            if (eps.size() == 0)
                group_ep_map.erase(oldEgURI.get());
        }
        if (egURI) {
            group_ep_map[egURI.get()].insert(uuid);
        }
    }


    unordered_set<URI> newlocall3eps;
    unordered_set<URI> newlocall2eps;

    Mutator mutator(framework, "policyelement");

    if (egURI) {
        // Update LocalL2 objects in the MODB, which will trigger
        // resolution of the endpoint group, if needed.
        optional<shared_ptr<L2Discovered> > l2d = 
            L2Discovered::resolve(framework);
        if (l2d) {
            shared_ptr<LocalL2Ep> l2e = l2d.get()
                ->addEpdrLocalL2Ep(uuid);
            l2e->setMac(endpoint.getMAC())
                .addEpdrEndPointToGroupRSrc()
                ->setTargetEpGroup(egURI.get());
            newlocall2eps.insert(l2e->getURI());
        }

        // Update LocalL3 objects in the MODB, which, though it won't
        // cause any improved functionality, may cause overall
        // happiness in the universe to increase.
        optional<shared_ptr<L3Discovered> > l3d = 
            L3Discovered::resolve(framework);
        if (l3d) {
            BOOST_FOREACH(const string& ip, endpoint.getIPs()) {
                shared_ptr<LocalL3Ep> l3e = l3d.get()
                    ->addEpdrLocalL3Ep(uuid);
                l3e->setIp(ip)
                    .setMac(endpoint.getMAC())
                    .addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(egURI.get());
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

    es.endpoint = endpoint;

    mutator.commit();
    updateEndpointReg(uuid);
}

void EndpointManager::removeEndpoint(const std::string& uuid) {
    using namespace modelgbp::epdr;
    using namespace modelgbp::epr;

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
        ep_map.erase(it);
    }
    mutator.commit();
    guard.unlock();
    notifyListeners(uuid);
}

bool EndpointManager::updateEndpointReg(const std::string& uuid) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    ep_map_t::iterator it = ep_map.find(uuid);
    if (it == ep_map.end()) return false;

    EndpointState& es = it->second;
    const optional<URI>& egURI = es.endpoint.getEgURI();
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
    if (l2u && bd) {
        // If the bridge domain is known, we can register the l2
        // endpoint
        shared_ptr<L2Ep> l2e = l2u.get()
            ->addEprL2Ep(bd.get()->getURI().toString(),
                         es.endpoint.getMAC());
        newl2eps.insert(l2e->getURI());
    }

    optional<shared_ptr<L3Universe> > l3u = 
        L3Universe::resolve(framework);
    if (l3u && rd) {
        // If the routing domain is known, we can register the l3
        // endpoints in the endpoint registry
        BOOST_FOREACH(const string& ip, es.endpoint.getIPs()) {
            shared_ptr<L3Ep> l3e = l3u.get()
                ->addEprL3Ep(rd.get()->getURI().toString(), ip);
            l3e->setMac(es.endpoint.getMAC());
            newl3eps.insert(l3e->getURI());
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

    group_ep_map_t::const_iterator it =
        group_ep_map.find(egURI);
    if (it == group_ep_map.end()) return;

    BOOST_FOREACH(const std::string& uuid, it->second) {
        if (updateEndpointReg(uuid))
            notify.insert(uuid);
    }
    guard.unlock();

    BOOST_FOREACH(const std::string& uuid, notify) {
        notifyListeners(uuid);
    }
}

void
EndpointManager::getEndpointsForGroup(const URI& egURI,
                                      /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    group_ep_map_t::const_iterator it = group_ep_map.find(egURI);
    if (it != group_ep_map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}
} /* namespace ovsagent */
