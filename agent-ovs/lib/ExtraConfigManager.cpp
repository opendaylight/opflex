/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ExtraConfigManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/ExtraConfigManager.h>
#include <opflexagent/logging.h>

#include <memory>

namespace opflexagent {

using std::string;
using std::unique_lock;
using std::mutex;
using boost::optional;
using std::shared_ptr;
using std::unordered_set;
using std::make_shared;

ExtraConfigManager::ExtraConfigManager() {

}

void ExtraConfigManager::registerListener(ExtraConfigListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    extraConfigListeners.push_back(listener);
}

void ExtraConfigManager::unregisterListener(ExtraConfigListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    extraConfigListeners.remove(listener);
}

void ExtraConfigManager::notifyListeners(const opflex::modb::URI& domainURI) {
    unique_lock<mutex> guard(listener_mutex);
    for (ExtraConfigListener* listener : extraConfigListeners) {
        listener->rdConfigUpdated(domainURI);
    }
}

shared_ptr<const RDConfig>
ExtraConfigManager::getRDConfig(const opflex::modb::URI& uri) {
    unique_lock<mutex> guard(ec_mutex);
    rdc_map_t::const_iterator it = rdc_map.find(uri);
    if (it != rdc_map.end())
        return it->second.rdConfig;
    return shared_ptr<const RDConfig>();
}

void ExtraConfigManager::updateRDConfig(const RDConfig& rdConfig) {
    unique_lock<mutex> guard(ec_mutex);
    RDConfigState& as = rdc_map[rdConfig.getDomainURI()];

    as.rdConfig = make_shared<const RDConfig>(rdConfig);

    guard.unlock();
    notifyListeners(rdConfig.getDomainURI());
}

void ExtraConfigManager::removeRDConfig(const opflex::modb::URI& uri) {
    unique_lock<mutex> guard(ec_mutex);
    rdc_map_t::iterator it = rdc_map.find(uri);
    if (it != rdc_map.end()) {
        rdc_map.erase(it);
    }

    guard.unlock();
    notifyListeners(uri);
}

} /* namespace opflexagent */
