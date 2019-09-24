/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSSnatSource class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
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

#include <opflexagent/FSSnatSource.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::runtime_error;

FSSnatSource::FSSnatSource(SnatManager* manager_,
                           FSWatcher& listener,
                           const std::string& snatDir)
    : SnatSource(manager_) {
    LOG(INFO) << "Watching " << snatDir << " for snat data";
    listener.addWatch(snatDir, *this);
}

static bool issnat(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".snat") &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSSnatSource::updated(const fs::path& filePath) {
    if (!issnat(filePath)) return;

    static const std::string UUID("uuid");
    static const std::string SNAT_IP("snat-ip");
    // default is remote unless "local": true
    static const std::string LOCAL("local");
    static const std::string INTERFACE_NAME("interface-name");
    static const std::string INTERFACE_MAC("interface-mac");
    static const std::string INTERFACE_VLAN("interface-vlan");
    static const std::string DEST("dest");
    static const std::string DEST_PREFIX("dest-prefix");
    static const std::string ZONE("zone");
    static const std::string PORT_RANGE("port-range");
    static const std::string PORT_RANGE_START("start");
    static const std::string PORT_RANGE_END("end");
    static const std::string REMOTE("remote");
    static const std::string MAC("mac");

    try {
        using boost::property_tree::ptree;
        Snat newsnat;
        ptree properties;
        bool valid_range = false;

        string pathstr = filePath.string();

        read_json(pathstr, properties);

        // Common to local and remote
        newsnat.setUUID(properties.get<string>(UUID));
        newsnat.setSnatIP(properties.get<string>(SNAT_IP));
        newsnat.setInterfaceName(properties.get<string>(INTERFACE_NAME));
        optional<uint16_t> ifaceVlan =
            properties.get_optional<uint16_t>(INTERFACE_VLAN);
        if (ifaceVlan)
            newsnat.setIfaceVlan(ifaceVlan.get());

        // Local configuration
        optional<bool> local =
            properties.get_optional<bool>(LOCAL);
        if (local)
            newsnat.setLocal(local.get());
        optional<string> ifaceMac =
             properties.get_optional<string>(INTERFACE_MAC);
        if (ifaceMac)
            newsnat.setInterfaceMAC(opflex::modb::MAC(ifaceMac.get()));
        optional<string> nw_dst =
            properties.get_optional<string>(DEST);
        if (nw_dst)
            newsnat.setDest(nw_dst.get());
        optional<uint8_t> prefix =
            properties.get_optional<uint8_t>(DEST_PREFIX);
        if (prefix)
            newsnat.setDestPrefix(prefix.get());
        optional<uint16_t> zone =
            properties.get_optional<uint16_t>(ZONE);
        if (zone)
            newsnat.setZone(zone.get());

        optional<ptree&> prs =
            properties.get_child_optional(PORT_RANGE);
        if (prs) {
            for (const ptree::value_type &v : prs.get()) {
                optional<uint16_t> a =
                    v.second.get_optional<uint16_t>
                    (PORT_RANGE_START);
                optional<uint16_t> b =
                    v.second.get_optional<uint16_t>
                    (PORT_RANGE_END);
                if (a && b && b.get() > a.get()) {
                    newsnat.addPortRange("local", a.get(), b.get());
                    valid_range = true;
                }
            }
        }

        // Remote configuration
        optional<ptree&> remote =
            properties.get_child_optional(REMOTE);
        if (remote) {
            for (const ptree::value_type &r : remote.get()) {
                optional<string> remoteMac =
                    r.second.get_optional<string>(MAC);
                if (!remoteMac)
                    continue;
                optional<const ptree&> prs =
                    r.second.get_child_optional(PORT_RANGE);
                if (prs) {
                    for (const ptree::value_type &v : prs.get()) {
                        optional<uint16_t> a =
                            v.second.get_optional<uint16_t>
                            (PORT_RANGE_START);
                        optional<uint16_t> b =
                            v.second.get_optional<uint16_t>
                            (PORT_RANGE_END);
                        if (a && b && b.get() > a.get()) {
                            newsnat.addPortRange(remoteMac.get(),
                                                 a.get(), b.get());
                            valid_range = true;
                        }
                    }
                }
            }
        }

        snat_map_t::const_iterator it = knownSnats.find(pathstr);
        if (it != knownSnats.end()) {
            if (newsnat.getSnatIP() != it->second.first)
                deleted(filePath);
        }
        if (valid_range) {
            knownSnats[pathstr] = make_pair(newsnat.getSnatIP(),
                                            newsnat.getUUID());
            updateSnat(newsnat);
            LOG(INFO) << "Updated Snat " << newsnat
                      << " from " << filePath;
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load snat from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading snat "
                   << "information from "
                   << filePath;
   }
}

void FSSnatSource::deleted(const fs::path& filePath) {
    try {
        string pathstr = filePath.string();
        snat_map_t::iterator it = knownSnats.find(pathstr);
        if (it != knownSnats.end()) {
            LOG(INFO) << "Removed snat-ip "
                      << it->second.first
                      << "uuid " << it->second.second
                      << " at " << filePath;
            removeSnat(it->second.first, it->second.second);
            knownSnats.erase(it);
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete snat for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting snat information for "
                   << filePath;
    }
}

} /* namespace opflexagent */
