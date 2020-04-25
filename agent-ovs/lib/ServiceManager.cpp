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
#include <opflexagent/Agent.h>
#include <opflexagent/EndpointManager.h>
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

#ifdef HAVE_PROMETHEUS_SUPPORT
ServiceManager::ServiceManager (Agent& agent_,
                                opflex::ofcore::OFFramework& framework_,
                                PrometheusManager& prometheusManager_)
    : agent(agent_), framework(framework_),
      prometheusManager(prometheusManager_) {
}
#else
ServiceManager::ServiceManager (Agent& agent_,
                                opflex::ofcore::OFFramework& framework_)
    : agent(agent_), framework(framework_) {
}
#endif

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

void ServiceManager::clearSvcCounterStats (const Service& service,
                                           shared_ptr<SvcCounter> pSvc,
                                           shared_ptr<SvcTargetCounter> pSvcTgt)
{
    auto stRxPktCount = pSvcTgt->getRxpackets(0);
    auto stRxByteCount = pSvcTgt->getRxbytes(0);
    auto stTxPktCount = pSvcTgt->getTxpackets(0);
    auto stTxByteCount = pSvcTgt->getTxbytes(0);
    auto stNodePortRxPktCount = pSvcTgt->getNodePortRxpackets(0);
    auto stNodePortRxByteCount = pSvcTgt->getNodePortRxbytes(0);
    auto stNodePortTxPktCount = pSvcTgt->getNodePortTxpackets(0);
    auto stNodePortTxByteCount = pSvcTgt->getNodePortTxbytes(0);
    auto sRxPktCount = pSvc->getRxpackets(0);
    auto sRxByteCount = pSvc->getRxbytes(0);
    auto sTxPktCount = pSvc->getTxpackets(0);
    auto sTxByteCount = pSvc->getTxbytes(0);
    auto sNodePortRxPktCount = pSvc->getNodePortRxpackets(0);
    auto sNodePortRxByteCount = pSvc->getNodePortRxbytes(0);
    auto sNodePortTxPktCount = pSvc->getNodePortTxpackets(0);
    auto sNodePortTxByteCount = pSvc->getNodePortTxbytes(0);
    pSvc->setRxpackets(sRxPktCount - stRxPktCount)
         .setRxbytes(sRxByteCount - stRxByteCount)
         .setTxpackets(sTxPktCount - stTxPktCount)
         .setTxbytes(sTxByteCount - stTxByteCount)
         .setNodePortRxpackets(sNodePortRxPktCount - stNodePortRxPktCount)
         .setNodePortRxbytes(sNodePortRxByteCount - stNodePortRxByteCount)
         .setNodePortTxpackets(sNodePortTxPktCount - stNodePortTxPktCount)
         .setNodePortTxbytes(sNodePortTxByteCount - stNodePortTxByteCount);
#ifdef HAVE_PROMETHEUS_SUPPORT
    prometheusManager.addNUpdateSvcCounter(service.getUUID(),
                                   pSvc->getRxbytes(0),
                                   pSvc->getRxpackets(0),
                                   pSvc->getTxbytes(0),
                                   pSvc->getTxpackets(0),
                                   attr_map_t());
    if (!service.isExternal()) {
        prometheusManager.addNUpdateSvcCounter("nodeport-"+service.getUUID(),
                                       pSvc->getNodePortRxbytes(0),
                                       pSvc->getNodePortRxpackets(0),
                                       pSvc->getNodePortTxbytes(0),
                                       pSvc->getNodePortTxpackets(0),
                                       attr_map_t(), true);
    }
#endif
}

