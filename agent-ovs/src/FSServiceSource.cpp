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

#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <opflex/modb/URIBuilder.h>

#include "FSServiceSource.h"
#include "logging.h"

namespace ovsagent {

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

static bool isanycastservice(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".as") &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSServiceSource::updated(const fs::path& filePath) {
    if (!isanycastservice(filePath)) return;

    static const std::string AS_UUID("uuid");
    static const std::string AS_SERVICE_MAC("service-mac");
    static const std::string AS_INTERFACE_NAME("interface-name");
    static const std::string AS_DOMAIN("domain");
    static const std::string AS_DOMAIN_POLICY_SPACE("domain-policy-space");
    static const std::string AS_DOMAIN_NAME("domain-name");

    static const std::string SERVICE_MAPPING("service-mapping");
    static const std::string SM_SERVICE_IP("service-ip");
    static const std::string SM_GATEWAY_IP("gateway-ip");
    static const std::string SM_NEXT_HOP_IP("next-hop-ip");

    try {
        using boost::property_tree::ptree;
        AnycastService newserv;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);
        newserv.setUUID(properties.get<string>(AS_UUID));

        optional<string> serviceMac =
            properties.get_optional<string>(AS_SERVICE_MAC);
        if (serviceMac) {
            newserv.setServiceMAC(MAC(serviceMac.get()));
        }

        optional<string> ifaceName =
            properties.get_optional<string>(AS_INTERFACE_NAME);
        if (ifaceName)
            newserv.setInterfaceName(ifaceName.get());

        optional<string> domain =
            properties.get_optional<string>(AS_DOMAIN);
        if (domain) {
            newserv.setDomainURI(URI(domain.get()));
        } else {
            optional<string> domainName =
                properties.get_optional<string>(AS_DOMAIN_NAME);
            optional<string> domainPSpace =
                properties.get_optional<string>(AS_DOMAIN_POLICY_SPACE);
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
            BOOST_FOREACH(const ptree::value_type &v, sms.get()) {
                AnycastService::ServiceMapping sm;

                optional<string> serviceIp =
                    v.second.get_optional<string>(SM_SERVICE_IP);
                if (serviceIp)
                    sm.setServiceIP(serviceIp.get());

                optional<string> gatewayIp =
                    v.second.get_optional<string>(SM_GATEWAY_IP);
                if (gatewayIp)
                    sm.setGatewayIP(gatewayIp.get());

                optional<string> nextHopIp =
                    v.second.get_optional<string>(SM_NEXT_HOP_IP);
                if (nextHopIp)
                    sm.setNextHopIP(nextHopIp.get());

                newserv.addServiceMapping(sm);
            }
        }

        serv_map_t::const_iterator it = knownAnycastServs.find(pathstr);
        if (it != knownAnycastServs.end()) {
            if (newserv.getUUID() != it->second)
                deleted(filePath);
        }
        knownAnycastServs[pathstr] = newserv.getUUID();
        updateAnycastService(newserv);

        LOG(INFO) << "Updated anycast service " << newserv
                  << " from " << filePath;

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load anycast service from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading anycast service "
                   << "information from "
                   << filePath;
    }
}

void FSServiceSource::deleted(const fs::path& filePath) {
    try {
        string pathstr = filePath.string();
        serv_map_t::iterator it = knownAnycastServs.find(pathstr);
        if (it != knownAnycastServs.end()) {
            LOG(INFO) << "Removed anycast service "
                      << it->second
                      << " at " << filePath;
            removeAnycastService(it->second);
            knownAnycastServs.erase(it);
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

} /* namespace ovsagent */
