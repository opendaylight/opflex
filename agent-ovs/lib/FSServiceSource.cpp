/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSServiceSource class.
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

#include <opflexagent/FSServiceSource.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::runtime_error;
using opflex::modb::URI;
using opflex::modb::MAC;

FSServiceSource::FSServiceSource(ServiceManager* manager_,
                                   FSWatcher& listener,
                                   const std::string& serviceDir)
    : ServiceSource(manager_) {
    LOG(INFO) << "Watching " << serviceDir << " for service data";
    listener.addWatch(serviceDir, *this);
}

static bool isservice(fs::path filePath) {
    string fstr = filePath.filename().string();
    return ((boost::algorithm::ends_with(fstr, ".as") ||
             boost::algorithm::ends_with(fstr, ".service")) &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSServiceSource::updated(const fs::path& filePath) {
    if (!isservice(filePath)) return;

    static const std::string UUID("uuid");
    static const std::string SERVICE_MAC("service-mac");
    static const std::string INTERFACE_NAME("interface-name");
    static const std::string INTERFACE_VLAN("interface-vlan");
    static const std::string INTERFACE_IP("interface-ip");
    static const std::string SERVICE_MODE("service-mode");
    static const std::string SERVICE_DOMAIN("domain");
    static const std::string DOMAIN_POLICY_SPACE("domain-policy-space");
    static const std::string DOMAIN_NAME("domain-name");

    static const std::string SERVICE_MAPPING("service-mapping");
    static const std::string SM_SERVICE_IP("service-ip");
    static const std::string SM_SERVICE_PROTO("service-proto");
    static const std::string SM_SERVICE_PORT("service-port");
    static const std::string SM_GATEWAY_IP("gateway-ip");
    static const std::string SM_NEXT_HOP_IP("next-hop-ip");
    static const std::string SM_NEXT_HOP_IPS("next-hop-ips");
    static const std::string SM_NEXT_HOP_PORT("next-hop-port");
    static const std::string SM_CONNTRACK("conntrack-enabled");

    try {
        using boost::property_tree::ptree;
        Service newserv;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);
        newserv.setUUID(properties.get<string>(UUID));

        std::string serviceModeStr =
            properties.get<std::string>(SERVICE_MODE,
                                        "local-anycast");
        if (serviceModeStr == "loadbalancer") {
            newserv.setServiceMode(Service::LOADBALANCER);
        } else {
            newserv.setServiceMode(Service::LOCAL_ANYCAST);
        }

        optional<string> serviceMac =
            properties.get_optional<string>(SERVICE_MAC);
        if (serviceMac) {
            newserv.setServiceMAC(MAC(serviceMac.get()));
        }

        optional<string> ifaceName =
            properties.get_optional<string>(INTERFACE_NAME);
        if (ifaceName)
            newserv.setInterfaceName(ifaceName.get());

        optional<uint16_t> ifaceVlan =
            properties.get_optional<uint16_t>(INTERFACE_VLAN);
        if (ifaceVlan)
            newserv.setIfaceVlan(ifaceVlan.get());

        optional<string> ifaceIp =
            properties.get_optional<string>(INTERFACE_IP);
        if (ifaceIp) {
            newserv.setIfaceIP(ifaceIp.get());
        }

        optional<string> domain =
            properties.get_optional<string>(SERVICE_DOMAIN);
        if (domain) {
            newserv.setDomainURI(URI(domain.get()));
        } else {
            optional<string> domainName =
                properties.get_optional<string>(DOMAIN_NAME);
            optional<string> domainPSpace =
                properties.get_optional<string>(DOMAIN_POLICY_SPACE);
            if (domainName && domainPSpace) {
                newserv.setDomainURI(opflex::modb::URIBuilder()
                                     .addElement("PolicyUniverse")
                                     .addElement("PolicySpace")
                                     .addElement(domainPSpace.get())
                                     .addElement("GbpRoutingDomain")
                                     .addElement(domainName.get()).build());
            }
        }

        optional<ptree&> sms =
            properties.get_child_optional(SERVICE_MAPPING);
        if (sms) {
            for (const ptree::value_type &v : sms.get()) {
                Service::ServiceMapping sm;

                optional<string> serviceIp =
                    v.second.get_optional<string>(SM_SERVICE_IP);
                if (serviceIp)
                    sm.setServiceIP(serviceIp.get());

                optional<string> serviceProto =
                    v.second.get_optional<string>(SM_SERVICE_PROTO);
                if (serviceProto)
                    sm.setServiceProto(serviceProto.get());

                optional<uint16_t> servicePort =
                    v.second.get_optional<uint16_t>(SM_SERVICE_PORT);
                if (servicePort)
                    sm.setServicePort(servicePort.get());

                optional<string> gatewayIp =
                    v.second.get_optional<string>(SM_GATEWAY_IP);
                if (gatewayIp)
                    sm.setGatewayIP(gatewayIp.get());

                optional<string> nextHopIp =
                    v.second.get_optional<string>(SM_NEXT_HOP_IP);
                if (nextHopIp)
                    sm.addNextHopIP(nextHopIp.get());

                optional<const ptree&> nhips =
                    v.second.get_child_optional(SM_NEXT_HOP_IPS);
                if (nhips) {
                    for (auto& nhip : nhips.get()) {
                        sm.addNextHopIP(nhip.second.data());
                    }
                }

                optional<uint16_t> nextHopPort =
                    v.second.get_optional<uint16_t>(SM_NEXT_HOP_PORT);
                if (nextHopPort)
                    sm.setNextHopPort(nextHopPort.get());

                optional<bool> conntrack =
                    v.second.get_optional<bool>(SM_CONNTRACK);
                if (conntrack)
                    sm.setConntrackMode(conntrack.get());

                newserv.addServiceMapping(sm);
            }
        }

        serv_map_t::const_iterator it = knownServs.find(pathstr);
        if (it != knownServs.end()) {
            if (newserv.getUUID() != it->second)
                deleted(filePath);
        }
        knownServs[pathstr] = newserv.getUUID();
        updateService(newserv);

        LOG(INFO) << "Updated service " << newserv
                  << " from " << filePath;

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load service from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading service "
                   << "information from "
                   << filePath;
    }
}

void FSServiceSource::deleted(const fs::path& filePath) {
    try {
        string pathstr = filePath.string();
        serv_map_t::iterator it = knownServs.find(pathstr);
        if (it != knownServs.end()) {
            LOG(INFO) << "Removed service "
                      << it->second
                      << " at " << filePath;
            removeService(it->second);
            knownServs.erase(it);
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete service for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting service information for "
                   << filePath;
    }
}

} /* namespace opflexagent */