/* Populate MODB with service target observer */
void
ServiceManager::updateSvcTargetObserverMoDB (const opflexagent::Service& service,
                                             bool add,
                                             shared_ptr<SvcCounter> pSvcCounter)
{
    using namespace opflex::modb;
    using modelgbp::observer::SvcStatUniverse;

    LOG(DEBUG) << "Updating obs modb for service target: " << service.getUUID()
               << " add: " << add;

    std::vector<shared_ptr<SvcTargetCounter> > out;
    if (add) {
        pSvcCounter->resolveGbpeSvcTargetCounter(out);
    }

    for (const auto& sm : service.getServiceMappings()) {
        for (const auto& ip : sm.getNextHopIPs()) {
            optional<shared_ptr<SvcTargetCounter> > opSvcTarget =
                                pSvcCounter->resolveGbpeSvcTargetCounter(ip);
            if (add) {
                shared_ptr<SvcTargetCounter> pSvcTarget = nullptr;
                if (!opSvcTarget) {
                    pSvcTarget = pSvcCounter->addGbpeSvcTargetCounter(ip);
#ifdef HAVE_PROMETHEUS_SUPPORT
                    // Keeping prom and modb in sync. Create the metric with IP annotation.
                    // depending on EP being local, intflowmanager will update the
                    // metric with more annotations. If EP IP is deleted or becomes external,
                    // then the annotations other than IP will be removed and counters
                    // reset back to 0
                    prometheusManager.addNUpdateSvcTargetCounter(service.getUUID(),
                                                                 ip,
                                                                 0, 0, 0, 0,
                                                                 service.getAttributes(),
                                                                 attr_map_t(),
                                                                 true);
                    if (!service.isExternal()) {
                        prometheusManager.addNUpdateSvcTargetCounter("nodeport-"+service.getUUID(),
                                                             ip,
                                                             0, 0, 0, 0,
                                                             service.getAttributes(),
                                                             attr_map_t(),
                                                             true, true);
                    }
#endif
                } else {
                    pSvcTarget = opSvcTarget.get();
#ifdef HAVE_PROMETHEUS_SUPPORT
                    // If service attributes are changing, and if flows are created for this
                    // svc-tgt, then IntFlowManager will take care of annotation updates. But
                    // if flows dont get created, following will update the right annotations.
                    // the pod specific metrics will get updated if the flow gets created for
                    // this next-hop (depending on the state of EP).
                    prometheusManager.addNUpdateSvcTargetCounter(service.getUUID(),
                                                                 ip,
                                                                 pSvcTarget->getRxbytes(0),
                                                                 pSvcTarget->getRxpackets(0),
                                                                 pSvcTarget->getTxbytes(0),
                                                                 pSvcTarget->getTxpackets(0),
                                                                 service.getAttributes(),
                                                                 attr_map_t(),
                                                                 true);
                    if (!service.isExternal()) {
                        prometheusManager.addNUpdateSvcTargetCounter("nodeport-"+service.getUUID(),
                                                             ip,
                                                             pSvcTarget->getNodePortRxbytes(0),
                                                             pSvcTarget->getNodePortRxpackets(0),
                                                             pSvcTarget->getNodePortTxbytes(0),
                                                             pSvcTarget->getNodePortTxpackets(0),
                                                             service.getAttributes(),
                                                             attr_map_t(),
                                                             true, true);
                    } else {
                        prometheusManager.removeSvcTargetCounter("nodeport-"+service.getUUID(), ip);
                    }
#endif
                }
                for (size_t idx=0; idx < out.size(); idx++) {
                    if (out[idx]->getIp("") == ip)
                        out.erase(out.begin()+idx);
                }
            } else {
                if (opSvcTarget) {
                    clearSvcCounterStats(service, pSvcCounter, opSvcTarget.get());
#ifdef HAVE_PROMETHEUS_SUPPORT
                    prometheusManager.removeSvcTargetCounter(service.getUUID(), ip);
                    if (!service.isExternal())
                        prometheusManager.removeSvcTargetCounter("nodeport-"+service.getUUID(), ip);
#endif
                    opSvcTarget.get()->remove();
                }
            }
        }
    }

    // Remove deleted service targets
    for (auto& pSvcTarget : out) {
        clearSvcCounterStats(service, pSvcCounter, pSvcTarget);
#ifdef HAVE_PROMETHEUS_SUPPORT
        auto nhip = pSvcTarget->getIp();
        if (nhip) {
            prometheusManager.removeSvcTargetCounter(service.getUUID(), nhip.get());
            if (!service.isExternal())
                prometheusManager.removeSvcTargetCounter("nodeport-"+service.getUUID(), nhip.get());
        }
#endif
        pSvcTarget->remove();
    }
}

