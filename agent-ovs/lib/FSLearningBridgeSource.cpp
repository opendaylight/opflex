/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSLearningBridgeSource class.
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
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

#include <opflexagent/FSLearningBridgeSource.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::runtime_error;
using std::make_pair;

FSLearningBridgeSource::
FSLearningBridgeSource(LearningBridgeManager* manager_,
                       FSWatcher& listener,
                       const std::string& learningBridgeDir)
    : LearningBridgeSource(manager_) {
    LOG(INFO) << "Watching " << learningBridgeDir
              << " for learning bridge data";
    listener.addWatch(learningBridgeDir, *this);
}

static bool islearningBridge(fs::path filePath) {
    string fstr = filePath.filename().string();
    return boost::algorithm::ends_with(fstr, ".lbiface") &&
        !boost::algorithm::starts_with(fstr, ".");
}

void FSLearningBridgeSource::updated(const fs::path& filePath) {
    if (!islearningBridge(filePath)) return;

    static const std::string UUID("uuid");
    static const std::string IFACE_NAME("interface-name");
    static const std::string TRUNK_VLANS("trunk-vlans");
    static const std::string VLAN_RANGE_START("start");
    static const std::string VLAN_RANGE_END("end");

    try {
        using boost::property_tree::ptree;
        LearningBridgeIface newif;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);
        newif.setUUID(properties.get<string>(UUID));

        optional<string> iface =
            properties.get_optional<string>(IFACE_NAME);
        if (iface)
            newif.setInterfaceName(iface.get());

        optional<ptree&> trunkVlans =
            properties.get_child_optional(TRUNK_VLANS);
        if (trunkVlans) {
            for (const ptree::value_type &v : trunkVlans.get()) {
                 optional<uint16_t> start =
                     v.second.get_optional<uint16_t>(VLAN_RANGE_START);
                 optional<uint16_t> end =
                     v.second.get_optional<uint16_t>(VLAN_RANGE_END);
                 if (!start) start = end;
                 if (!end) end = start;
                 if (start) {
                     newif.addTrunkVlans(make_pair(start.get(), end.get()));
                 }
            }
        }

        knownIfs[pathstr] = newif.getUUID();
        updateLBIface(newif);

        LOG(INFO) << "Updated learning bridge interface " << newif
                  << " from " << filePath;

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load learning bridge interface from:"
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading learning bridge "
                   << "interface information from "
                   << filePath;
    }
}

void FSLearningBridgeSource::deleted(const fs::path& filePath) {
    try {
        string pathstr = filePath.string();
        auto it = knownIfs.find(pathstr);
        if (it != knownIfs.end()) {
            LOG(INFO) << "Removed learning bridge interface "
                      << it->second
                      << " at " << filePath;
            removeLBIface(it->second);
            knownIfs.erase(it);
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete learning bridge for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting learning "
                   << "bridge interface information for "
                   << filePath;
    }
}

} /* namespace opflexagent */
