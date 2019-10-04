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
    /**
     * Collect Packet Drop Log Configuration
     */
    class PacketDropLogConfig {
typedef modelgbp::observer::DropLogModeEnumT DropLogMode;
    public:
       /**
        * Instantiate a new packet drop log config
        * @param uri Fixed URI of the global packet drop log config
        */
        PacketDropLogConfig(const opflex::modb::URI &uri):
            dropLogCfgURI(uri),
            dropLogMode(DropLogMode::CONST_UNFILTERED_DROP_LOG),
            dropLogEnable(false) {};
        /**
         * Path of the file from where this config was read
         */
        std::string filePath;
        /**
         * Fixed URI of the global packet drop log config
         */
        opflex::modb::URI  dropLogCfgURI;
        /**
         * Drop log mode whether global or flow-specific
         */
        uint8_t  dropLogMode;
        /**
         * Whether Drop logging is enabled/disabled
         */
        bool dropLogEnable;
    };
    /**
     * Collect Packet Drop Flow Specification
     */
    class PacketDropFlowSpec {
    public:
        /**
         * uuid of the flow spec
         */
        std::string uuid;
        ///@{
        /* IP addresses inner/outer of the flow */
        optional<boost::asio::ip::address> innerSrc, innerDst, outerSrc, outerDst;
        ///@}
        ///@{
        /* Inner source,destination mac for the flow */
        optional<opflex::modb::MAC> innerSrcMac, innerDstMac;
        ///@}
        ///@{
        /* Inner eth type, source and destination port */
        optional<uint16_t> ethType, sPort,dPort;
        ///@}
        /**
         * Inner ip proto
         */
        optional<uint8_t> ipProto;
        /**
         * Tunnel Id(vnid) for the VxLAN encapsulated packet
         */
        optional<uint32_t> tunnelId;
    };
    /**
     * Collect Packet Drop Flow Configuration
     */
    class PacketDropFlowConfig {
    public:
        /**
        * Instantiate a new packet drop flow configuration
        * @param _filePath File path from where flow spec was read
        * @param uri URI of this flow specification
        */
        PacketDropFlowConfig(std::string& _filePath, opflex::modb::URI &uri):
            filePath(_filePath), dropFlowURI(uri) {};
        /**
         * Path of the file from where this config was read
         */
        std::string filePath;
        /**
         * URI of the flow spec
         */
        opflex::modb::URI  dropFlowURI;
        /**
         * URI of the flow spec
         */
        PacketDropFlowSpec spec;
    };
    /**
     * Map of file path and flow spec
     */
    typedef std::map<std::string, PacketDropFlowSpec> DropFlowMap;

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PACKETDROPLOGCONFIG_H */