/* Populate MODB with service observer */
void
ServiceManager::updateSvcObserverMoDB (const opflexagent::Service& service, bool add)
{
    using namespace opflex::modb;
    using modelgbp::observer::SvcStatUniverse;

    Mutator mutator(framework, "policyelement");

    optional<shared_ptr<SvcStatUniverse> > ssu =
                          SvcStatUniverse::resolve(framework);
    if (!ssu)
        return;

    LOG(DEBUG) << "Updating obs modb for service: " << service.getUUID()
               << " add: " << add;

    optional<shared_ptr<SvcCounter> > opService =
                    ssu.get()->resolveGbpeSvcCounter(service.getUUID());
    if (add) {
        Service::ServiceMode mode = service.getServiceMode();
        if (mode != Service::ServiceMode::LOADBALANCER) {
            LOG(DEBUG) << "Service stats not supported for non-LB services";
            // clear obs and prom metrics during update;
            // below will be no-op during create
            updateSvcObserverMoDB(service, false);
            return;
        }

        shared_ptr<SvcCounter> pService = nullptr;
        if (opService)
            pService = opService.get();
        else {
            pService = ssu.get()->addGbpeSvcCounter(service.getUUID());
#ifdef HAVE_PROMETHEUS_SUPPORT
            prometheusManager.incSvcCounter();
#endif
        }

        const Service::attr_map_t& svcAttr = service.getAttributes();

        auto nameItr = svcAttr.find("name");
        if (nameItr != svcAttr.end())
            pService->setName(nameItr->second);
        else
            pService->unsetName();

        auto nsItr = svcAttr.find("namespace");
        if (nsItr != svcAttr.end())
            pService->setNamespace(nsItr->second);
        else
            pService->unsetNamespace();

        auto scopeItr = svcAttr.find("scope");
        if (scopeItr != svcAttr.end()) {
            pService->setScope(scopeItr->second);
            updateSvcTargetObserverMoDB(service, true, pService);
        } else
            pService->unsetScope();

#ifdef HAVE_PROMETHEUS_SUPPORT
        prometheusManager.addNUpdateSvcCounter(service.getUUID(),
                                               pService->getRxbytes(0),
                                               pService->getRxpackets(0),
                                               pService->getTxbytes(0),
                                               pService->getTxpackets(0),
                                               svcAttr);
        if (!service.isExternal()) {
            prometheusManager.addNUpdateSvcCounter("nodeport-"+service.getUUID(),
                                               pService->getNodePortRxbytes(0),
                                               pService->getNodePortRxpackets(0),
                                               pService->getNodePortTxbytes(0),
                                               pService->getNodePortTxpackets(0),
                                               svcAttr, true);
        } else {
            // cluster/nodeport service pods are a superset of the external LB
            // service pods. We create nodeport metrics only for cluster services.
            // If a service moves from cluster => external, then remove these metrics.
            // This is not expected to happen, but keeping the agent generic.
            prometheusManager.removeSvcCounter("nodeport-"+service.getUUID());
        }
#endif
    } else {
        if (opService) {
            updateSvcTargetObserverMoDB(service, false, opService.get());
            opService.get()->remove();
#ifdef HAVE_PROMETHEUS_SUPPORT
            prometheusManager.decSvcCounter();
            prometheusManager.removeSvcCounter(service.getUUID());
            if (!service.isExternal())
                prometheusManager.removeSvcCounter("nodeport-"+service.getUUID());
#endif
        }
    }

    mutator.commit();
}

/* Populate MODB with configs of services, service mappings and attributes */
void
ServiceManager::updateConfigMoDB (const opflexagent::Service& service, bool add)
{
    using namespace modelgbp::svc;
    using namespace opflex::modb;
    Mutator mutator(framework, "policyelement");

    optional<shared_ptr<ServiceUniverse> > su =
                          ServiceUniverse::resolve(framework);
    if (!su)
        return;

    LOG(DEBUG) << "Updating cfg modb for service: " << service.getUUID()
               << " add: " << add;
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
        if (opService) {
            std::vector<shared_ptr<ServiceMapping> > outSM;
            opService.get()->resolveSvcServiceMapping(outSM);
            for (auto& sm : outSM)
                sm->remove();
            std::vector<shared_ptr<ServiceAttribute> > outSA;
            opService.get()->resolveSvcServiceAttribute(outSA);
            for (auto& sa : outSA)
                sa->remove();
            opService.get()->remove();
        }
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

    // During service update, today host agent creates a new service file always
    // So we wont have a case of actual service prop update.
    // Note: If we are doing actual service file update, then the prior Service for
    // that UUID appears to get quashed above, and the new updated Service state gets processed
    // within ServiceManager. In such a case, keeping MoDB also upto date with the service
    // state by doing a delete/readd of the service object.
    // We can keep a rolling hash to detect MoDB update, but considering that the service files
    // are never updated today, we can revisit hashing changes later.
    updateConfigMoDB(service, false);
    updateConfigMoDB(service, true);

    updateSvcObserverMoDB(service, true);

    guard.unlock();
    notifyListeners(uuid);
}

void ServiceManager::removeService(const std::string& uuid) {
    unique_lock<mutex> guard(serv_mutex);
    aserv_map_t::iterator it = aserv_map.find(uuid);
    if (it != aserv_map.end()) {
        // update interface name to service mapping
        ServiceState& as = it->second;
        updateSvcObserverMoDB(*as.service, false);
        updateConfigMoDB(*as.service, false);
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
