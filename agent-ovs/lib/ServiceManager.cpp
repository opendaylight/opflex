/* -*- C++ -*-; c-basic-offset: 5; indent-tabs-mode: nil */
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
#include <opflex/modb/Mutator.h>
#include <modelgbp/svc/ServiceUniverse.hpp>
#include <modelgbp/svc/ServiceModeEnumT.hpp>
#include <modelgbp/svc/ConnTrackEnumT.hpp>
#include <modelgbp/gbpe/EncapTypeEnumT.hpp>

namespace opflexagent {

using std::string;
using std::unordered_set;
using std::shared_ptr;
using std::make_shared;
using std::unique_lock;
using std::mutex;
using boost::optional;

ServiceManager::ServiceManager (Agent& agent_,
                                opflex::ofcore::OFFramework& framework_)
    : agent(agent_), framework(framework_) {
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
            unordered_set<std::string>& servs = it->second;
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
            unordered_set<std::string>& servs = it->second;
            servs.erase(service.getUUID());

            if (servs.size() == 0) {
                domain_aserv_map.erase(it);
            }
        }
    }
}

/* Populate MODB with services, service mappings and attributes */
void
ServiceManager::updateMoDB (const opflexagent::Service& service, bool add)
{
    using namespace modelgbp::svc;
    using namespace modelgbp::gbpe;
    using namespace opflex::modb;
    Mutator mutator(framework, "policyelement");

    optional<shared_ptr<ServiceUniverse> > su =
                          ServiceUniverse::resolve(framework);
    if (!su)
        return;

    optional<shared_ptr<modelgbp::svc::Service> > opService =
                    su.get()->resolveSvcService(service.getUUID());

    if (add) {
        shared_ptr<modelgbp::svc::Service> pService = nullptr;
        if (opService)
            pService = opService.get();
        else
            pService = su.get()->addSvcService(service.getUUID());
        // Populate service props
        const optional<URI>& domain = service.getDomainURI();
        if (domain)
            pService->setDom(domain.get().toString());
        else
            pService->unsetDom();

        const optional<string>& interfaceName = service.getInterfaceName();
        if (interfaceName)
            pService->setInterfaceName(interfaceName.get());
        else
            pService->unsetInterfaceName();

        const optional<string>& interfaceIP = service.getIfaceIP();
        if (interfaceIP)
            pService->setInterfaceIP(interfaceIP.get());
        else
            pService->unsetInterfaceIP();

        const optional<MAC>& mac = service.getServiceMAC();
        if (mac)
            pService->setMac(mac.get());
        else
            pService->unsetMac();

        Service::ServiceMode mode = service.getServiceMode();
        if (mode == Service::ServiceMode::LOCAL_ANYCAST)
            pService->setMode(ServiceModeEnumT::CONST_ANYCAST);
        else if (mode == Service::ServiceMode::LOADBALANCER)
            pService->setMode(ServiceModeEnumT::CONST_LB);
        else
            pService->unsetMode();

        const optional<uint16_t>& ifaceVlan = service.getIfaceVlan();
        if (ifaceVlan) {
            pService->setInterfaceEncapType(EncapTypeEnumT::CONST_VLAN);
            pService->setInterfaceEncapId(ifaceVlan.get());
        } else {
            pService->unsetInterfaceEncapType();
            pService->unsetInterfaceEncapId();
        }

        // Populate service attributes
        const Service::attr_map_t& attr_map = service.getAttributes();
        for (const std::pair<const string, string>& ap : attr_map) {
            shared_ptr<ServiceAttribute> sa =
                        pService->addSvcServiceAttribute(ap.first);
            sa->setValue(ap.second);
        }

        // Populate service mappings
        for (const auto& sm : service.getServiceMappings()) {
            const auto& proto = sm.getServiceProto();
            const auto& ip = sm.getServiceIP();
            const auto& port = sm.getServicePort();
            if (proto && ip && port) {
                shared_ptr<ServiceMapping> pSM =
                        pService->addSvcServiceMapping(ip.get(), proto.get(), port.get());

                if (sm.isConntrackMode())
                    pSM->setConnectionTracking(ConnTrackEnumT::CONST_ENABLED);
                else
                    pSM->setConnectionTracking(ConnTrackEnumT::CONST_DISABLED);

                const auto& nhPort = sm.getNextHopPort();
                if (nhPort)
                    pSM->setNexthopPort(nhPort.get());
                else
                    pSM->unsetNexthopPort();

                const auto& gwIP = sm.getGatewayIP();
                if (gwIP)
                    pSM->setGatewayIP(gwIP.get());
                else
                    pSM->unsetGatewayIP();

                // Populate next hop IPs of service mapping
                for (const auto& ip : sm.getNextHopIPs())
                    shared_ptr<NexthopIP> nhIP = pSM->addSvcNexthopIP(ip);
            }
        }
    } else {
        if (opService)
            opService.get()->remove();
    }

    mutator.commit();
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

    updateMoDB(service, true);

    guard.unlock();
    notifyListeners(uuid);
}

void ServiceManager::removeService(const std::string& uuid) {
    unique_lock<mutex> guard(serv_mutex);
    aserv_map_t::iterator it = aserv_map.find(uuid);
    if (it != aserv_map.end()) {
        // update interface name to service mapping
        ServiceState& as = it->second;
        updateMoDB(*as.service, false);
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

template <typename M>
static void getSvcs(const M& map, /* out */ unordered_set<string>& svcs) {
    for (const auto& elem : map) {
        svcs.insert(elem.first);
    }
}

void ServiceManager::getServiceUUIDs ( /* out */
                        std::unordered_set<std::string> &svcs)
{
    unique_lock<mutex> guard(serv_mutex);
    getSvcs(aserv_map, svcs);
}

} /* namespace opflexagent */
