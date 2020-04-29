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
    static const std::string SERVICE_TYPE("service-type");
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
    static const std::string SM_NODE_PORT("node-port");
    static const std::string SM_CONNTRACK("conntrack-enabled");
    static const std::string SVC_ATTRIBUTES("attributes");
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

        std::string serviceTypeStr =
            properties.get<std::string>(SERVICE_TYPE,
                                        "clusterIp");
        if (serviceTypeStr == "clusterIp") {
            newserv.setServiceType(Service::CLUSTER_IP);
        } else if (serviceTypeStr == "nodePort") {
            newserv.setServiceType(Service::NODE_PORT);
        } else if (serviceTypeStr == "loadBalancer") {
            newserv.setServiceType(Service::LOAD_BALANCER);
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

        optional<ptree&> attrs =
            properties.get_child_optional(SVC_ATTRIBUTES);
        if (attrs) {
            for (const ptree::value_type &v : attrs.get()) {
                newserv.addAttribute(v.first, v.second.data());
            }
        } else {
            // In pod<--> svc stats MOs, we want to mention name of the service.
            // For k8s, "name" will be part of attr map. For OpenStack, there
            // wont be any attr map, in which case we instead use svc intf name.
            // To keep the code generic within agent to process attr for k8s vs open
            // stack, if attr map isnt there, create one with "name" = intf name
            if (ifaceName)
                newserv.addAttribute("name", ifaceName.get());
        }

        // When a LB service is configured in k8s, external ip, cluster ip and
        // nodeport gets allocated. This will also lead to creation of 2 service
        // files - 1 external and 1 non-external (nodeport+cluster). Both will
        // have nodeport set by host-agent.
        // A nodePort service will have a cluster IP allocated as well, so we
        // need to support E-W stats for nodePort services as well.
        // A cluster service will have just cluster IP allocated; nodeport wont be set.
        // Since both cluster and nodeport stats flows are created for the same
        // service file, both the stats are housed under svc-target and svc counter mos.
        // Since the non-external service file service-pods (next hops) are a super-set
        // of the external service file's service-pods (only local to the node), node port
        // flows get created only for "non-external" services. Also it will cover pure
        // node-port services in k8s.
        // Note:
        // - The counter mos will have only 2 types of scope: cluster and ext (based on LB type).
        // - However, the service metrics in prometheus are annotated with "name", "namespace"
        // and "scope", with scope being "cluster" or "nodePort" or "ext" to keep the metrics
        // unique.
        // - For external LB service, the internal and external service file created
        // by host-agent both have type=loadBalancer. This will lead to duplicate metrics in
        // prometheus. If the file doesnt end with "-external", then keep the type as clusterIp
        // which is the actual purpose of that service file.
        if (newserv.getServiceType() == Service::LOAD_BALANCER) {
            const auto& uuid = newserv.getUUID();
            if ((uuid.size() > sizeof("-external"))
                    && boost::algorithm::ends_with(uuid, "-external")) {
                newserv.addAttribute("scope", "ext");
            } else {
                newserv.setServiceType(Service::CLUSTER_IP);
                newserv.addAttribute("scope", "cluster");
            }
        } else {
            newserv.addAttribute("scope", "cluster");
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

                optional<uint16_t> nodePort =
                    v.second.get_optional<uint16_t>(SM_NODE_PORT);
                if (nodePort)
                    sm.setNodePort(nodePort.get());

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
