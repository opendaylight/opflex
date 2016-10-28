/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ServiceManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include "ServiceManager.h"
#include "logging.h"

namespace ovsagent {

using std::string;
using std::unordered_set;
using boost::unique_lock;
using boost::mutex;
using boost::shared_ptr;
using boost::optional;
using boost::make_shared;

ServiceManager::ServiceManager() {

}

void ServiceManager::registerListener(ServiceListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    serviceListeners.push_back(listener);
}

void ServiceManager::unregisterListener(ServiceListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    serviceListeners.remove(listener);
}

void ServiceManager::notifyListeners(const std::string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    BOOST_FOREACH(ServiceListener* listener, serviceListeners) {
        listener->anycastServiceUpdated(uuid);
    }
}

shared_ptr<const AnycastService>
ServiceManager::getAnycastService(const string& uuid) {
    unique_lock<mutex> guard(serv_mutex);
    aserv_map_t::const_iterator it = aserv_map.find(uuid);
    if (it != aserv_map.end())
        return it->second.service;
    return shared_ptr<const AnycastService>();
}

void ServiceManager::removeIfaces(const AnycastService& service) {
    if (service.getInterfaceName()) {
        string_serv_map_t::iterator it =
            iface_aserv_map.find(service.getInterfaceName().get());
        if (it != iface_aserv_map.end()) {
            unordered_set<std::string> servs = it->second;
            servs.erase(service.getUUID());

            if (servs.size() == 0) {
                iface_aserv_map.erase(it);
            }
        }
    }
}

void ServiceManager::removeDomains(const AnycastService& service) {
    if (service.getDomainURI()) {
        uri_serv_map_t::iterator it =
            domain_aserv_map.find(service.getDomainURI().get());
        if (it != domain_aserv_map.end()) {
            unordered_set<std::string> servs = it->second;
            servs.erase(service.getUUID());

            if (servs.size() == 0) {
                domain_aserv_map.erase(it);
            }
        }
    }
}

void ServiceManager::updateAnycastService(const AnycastService& service) {
    unique_lock<mutex> guard(serv_mutex);
    const string& uuid = service.getUUID();
    AnycastServiceState& as = aserv_map[uuid];

    // update interface name to service mapping
    if (as.service) {
        removeIfaces(*as.service);
        removeDomains(*as.service);
    }
    if (service.getInterfaceName()) {
        iface_aserv_map[service.getInterfaceName().get()].insert(uuid);
    }
    if (service.getDomainURI()) {
        domain_aserv_map[service.getDomainURI().get()].insert(uuid);
    }

    as.service = make_shared<const AnycastService>(service);

    guard.unlock();
    notifyListeners(uuid);
}

void ServiceManager::removeAnycastService(const std::string& uuid) {
    unique_lock<mutex> guard(serv_mutex);
    aserv_map_t::iterator it = aserv_map.find(uuid);
    if (it != aserv_map.end()) {
        // update interface name to service mapping
        AnycastServiceState& as = it->second;
        removeIfaces(*as.service);
        removeDomains(*as.service);

        aserv_map.erase(it);
    }

    guard.unlock();
    notifyListeners(uuid);
}

void ServiceManager::getAnycastServicesByIface(const std::string& ifaceName,
                                               /*out*/ unordered_set<string>& servs) {
    unique_lock<mutex> guard(serv_mutex);
    string_serv_map_t::const_iterator it = iface_aserv_map.find(ifaceName);
    if (it != iface_aserv_map.end()) {
        servs.insert(it->second.begin(), it->second.end());
    }
}

void ServiceManager::getAnycastServicesByDomain(const opflex::modb::URI& domain,
                                                /*out*/ unordered_set<string>& servs) {
    unique_lock<mutex> guard(serv_mutex);
    uri_serv_map_t::const_iterator it = domain_aserv_map.find(domain);
    if (it != domain_aserv_map.end()) {
        servs.insert(it->second.begin(), it->second.end());
    }
}

} /* namespace ovsagent */
