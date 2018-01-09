/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSRDConfigSource class.
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

#include <opflexagent/FSRDConfigSource.h>
#include <opflexagent/ExtraConfigManager.h>
#include <opflexagent/RDConfig.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::make_pair;
using std::runtime_error;
using opflex::modb::URI;

FSRDConfigSource::FSRDConfigSource(ExtraConfigManager* manager_,
                                   FSWatcher& listener,
                                   const std::string& serviceDir)
    : manager(manager_) {
    listener.addWatch(serviceDir, *this);
}

static bool isrdconfig(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".rdconfig") &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSRDConfigSource::updated(const fs::path& filePath) {
    if (!isrdconfig(filePath)) return;

    static const std::string RD_UUID("uuid");
    static const std::string RD_DOMAIN("domain");
    static const std::string RD_DOMAIN_POLICY_SPACE("domain-policy-space");
    static const std::string RD_DOMAIN_NAME("domain-name");
    static const std::string RD_INTERNAL_SUBNETS("internal-subnets");

    try {
        using boost::property_tree::ptree;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);

        optional<URI> domainURI;
        optional<string> domain =
            properties.get_optional<string>(RD_DOMAIN);
        if (domain) {
            domainURI = URI(domain.get());
        } else {
            optional<string> domainName =
                properties.get_optional<string>(RD_DOMAIN_NAME);
            optional<string> domainPSpace =
                properties.get_optional<string>(RD_DOMAIN_POLICY_SPACE);
            if (domainName && domainPSpace) {
                domainURI = (opflex::modb::URIBuilder()
                             .addElement("PolicyUniverse")
                             .addElement("PolicySpace")
                             .addElement(domainPSpace.get())
                             .addElement("GbpRoutingDomain")
                             .addElement(domainName.get()).build());
            }
        }
        if (!domainURI) {
            LOG(ERROR) << "Domain URI not set for routing domain from: "
                       << filePath;
            return;
        }
        RDConfig newrd(domainURI.get());

        optional<ptree&> sns =
            properties.get_child_optional(RD_INTERNAL_SUBNETS);
        if (sns) {
            for (const ptree::value_type &v : sns.get()) {
                newrd.addInternalSubnet(v.second.data());
            }
        }

        rdc_map_t::const_iterator it = knownDomainConfigs.find(pathstr);
        if (it != knownDomainConfigs.end()) {
            if (domainURI.get() != it->second)
                deleted(filePath);
        }
        knownDomainConfigs.insert(make_pair(pathstr, domainURI.get()));
        manager->updateRDConfig(newrd);

        LOG(INFO) << "Updated routing domain config " << newrd
                  << " from " << filePath;

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load routing domain config from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading routing domain config "
                   << "information from "
                   << filePath;
    }
}

void FSRDConfigSource::deleted(const fs::path& filePath) {
    try {
        string pathstr = filePath.string();
        rdc_map_t::iterator it = knownDomainConfigs.find(pathstr);
        if (it != knownDomainConfigs.end()) {
            LOG(INFO) << "Removed routing domain config "
                      << it->second
                      << " at " << filePath;
            manager->removeRDConfig(it->second);
            knownDomainConfigs.erase(it);
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete routing domain config for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting routing domain config for "
                   << filePath;
    }
}

} /* namespace opflexagent */
