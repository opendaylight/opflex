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
using boost::optional;

void SnatManager::registerListener(SnatListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    snatListeners.push_back(listener);
}

void SnatManager::unregisterListener(SnatListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    snatListeners.remove(listener);
}

void SnatManager::notifyListeners(const std::string& snatIp) {
    unique_lock<mutex> guard(listener_mutex);
    for (SnatListener* listener : snatListeners) {
        listener->snatUpdated(snatIp);
    }
}

shared_ptr<const Snat>
SnatManager::getSnat(const std::string& snatIp) {
    unique_lock<mutex> guard(snat_mutex);
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it != snat_map.end())
        return it->second.snat;
    return shared_ptr<const Snat>();
}

void SnatManager::updateSnat(const Snat& snat) {
    unique_lock<mutex> guard(snat_mutex);
    const string& snatIp = snat.getSnatIP();
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it != snat_map.end()) {
        snat_map.erase(it);
    }

    SnatState& state = snat_map[snatIp];
    state.snat = make_shared<const Snat>(snat);
    guard.unlock();
    notifyListeners(snatIp);
}

void SnatManager::removeSnat(const std::string& snatIp)
{
    unique_lock<mutex> guard(snat_mutex);
    snat_map_t::const_iterator it = snat_map.find(snatIp);
    if (it != snat_map.end()) {
        snat_map.erase(it);
    }
    guard.unlock();
    notifyListeners(snatIp);
}

} /* namespace opflexagent */
