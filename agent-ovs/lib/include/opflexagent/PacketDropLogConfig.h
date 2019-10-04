/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for packet drop log config
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_PACKETDROPLOGCONFIG_H
#define OPFLEXAGENT_PACKETDROPLOGCONFIG_H

#include <map>
#include <string>
#include <opflex/modb/URI.h>
#include <opflex/modb/MAC.h>
#include <boost/asio/ip/address.hpp>
#include <boost/optional.hpp>
#include <modelgbp/observer/DropLogModeEnumT.hpp>

using boost::optional;

namespace opflexagent {

    class PacketDropLogConfig {
typedef modelgbp::observer::DropLogModeEnumT DropLogMode;
    public:
        PacketDropLogConfig(const opflex::modb::URI &uri):
            dropLogMode(DropLogMode::CONST_UNFILTERED_DROP_LOG), dropLogEnable(false),
            dropLogCfgURI(uri) {};
        ~PacketDropLogConfig() {};
        std::string filePath;
        opflex::modb::URI  dropLogCfgURI;
        uint8_t  dropLogMode;
        bool dropLogEnable;
    };

    class PacketDropFlowSpec {
    public:
        PacketDropFlowSpec() {};
        ~PacketDropFlowSpec() {};
        std::string uuid;
        optional<boost::asio::ip::address> innerSrc, innerDst, outerSrc, outerDst;
        optional<opflex::modb::MAC> innerSrcMac, innerDstMac;
        optional<uint16_t> ethType, sPort,dPort;
        optional<uint8_t> ipProto;
        optional<uint32_t> tunnelId;
    };

    class PacketDropFlowConfig {
    public:
        PacketDropFlowConfig(std::string& _filePath, opflex::modb::URI &uri):
            filePath(_filePath), dropFlowURI(uri) {};
        ~PacketDropFlowConfig() {};
        std::string filePath;
        opflex::modb::URI  dropFlowURI;
        PacketDropFlowSpec spec;
    };
    typedef std::map<std::string, PacketDropFlowSpec> DropFlowMap;

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PACKETDROPLOGCONFIG_H */
