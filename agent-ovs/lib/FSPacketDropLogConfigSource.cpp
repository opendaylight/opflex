/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSPacketDropLogConfigSource class.
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

#include <opflexagent/FSPacketDropLogConfigSource.h>
#include <modelgbp/observer/DropLogModeEnumT.hpp>
#include <opflexagent/ExtraConfigManager.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::make_pair;
using std::runtime_error;
using opflex::modb::URI;
using boost::asio::ip::address;
using modelgbp::observer::DropLogModeEnumT;

FSPacketDropLogConfigSource::FSPacketDropLogConfigSource(ExtraConfigManager* manager_,
                                   FSWatcher& listener,
                                   const std::string& serviceDir,
                                   const opflex::modb::URI &uri)
    : manager(manager_),dropCfg(uri) {
    listener.addWatch(serviceDir, *this);
}

static bool isPacketDropLogConfig(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".droplogcfg") &&
            !boost::algorithm::starts_with(fstr, "."));
}

static bool isPacketDropFlowConfig(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".dropflowcfg") &&
            !boost::algorithm::starts_with(fstr, "."));
}

static inline bool validateIpAddress(const boost::optional<string> ipAddress,
        optional<boost::asio::ip::address> &_targetAddress,bool require_v4, string inputField) {
    address targetAddress;
    boost::system::error_code ec;
    if(!ipAddress) {
        return true;
    }
    targetAddress = address::from_string(ipAddress.get(), ec);
    if(ec) {
        LOG(ERROR) << "Invalid " << inputField
                << ipAddress.get() << ": " << ec;
        return false;
    }
    if(require_v4  && !targetAddress.is_v4()) {
        LOG(ERROR) << inputField << targetAddress << " should be a IPv4 address";
        return false;
    }
    _targetAddress = targetAddress;
    return true;
}

static inline bool validateMacAddress(const boost::optional<string> macAddress,
        optional<opflex::modb::MAC> &_targetMacAddress, string inputField) {
    opflex::modb::MAC targetMacAddress;
    if(!macAddress) {
        return true;
    }
    try {
        targetMacAddress = opflex::modb::MAC(macAddress.get());
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not parse MAC address for: "
                   << inputField << ": "
                   << ex.what();
        return false;
    } catch (...) {
        LOG(ERROR) << "Unknown error while parsing MAC address for"
                   << inputField;
        return false;
    }
    _targetMacAddress = targetMacAddress;
    return true;
}

