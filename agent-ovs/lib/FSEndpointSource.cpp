/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSEndpointSource class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <config.h>
#if defined(HAVE_SYS_INOTIFY_H) && defined(HAVE_SYS_EVENTFD_H)
#define USE_INOTIFY
#endif

#include <stdexcept>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <opflex/modb/URIBuilder.h>

#include <opflexagent/FSEndpointSource.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::runtime_error;
using std::make_pair;
using opflex::modb::URI;
using opflex::modb::MAC;

FSEndpointSource::FSEndpointSource(EndpointManager* manager_,
                                   FSWatcher& listener,
                                   const std::string& endpointDir)
    : EndpointSource(manager_) {
    LOG(INFO) << "Watching " << endpointDir << " for endpoint data";
    listener.addWatch(endpointDir, *this);
}

static bool isep(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".ep") &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSEndpointSource::updated(const fs::path& filePath) {
    if (!isep(filePath)) return;

    static const std::string EP_UUID("uuid");
    static const std::string EP_MAC("mac");
    static const std::string EP_IP("ip");
    static const std::string EP_ANYCAST_RETURN_IP("anycast-return-ip");
    static const std::string EP_VIRTUAL_IP("virtual-ip");
    static const std::string EP_GROUP("endpoint-group");
    static const std::string POLICY_SPACE_NAME("policy-space-name");
    static const std::string EG_POLICY_SPACE("eg-policy-space");
    static const std::string EG_MAPPING_ALIAS("eg-mapping-alias");
    static const std::string EP_GROUP_NAME("endpoint-group-name");
    static const std::string EP_SEC_GROUP("security-group");
    static const std::string SEC_GROUP_POLICY_SPACE("policy-space");
    static const std::string SEC_GROUP_NAME("name");
    static const std::string EP_IFACE_NAME("interface-name");
    static const std::string EP_ACCESS_IFACE("access-interface");
    static const std::string EP_ACCESS_IFACE_VLAN("access-interface-vlan");
    static const std::string EP_ACCESS_UPLINK_IFACE("access-uplink-interface");
    static const std::string EP_PROMISCUOUS("promiscuous-mode");
    static const std::string EP_DISC_PROXY("discovery-proxy-mode");
    static const std::string EP_NAT_MODE("nat-mode");
    static const std::string EP_ATTRIBUTE_VM_NAME("vm-name");
    static const std::string EP_ATTRIBUTES("attributes");

    static const std::string DHCP4("dhcp4");
    static const std::string DHCP6("dhcp6");
    static const std::string DHCP_IP("ip");
    static const std::string DHCP_PREFIX_LEN("prefix-len");
    static const std::string DHCP_SERVER_IP("server-ip");
    static const std::string DHCP_SERVER_MAC("server-mac");
    static const std::string DHCP_ROUTERS("routers");
    static const std::string DHCP_DNS_SERVERS("dns-servers");
    static const std::string DHCP_DOMAIN("domain");
    static const std::string DHCP_SEARCH_LIST("search-list");
    static const std::string DHCP_STATIC_ROUTES("static-routes");
    static const std::string DHCP_STATIC_ROUTE_DEST("dest");
    static const std::string DHCP_STATIC_ROUTE_DEST_PREFIX("dest-prefix");
    static const std::string DHCP_STATIC_ROUTE_NEXTHOP("next-hop");
    static const std::string DHCP_INTERFACE_MTU("interface-mtu");
    static const std::string DHCP_LEASE_TIME("lease-time");
    static const std::string DHCP_T1("t1");
    static const std::string DHCP_T2("t2");
    static const std::string DHCP_PREFERRED_LIFETIME("preferred-lifetime");
    static const std::string DHCP_VALID_LIFETIME("valid-lifetime");

    static const std::string IP_ADDRESS_MAPPING("ip-address-mapping");
    static const std::string IPM_MAPPED_IP("mapped-ip");
    static const std::string IPM_FLOATING_IP("floating-ip");
    static const std::string IPM_NEXTHOP_IF("next-hop-if");
    static const std::string IPM_NEXTHOP_MAC("next-hop-mac");

    static const std::string SNAT_IP("snat-ip");

    try {
        using boost::property_tree::ptree;
        Endpoint newep;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);
        newep.setUUID(properties.get<string>(EP_UUID));
        optional<string> mac = properties.get_optional<string>(EP_MAC);
        if (mac) {
            newep.setMAC(MAC(mac.get()));
        }
        optional<ptree&> ips = properties.get_child_optional(EP_IP);
        if (ips) {
            for (const ptree::value_type &v : ips.get())
                newep.addIP(v.second.data());
        }
        optional<ptree&> anycastReturnIps =
            properties.get_child_optional(EP_ANYCAST_RETURN_IP);
        if (anycastReturnIps) {
            for (const ptree::value_type &v : anycastReturnIps.get())
                newep.addAnycastReturnIP(v.second.data());
        }
        optional<ptree&> virtualIps =
            properties.get_child_optional(EP_VIRTUAL_IP);
        if (virtualIps) {
            for (const ptree::value_type &v : virtualIps.get()) {
                 optional<string> vmac =
                     v.second.get_optional<string>(EP_MAC);
                 optional<string> vip =
                     v.second.get_optional<string>(EP_IP);
                 if (vip) {
                     if (vmac) {
                         newep.addVirtualIP(make_pair(MAC(vmac.get()),
                                                      vip.get()));
                     } else if (mac) {
                         newep.addVirtualIP(make_pair(MAC(mac.get()),
                                                      vip.get()));
                     }
                 }
            }
        }

        optional<string> eg =
            properties.get_optional<string>(EP_GROUP);
        if (eg) {
            newep.setEgURI(URI(eg.get()));
        } else {
            optional<string> eg_name =
                properties.get_optional<string>(EP_GROUP_NAME);
            optional<string> ps_name =
                properties.get_optional<string>(EG_POLICY_SPACE);
            if (!ps_name)
                ps_name = properties.get_optional<string>(POLICY_SPACE_NAME);
            if (eg_name && ps_name) {
                newep.setEgURI(opflex::modb::URIBuilder()
                               .addElement("PolicyUniverse")
                               .addElement("PolicySpace")
                               .addElement(ps_name.get())
                               .addElement("GbpEpGroup")
                               .addElement(eg_name.get()).build());
            } else {
                optional<string> eg_mapping_alias =
                    properties.get_optional<string>(EG_MAPPING_ALIAS);
                if (eg_mapping_alias) {
                    newep.setEgMappingAlias(eg_mapping_alias.get());
                }
            }
        }

        optional<ptree&> secGrps =
            properties.get_child_optional(EP_SEC_GROUP);
        if (secGrps) {
            for (const ptree::value_type &v : secGrps.get()) {
                optional<string> secGrpPS =
                    v.second.get_optional<string>(SEC_GROUP_POLICY_SPACE);
                optional<string> secGrpName =
                    v.second.get_optional<string>(SEC_GROUP_NAME);
                if (secGrpName && secGrpPS) {
                    newep.addSecurityGroup(opflex::modb::URIBuilder()
                                           .addElement("PolicyUniverse")
                                           .addElement("PolicySpace")
                                           .addElement(secGrpPS.get())
                                           .addElement("GbpSecGroup")
                                           .addElement(secGrpName.get())
                                           .build());
                }
            }
        }

        optional<string> iface =
            properties.get_optional<string>(EP_IFACE_NAME);
        if (iface)
            newep.setInterfaceName(iface.get());
        optional<string> accessIface =
            properties.get_optional<string>(EP_ACCESS_IFACE);
        if (accessIface)
            newep.setAccessInterface(accessIface.get());
        optional<uint16_t> accessIfaceVlan =
            properties.get_optional<uint16_t>(EP_ACCESS_IFACE_VLAN);
        if (accessIfaceVlan)
            newep.setAccessIfaceVlan(accessIfaceVlan.get());
        optional<string> accessUplinkIface =
            properties.get_optional<string>(EP_ACCESS_UPLINK_IFACE);
        if (accessUplinkIface)
            newep.setAccessUplinkInterface(accessUplinkIface.get());
        optional<bool> promisc =
            properties.get_optional<bool>(EP_PROMISCUOUS);
        if (promisc)
            newep.setPromiscuousMode(promisc.get());
        optional<bool> discprox =
            properties.get_optional<bool>(EP_DISC_PROXY);
        if (discprox)
            newep.setDiscoveryProxyMode(discprox.get());
        optional<bool> natMode =
            properties.get_optional<bool>(EP_NAT_MODE);
        if (natMode)
            newep.setNatMode(natMode.get());

        optional<ptree&> attrs =
            properties.get_child_optional(EP_ATTRIBUTES);
        if (attrs) {
            for (const ptree::value_type &v : attrs.get()) {
                newep.addAttribute(v.first, v.second.data());
                if (v.first == EP_ATTRIBUTE_VM_NAME &&
                    // vm-name attribute starts with snat|
                    v.second.data().rfind("snat|", 0) == 0) {
                    newep.setNatMode(true);
                }
            }
        }

        optional<ptree&> dhcp4 = properties.get_child_optional(DHCP4);
        if (dhcp4) {
            Endpoint::DHCPv4Config c;

            optional<string> ip =
                dhcp4.get().get_optional<string>(DHCP_IP);
            if (ip)
                c.setIpAddress(ip.get());

            optional<string> serverIp =
                dhcp4.get().get_optional<string>(DHCP_SERVER_IP);
            if (serverIp)
                c.setServerIp(serverIp.get());

            optional<string> serverMac =
                dhcp4.get().get_optional<string>(DHCP_SERVER_MAC);
            if (serverMac)
                c.setServerMac(MAC(serverMac.get()));

            optional<uint8_t> prefix =
                dhcp4.get().get_optional<uint8_t>(DHCP_PREFIX_LEN);
            if (prefix)
                c.setPrefixLen(prefix.get());

            optional<ptree&> routers =
                dhcp4.get().get_child_optional(DHCP_ROUTERS);
            if (routers) {
                for (const ptree::value_type &u : routers.get())
                    c.addRouter(u.second.data());
            }

            optional<ptree&> dns =
                dhcp4.get().get_child_optional(DHCP_DNS_SERVERS);
            if (dns) {
                for (const ptree::value_type &u : dns.get())
                    c.addDnsServer(u.second.data());
            }

            optional<string> domain =
                dhcp4.get().get_optional<string>(DHCP_DOMAIN);
            if (domain)
                c.setDomain(domain.get());

            optional<ptree&> staticRoutes =
                dhcp4.get().get_child_optional(DHCP_STATIC_ROUTES);
            if (staticRoutes) {
                for (const ptree::value_type &u : staticRoutes.get()) {
                    optional<string> dst = u.second.get_optional<string>
                        (DHCP_STATIC_ROUTE_DEST);
                    uint8_t dstPrefix =
                        u.second.get<uint8_t>
                        (DHCP_STATIC_ROUTE_DEST_PREFIX, 32);
                    optional<string> nextHop = u.second.get_optional<string>
                        (DHCP_STATIC_ROUTE_NEXTHOP);
                    if (dst && nextHop)
                            c.addStaticRoute(dst.get(),
                                             dstPrefix,
                                             nextHop.get());
                }
            }

            optional<uint16_t> interfaceMtu =
                dhcp4.get().get_optional<uint16_t>(DHCP_INTERFACE_MTU);
            if (interfaceMtu)
                c.setInterfaceMtu(interfaceMtu.get());

            optional<uint32_t> leaseTime =
                dhcp4.get().get_optional<uint32_t>(DHCP_LEASE_TIME);
            if (leaseTime)
                c.setLeaseTime(leaseTime.get());

            newep.setDHCPv4Config(c);
        }

        optional<ptree&> dhcp6 = properties.get_child_optional(DHCP6);
        if (dhcp6) {
            Endpoint::DHCPv6Config c;

            optional<ptree&> searchPath =
                dhcp6.get().get_child_optional(DHCP_SEARCH_LIST);
            if (searchPath) {
                for (const ptree::value_type &u : searchPath.get())
                    c.addSearchListEntry(u.second.data());
            }

            optional<ptree&> dns =
                dhcp6.get().get_child_optional(DHCP_DNS_SERVERS);
            if (dns) {
                for (const ptree::value_type &u : dns.get())
                    c.addDnsServer(u.second.data());
            }

            optional<uint32_t> t1 =
                dhcp6.get().get_optional<uint32_t>(DHCP_T1);
            if (t1)
                c.setT1(t1.get());

            optional<uint32_t> t2 =
                dhcp6.get().get_optional<uint32_t>(DHCP_T2);
            if (t2)
                c.setT2(t2.get());

            optional<uint32_t> validLifetime =
                dhcp6.get().get_optional<uint32_t>(DHCP_VALID_LIFETIME);
            if (validLifetime)
                c.setValidLifetime(validLifetime.get());

            optional<uint32_t> preferredLifetime =
                dhcp6.get().get_optional<uint32_t>(DHCP_PREFERRED_LIFETIME);
            if (preferredLifetime)
                c.setPreferredLifetime(preferredLifetime.get());

            newep.setDHCPv6Config(c);
        }

        optional<ptree&> ipms =
            properties.get_child_optional(IP_ADDRESS_MAPPING);
        if (ipms) {
            for (const ptree::value_type &v : ipms.get()) {
                optional<string> fuuid =
                    v.second.get_optional<string>(EP_UUID);
                if (!fuuid) continue;

                Endpoint::IPAddressMapping ipm(fuuid.get());

                optional<string> floatingIp =
                    v.second.get_optional<string>(IPM_FLOATING_IP);
                if (floatingIp)
                    ipm.setFloatingIP(floatingIp.get());

                optional<string> mappedIp =
                    v.second.get_optional<string>(IPM_MAPPED_IP);
                if (mappedIp)
                    ipm.setMappedIP(mappedIp.get());

                optional<string> feg =
                    v.second.get_optional<string>(EP_GROUP);
                if (feg) {
                    ipm.setEgURI(URI(feg.get()));
                } else {
                    optional<string> feg_name =
                        v.second.get_optional<string>(EP_GROUP_NAME);
                    optional<string> fps_name =
                        v.second.get_optional<string>(POLICY_SPACE_NAME);
                    if (feg_name && fps_name) {
                        ipm.setEgURI(opflex::modb::URIBuilder()
                                     .addElement("PolicyUniverse")
                                     .addElement("PolicySpace")
                                     .addElement(fps_name.get())
                                     .addElement("GbpEpGroup")
                                     .addElement(feg_name.get()).build());
                    }
                }

                optional<string> nextHopIf =
                    v.second.get_optional<string>(IPM_NEXTHOP_IF);
                if (nextHopIf)
                    ipm.setNextHopIf(nextHopIf.get());

                optional<string> nextHopMac =
                    v.second.get_optional<string>(IPM_NEXTHOP_MAC);
                if (nextHopMac) {
                    ipm.setNextHopMAC(MAC(nextHopMac.get()));
                }

                if (ipm.getMappedIP())
                    newep.addIPAddressMapping(ipm);
            }
        }

        optional<string> snatIp =
            properties.get_optional<string>(SNAT_IP);
        if (snatIp)
            newep.setSnatIP(snatIp.get());

        ep_map_t::const_iterator it = knownEps.find(pathstr);
        if (it != knownEps.end()) {
            if (newep.getUUID() != it->second)
                deleted(filePath);
        }
        knownEps[pathstr] = newep.getUUID();
        updateEndpoint(newep);

        LOG(INFO) << "Updated endpoint " << newep
                  << " from " << filePath;

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load endpoint from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading endpoint information from "
                   << filePath;
    }
}

void FSEndpointSource::deleted(const fs::path& filePath) {
    try {
        string pathstr = filePath.string();
        ep_map_t::iterator it = knownEps.find(pathstr);
        if (it != knownEps.end()) {
            LOG(INFO) << "Removed endpoint "
                      << it->second
                      << " at " << filePath;
            removeEndpoint(it->second);
            knownEps.erase(it);
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete endpoint for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting endpoint information for "
                   << filePath;
    }
}

} /* namespace opflexagent */
