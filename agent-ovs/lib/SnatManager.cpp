/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SnatManager class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/SnatManager.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using std::string;
using std::unordered_set;
using std::shared_ptr;
using std::make_shared;
using std::unique_lock;
using std::mutex;
using std::make_pair;
using boost::optional;

void SnatManager::registerListener(SnatListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    snatListeners.push_back(listener);
}

void SnatManager::unregisterListener(SnatListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    snatListeners.remove(listener);
}

void SnatManager::notifyListeners(const string& snatIp, const string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    for (SnatListener* listener : snatListeners) {
        listener->snatUpdated(snatIp, uuid);
    }
}

shared_ptr<const Snat>
SnatManager::getSnat(const string& snatIp) {
    unique_lock<mutex> guard(snat_mutex);
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it != snat_map.end())
        return it->second.snat;
    return shared_ptr<const Snat>();
}

void SnatManager::removeIfaces(const Snat& snat) {
    iface_snats_map_t::iterator it =
         iface_snats_map.find(snat.getInterfaceName());
    if (it != iface_snats_map.end()) {
         snats_t& snats = it->second;
         snats.erase(make_pair(snat.getUUID(), snat.getSnatIP()));

         if (snats.size() == 0) {
            iface_snats_map.erase(it);
         }
    }
}

void SnatManager::updateSnat(const Snat& snat) {
    unique_lock<mutex> guard(snat_mutex);
    const string& snatIp = snat.getSnatIP();
    const string& uuid = snat.getUUID();
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it != snat_map.end()) {
        snat_map.erase(it);
        shared_ptr<const Snat> asWrapper = it->second.snat;
        const Snat& as = *asWrapper;
        removeIfaces(as);
    }

    iface_snats_map[snat.getInterfaceName()]
        .insert(make_pair(uuid, snatIp));
    SnatState& state = snat_map[snatIp];
    state.snat = make_shared<const Snat>(snat);
    guard.unlock();
    notifyListeners(snatIp, uuid);
}

void SnatManager::removeSnat(const string& snatIp, const string& uuid) {
    unique_lock<mutex> guard(snat_mutex);
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it != snat_map.end()) {
        shared_ptr<const Snat> asWrapper = it->second.snat;
        const Snat& as = *asWrapper;
        removeIfaces(as);
        snat_map.erase(it);
    }
    guard.unlock();
    notifyListeners(snatIp, uuid);
}

void SnatManager::addEndpoint(const string& snatIp,
                              const string& uuid) {
    unique_lock<mutex> guard(snat_mutex);
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it == snat_map.end()) {
        LOG(DEBUG) << "Adding endpoint " << uuid
                   << "to non-existant SNAT " << snatIp;
    }
    ep_set_t& epset = ep_map[snatIp];
    if (epset.find(uuid) != epset.end())
        return;

    epset.insert(uuid);
}

void SnatManager::delEndpoint(const string& snatIp,
                              const string& uuid) {
    unique_lock<mutex> guard(snat_mutex);
    auto it1 = ep_map.find(snatIp);
    if (it1 == ep_map.end()) {
        LOG(ERROR) << "snatIp " << snatIp
                   << "not found in ep_map";
        return;
    }
    ep_set_t& epset = it1->second;

    auto it2 = epset.find(uuid);
    if (it2 != epset.end())
        epset.erase(it2);

    if (epset.empty())
        ep_map.erase(it1);
}

void SnatManager::delEndpoint(const string& uuid) {
    unique_lock<mutex> guard(snat_mutex);
    auto it1 = ep_map.begin();

    while(it1 != ep_map.end()) {
        ep_set_t& epset = it1->second;

        auto it2 = epset.find(uuid);
        if (it2 != epset.end())
            epset.erase(it2);

        if (epset.empty())
            it1 = ep_map.erase(it1);
        else
            it1++;
    }
}

void SnatManager::getEndpoints(const string& snatIp,
                               unordered_set<string>& eps)
{
    unique_lock<mutex> guard(snat_mutex);
    auto it = ep_map.find(snatIp);
    if (it != ep_map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

void SnatManager::getSnatsByIface(const std::string& ifaceName,
                                  /* out */ snats_t& snats) {
    unique_lock<mutex> guard(snat_mutex);
    iface_snats_map_t::const_iterator it = iface_snats_map.find(ifaceName);
    if (it != iface_snats_map.end()) {
        snats.insert(it->second.begin(), it->second.end());
    }
}
} /* namespace opflexagent */