void FSPacketDropLogConfigSource::updated(const fs::path& filePath) {
    using boost::property_tree::ptree;
    ptree properties;

    try {
        if (isPacketDropLogConfig(filePath)) {
            if(!dropCfg.filePath.empty() &&
                    (filePath.string() != dropCfg.filePath)) {
                return;
            }
            string pathStr = filePath.string();
            std::string dropLogMode;
            read_json(pathStr, properties);
            static const std::string DROP_LOG_ENABLE("drop-log-enable");
            static const std::string DROP_LOG_MODE("drop-log-mode");
            dropCfg.dropLogEnable =
                properties.get<bool>(DROP_LOG_ENABLE, false);
            dropLogMode =
                properties.get<string>(DROP_LOG_MODE, "unfiltered");
            if(dropLogMode == "unfiltered"){
                dropCfg.dropLogMode = DropLogModeEnumT::CONST_UNFILTERED_DROP_LOG;
            } else if(dropLogMode == "flow-based") {
                dropCfg.dropLogMode = DropLogModeEnumT::CONST_FLOW_BASED_DROP_LOG;
            } else {
                LOG(ERROR) << "Unknown drop-log-mode: " << dropLogMode;
                return;
            }
            dropCfg.filePath = pathStr;
            manager->packetDropLogConfigUpdated(dropCfg);

            LOG(INFO) << "Updated packet drop log config "
                      << " from " << filePath;
        } else if (isPacketDropFlowConfig(filePath)) {
            using boost::asio::ip::address;
            boost::system::error_code ec;
            string pathStr = filePath.string();
            read_json(pathStr, properties);
            LOG(INFO) << "TBD: Updated packet drop flow config "
                                  << " from " << filePath;

            bool validated = true;
            static const std::string UUID("uuid");
            static const std::string OUTER_SRC("outer-src-ip-address");
            static const std::string OUTER_DST("outer-dst-ip-address");
            static const std::string INNER_SRC("inner-src-ip-address");
            static const std::string INNER_DST("inner-dst-ip-address");
            static const std::string INNER_SRC_MAC("inner-src-mac");
            static const std::string INNER_DST_MAC("inner-dst-mac");
            static const std::string INNER_ETH_TYPE("inner-eth-type");
            static const std::string INNER_IP_PROTO("inner-ip-proto");
            static const std::string INNER_SRC_PORT("inner-src-port");
            static const std::string INNER_DST_PORT("inner-dst-port");
            static const std::string TUNNEL_ID("tunnel-id");
            optional<string> uuid = properties.get_optional<string>(UUID);
            validated &= (uuid)? true:false;
            if(!validated) {
                LOG(ERROR) << "UUID is required ";
                return;
            }
            opflex::modb::URI uri = opflex::modb::URIBuilder()
                    .addElement("ObserverDropFlowConfigUniverse")
                    .addElement("ObserverDropFlowConfig")
                    .addElement(uuid.get()).build();
            PacketDropFlowConfig flowCfg(pathStr,uri);
            flowCfg.spec.uuid = uuid.get();
            validated &= validateIpAddress(properties.get_optional<string>(OUTER_SRC),
                    flowCfg.spec.outerSrc, true, OUTER_SRC);
            validated &= validateIpAddress(properties.get_optional<string>(OUTER_DST),
                    flowCfg.spec.outerDst, true, OUTER_DST);
            validated &=  validateIpAddress(properties.get_optional<string>(INNER_SRC),
                    flowCfg.spec.innerSrc, false, INNER_SRC);
            validated &=  validateIpAddress(properties.get_optional<string>(INNER_DST),
                    flowCfg.spec.innerDst, false, INNER_DST);
            validated &=  validateMacAddress(properties.get_optional<string>(INNER_SRC_MAC),
                    flowCfg.spec.innerSrcMac, INNER_SRC_MAC);
            validated &=  validateMacAddress(properties.get_optional<string>(INNER_DST_MAC),
                    flowCfg.spec.innerDstMac, INNER_DST_MAC);
            flowCfg.spec.ethType = properties.get_optional<uint16_t>(INNER_ETH_TYPE);
            flowCfg.spec.ipProto = properties.get_optional<uint8_t>(INNER_IP_PROTO);
            flowCfg.spec.sPort = properties.get_optional<uint16_t>(INNER_SRC_PORT);
            flowCfg.spec.dPort = properties.get_optional<uint16_t>(INNER_DST_PORT);
            flowCfg.spec.tunnelId = properties.get_optional<uint16_t>(TUNNEL_ID);
            if(!validated){
                return;
            }
            auto it = dropFlowMap.find(pathStr);
            if( it != dropFlowMap.end()) {
                it->second = flowCfg.spec;
            } else {
                dropFlowMap.insert(make_pair(pathStr,flowCfg.spec));
            }
            manager->packetDropFlowConfigUpdated(flowCfg);
        }

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load packet drop log/flow config from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading packet drop log/flow config "
                   << "information from "
                   << filePath;
    }

}

void FSPacketDropLogConfigSource::deleted(const fs::path& filePath) {
    try {
        string pathStr = filePath.string();
        if (isPacketDropLogConfig(filePath)) {
            if(dropCfg.filePath != pathStr){
                return;
            }
            dropCfg.filePath.clear();
            dropCfg.dropLogEnable = false;
            dropCfg.dropLogMode = DropLogModeEnumT::CONST_UNFILTERED_DROP_LOG;
            manager->packetDropLogConfigUpdated(dropCfg);
            LOG(INFO) << "Removed packet drop log config "
                      << " at " << filePath;
        } else if(isPacketDropFlowConfig(filePath)) {

            DropFlowMap::iterator it = dropFlowMap.find(pathStr);
            if(it != dropFlowMap.end()) {
                opflex::modb::URI uri = opflex::modb::URIBuilder()
                        .addElement("ObserverDropFlowConfigUniverse")
                        .addElement("ObserverDropFlowConfig")
                        .addElement(it->second.uuid).build();
                PacketDropFlowConfig flowCfg(pathStr, uri);
                flowCfg.filePath.clear();
                manager->packetDropFlowConfigUpdated(flowCfg);
                LOG(INFO) << "Removed packet drop flow config "
                          << " at " << filePath;
                dropFlowMap.erase(it);
            }
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete packet drop log/flow config for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting packet drop log/flow config for "
                   << filePath;
    }
}

} /* namespace opflexagent */
