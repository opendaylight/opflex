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

#include <opflexagent/ServiceManager.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using std::string;
using std::unordered_set;
using std::shared_ptr;
using std::make_shared;
using std::unique_lock;
using std::mutex;
using boost::optional;

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
    for (ServiceListener* listener : serviceListeners) {
        listener->serviceUpdated(uuid);
    }
}

shared_ptr<const Service>
ServiceManager::getService(const string& uuid) {
    unique_lock<mutex> guard(serv_mutex);
    aserv_map_t::const_iterator it = aserv_map.find(uuid);
    if (it != aserv_map.end())
        return it->second.service;
    return shared_ptr<const Service>();
}

void ServiceManager::removeIfaces(const Service& service) {
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

void ServiceManager::removeDomains(const Service& service) {
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

void ServiceManager::updateService(const Service& service) {
    unique_lock<mutex> guard(serv_mutex);
    const string& uuid = service.getUUID();
    ServiceState& as = aserv_map[uuid];

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

    as.service = make_shared<const Service>(service);

    guard.unlock();
    notifyListeners(uuid);
}

void ServiceManager::removeService(const std::string& uuid) {
    unique_lock<mutex> guard(serv_mutex);
    aserv_map_t::iterator it = aserv_map.find(uuid);
    if (it != aserv_map.end()) {
        // update interface name to service mapping
        ServiceState& as = it->second;
        removeIfaces(*as.service);
        removeDomains(*as.service);

        aserv_map.erase(it);
    }

    guard.unlock();
    notifyListeners(uuid);
}

void ServiceManager::getServicesByIface(const std::string& ifaceName,
                                               /*out*/ unordered_set<string>& servs) {
    unique_lock<mutex> guard(serv_mutex);
    string_serv_map_t::const_iterator it = iface_aserv_map.find(ifaceName);
    if (it != iface_aserv_map.end()) {
        servs.insert(it->second.begin(), it->second.end());
    }
}

void ServiceManager::getServicesByDomain(const opflex::modb::URI& domain,
                                                /*out*/ unordered_set<string>& servs) {
    unique_lock<mutex> guard(serv_mutex);
    uri_serv_map_t::const_iterator it = domain_aserv_map.find(domain);
    if (it != domain_aserv_map.end()) {
        servs.insert(it->second.begin(), it->second.end());
    }
}

} /* namespace opflexagent */
