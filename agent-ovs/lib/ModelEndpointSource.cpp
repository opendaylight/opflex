/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ModelEndpointSource class.
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/ModelEndpointSource.h>
#include <opflexagent/logging.h>

#include <modelgbp/inv/LocalInventoryEp.hpp>
#include <modelgbp/inv/IfaceSecurityEnumT.hpp>
#include <modelgbp/inv/DiscoveryModeEnumT.hpp>
#include <modelgbp/domain/Config.hpp>

namespace opflexagent {

using std::shared_ptr;
using std::vector;
using boost::optional;
using opflex::modb::MAC;

ModelEndpointSource::
ModelEndpointSource(EndpointManager* manager_,
                    opflex::ofcore::OFFramework& framework_,
                    std::set<std::string> inventories_)
    : EndpointSource(manager_), framework(framework_) {
    LOG(INFO) << "Watching opflex model inventory for endpoint data";
    modelgbp::inv::LocalInventoryEp::registerListener(framework, this);

    opflex::modb::Mutator mutator(framework, "init");
    optional<shared_ptr<modelgbp::domain::Config> >
        config(modelgbp::domain::Config::resolve(framework));
    if (config) {
        for (const auto& name: inventories_) {
            config.get()->addDomainConfigToEndpointInventoryRSrc(name);
        }
    }
    mutator.commit();
}

ModelEndpointSource::~ModelEndpointSource() {
    modelgbp::inv::LocalInventoryEp::unregisterListener(framework, this);
}

static const std::string IP_TYPE_DEFAULT("default");
static const std::string IP_TYPE_VIRTUAL("virtual");
static const std::string IP_TYPE_ANYCAST_RETURN("anycast-return");

static const std::string IFACE_TYPE_INT("integration");
static const std::string IFACE_TYPE_ACCESS("access");
static const std::string IFACE_TYPE_ACCESS_UPLINK("access-uplink");

void ModelEndpointSource::objectUpdated (opflex::modb::class_id_t class_id,
                                         const opflex::modb::URI& uri) {
    using namespace modelgbp::inv;

    auto optep(LocalInventoryEp::resolve(framework, uri));
    if (!optep || !optep.get()->isUuidSet()) {
        ep_map_t::iterator it = knownEps.find(uri.toString());
        if (it != knownEps.end()) {
            LOG(INFO) << "Removed endpoint "
                      << it->second << " at " << uri;
            removeEndpoint(it->second);
            knownEps.erase(it);
        }
        return;
    }

    LocalInventoryEp& ep = *(optep.get());
    Endpoint newep;

    try {
        newep.setUUID(ep.getUuid().get());
        if (ep.isEgMappingAliasSet())
            newep.setEgMappingAlias(ep.getEgMappingAlias().get());
        if (ep.isMacSet())
            newep.setMAC(MAC(ep.getMac().get()));
        if (ep.isAccessVlanSet())
            newep.setAccessIfaceVlan(ep.getAccessVlan().get());
        newep.setPromiscuousMode(IfaceSecurityEnumT::CONST_PROMISCUOUS ==
                                 ep.getIfaceSecurity(IfaceSecurityEnumT::
                                                     CONST_NORMAL));
        newep.setDiscoveryProxyMode(DiscoveryModeEnumT::CONST_PROXY ==
                                    ep.getDiscoveryMode(DiscoveryModeEnumT::
                                                        CONST_NORMAL));
        if (ep.isAccessVlanSet())
            newep.setAccessIfaceVlan(ep.getAccessVlan().get());

        {
            auto gsrc = ep.resolveInvLocalInventoryEpToGroupRSrc();
            if (gsrc && gsrc.get()->isTargetSet()) {
                newep.setEgURI(gsrc.get()->getTargetURI().get());
            }
        }

        {
            vector<shared_ptr<LocalInventoryEpToSecGroupRSrc>> secGrps;
            ep.resolveInvLocalInventoryEpToSecGroupRSrc(secGrps);
            for(const auto& grp : secGrps) {
                if (!grp->isTargetSet()) continue;
                newep.addSecurityGroup(grp->getTargetURI().get());
            }
        }

        {
            vector<shared_ptr<Ip>> ips;
            ep.resolveInvIp(ips);
            for(const auto& ip : ips) {
                if (!ip->isIpSet()) continue;
                const auto type = ip->getType(IP_TYPE_DEFAULT);
                if (type == IP_TYPE_DEFAULT) {
                    newep.addIP(ip->getIp().get());
                } else if (type == IP_TYPE_ANYCAST_RETURN) {
                    newep.addAnycastReturnIP(ip->getIp().get());
                } else if (type == IP_TYPE_VIRTUAL) {
                    MAC mac;
                    if (ip->isMacSet()) {
                        mac = MAC(ip->getMac().get());
                    } else if (ep.isMacSet()) {
                        mac = MAC(ep.getMac().get());
                    } else {
                        continue;
                    }

                    newep.addVirtualIP(std::make_pair(mac, ip->getIp().get()));
                }
            }
        }

        {
            vector<shared_ptr<Interface>> ifaces;
            ep.resolveInvInterface(ifaces);
            for(const auto& iface : ifaces) {
                if (!iface->isNameSet()) continue;
                const auto type = iface->getType(IP_TYPE_DEFAULT);
                if (type == IFACE_TYPE_INT) {
                    newep.setInterfaceName(iface->getName().get());
                } else if (type == IFACE_TYPE_ACCESS) {
                    newep.setAccessInterface(iface->getName().get());
                } else if (type == IFACE_TYPE_ACCESS_UPLINK) {
                    newep.setAccessUplinkInterface(iface->getName().get());
                }
            }
        }

        {
            vector<shared_ptr<Attribute>> attrs;
            ep.resolveInvAttribute(attrs);
            for(const auto& attr : attrs) {
                if (!attr->isNameSet()) continue;
                newep.addAttribute(attr->getName().get(),
                                   attr->getValue(""));
            }
        }

        {
            auto dhcpv4 = ep.resolveInvDHCPv4Config();
            if (dhcpv4) {
                Endpoint::DHCPv4Config newd;
                if (dhcpv4.get()->isIpSet())
                    newd.setIpAddress(dhcpv4.get()->getIp().get());
                if (dhcpv4.get()->isPrefixLenSet())
                    newd.setPrefixLen(dhcpv4.get()->getPrefixLen().get());
                if (dhcpv4.get()->isServerIpSet())
                    newd.setServerIp(dhcpv4.get()->getServerIp().get());
                if (dhcpv4.get()->isDomainSet())
                    newd.setDomain(dhcpv4.get()->getDomain().get());
                if (dhcpv4.get()->isInterfaceMTUSet())
                    newd.setInterfaceMtu(dhcpv4.get()->getInterfaceMTU().get());
                if (dhcpv4.get()->isLeaseTimeSet())
                    newd.setLeaseTime(dhcpv4.get()->getLeaseTime().get());

                vector<shared_ptr<Router>> routers;
                dhcpv4.get()->resolveInvRouter(routers);
                for (auto& router : routers) {
                    if (!router->isNameSet()) continue;
                    newd.addRouter(router->getName().get());
                }
                vector<shared_ptr<DNSServer>> dnsServers;
                dhcpv4.get()->resolveInvDNSServer(dnsServers);
                for (auto& dns : dnsServers) {
                    if (!dns->isNameSet()) continue;
                    newd.addDnsServer(dns->getName().get());
                }

                vector<shared_ptr<StaticRoute>> staticRoutes;
                dhcpv4.get()->resolveInvStaticRoute(staticRoutes);
                for (auto& staticRoute : staticRoutes) {
                    if (!staticRoute->isDestSet() ||
                        !staticRoute->isNextHopSet() ||
                        !staticRoute->isPrefixLenSet()) continue;
                    newd.addStaticRoute(staticRoute->getDest().get(),
                                        staticRoute->getPrefixLen().get(),
                                        staticRoute->getNextHop().get());
                }
                newep.setDHCPv4Config(newd);
            }
        }

        {
            auto dhcpv6 = ep.resolveInvDHCPv6Config();
            if (dhcpv6) {
                Endpoint::DHCPv6Config newd;
                if (dhcpv6.get()->isT1Set())
                    newd.setT1(dhcpv6.get()->getT1().get());
                if (dhcpv6.get()->isT2Set())
                    newd.setT2(dhcpv6.get()->getT2().get());
                if (dhcpv6.get()->isValidLifetimeSet())
                    newd.setValidLifetime(dhcpv6.get()->
                                          getValidLifetime().get());
                if (dhcpv6.get()->isPreferredLifetimeSet())
                    newd.setPreferredLifetime(dhcpv6.get()->
                                              getPreferredLifetime().get());

                vector<shared_ptr<SearchListEntry>> searchList;
                dhcpv6.get()->resolveInvSearchListEntry(searchList);
                for (auto& searchListEntry : searchList) {
                    if (!searchListEntry->isNameSet()) continue;
                    newd.addSearchListEntry(searchListEntry->getName().get());
                }
                vector<shared_ptr<DNSServer>> dnsServers;
                dhcpv6.get()->resolveInvDNSServer(dnsServers);
                for (auto& dns : dnsServers) {
                    if (!dns->isNameSet()) continue;
                    newd.addDnsServer(dns->getName().get());
                }
                newep.setDHCPv6Config(newd);
            }
        }

        {
            vector<shared_ptr<IpMapping>> ipMappings;
            ep.resolveInvIpMapping(ipMappings);
            for(const auto& ipMapping : ipMappings) {
                if (!ipMapping->isNameSet()) continue;

                Endpoint::IPAddressMapping newipm(ipMapping->getName().get());
                if (ipMapping.get()->isMappedIpSet())
                    newipm.setMappedIP(ipMapping.get()->getMappedIp().get());
                if (ipMapping.get()->isFloatingIpSet())
                    newipm.setFloatingIP(ipMapping.get()->
                                         getFloatingIp().get());
                if (ipMapping.get()->isNextHopIfSet())
                    newipm.setNextHopIf(ipMapping.get()->getNextHopIf().get());
                if (ipMapping.get()->isNextHopMacSet())
                    newipm.setNextHopMAC(MAC(ipMapping.get()->
                                             getNextHopMac().get()));

                auto gsrc = ipMapping.get()->
                    resolveInvIpMappingToGroupRSrc();
                if (gsrc && gsrc.get()->isTargetSet()) {
                    newipm.setEgURI(gsrc.get()->getTargetURI().get());
                }

                newep.addIPAddressMapping(newipm);
            }
        }

        knownEps[uri.toString()] = newep.getUUID();
        updateEndpoint(newep);

        LOG(INFO) << "Updated endpoint " << newep
                  << " from " << uri;
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not add endpoint for "
                   << uri << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while adding endpoint for "
                   << uri;
    }
}

} /* namespace opflexagent */
