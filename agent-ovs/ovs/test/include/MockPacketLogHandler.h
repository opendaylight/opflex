/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for MockPacketLogHandler
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_MOCKPACKETLOGHANDLER_H_
#define OPFLEXAGENT_MOCKPACKETLOGHANDLER_H_

#include "PacketLogHandler.h"

namespace opflexagent {

/**
 * Mock class to test the packet decoder
 */
class MockPacketLogHandler: public PacketLogHandler {
public:
    /*
     * Constructor for MockPacketLogHandler.
     * Io_service arguments are not used in tests
     */
    MockPacketLogHandler(boost::asio::io_service &io_1,
            boost::asio::io_service &io_2): PacketLogHandler(io_1, io_2) {
    }
    /**
     * Start packet logging.
     * For brevity not all tables are added.
     */
    virtual bool startListener() {
        TableDescriptionMap intTableDesc,accTableDesc;
#define TABLE_DESC(descMap, table_id, table_name, drop_reason) \
        descMap.insert( \
                    std::make_pair(table_id, \
                            std::make_pair(table_name, drop_reason)));
        TABLE_DESC(intTableDesc, 1, "PORT_SECURITY_TABLE",
            "Port security policy missing/incorrect");
        TABLE_DESC(intTableDesc, 2, "SOURCE_TABLE",
            "Source policy group derivation missing/incorrect");
        TABLE_DESC(intTableDesc, 4, "SERVICE_REV_TABLE",
                "Service source policy missing/incorrect");
        TABLE_DESC(intTableDesc, 5, "BRIDGE_TABLE",
                "MAC lookup failed");
        TABLE_DESC(accTableDesc, 1, "GROUP_MAP_TABLE",
                "Access port incorrect");
        TABLE_DESC(accTableDesc, 3, "SEC_GROUP_OUT_TABLE",
                "Ingress security group missing/incorrect");
#undef TABLE_DESC
        setIntBridgeTableDescription(intTableDesc);
        setAccBridgeTableDescription(accTableDesc);
        pktDecoder.configure();
        return true;
    }
    /**
     * Get the underlying packet decoder
     * @return PacketDecoder
     */
     PacketDecoder &getDecoder() {
         return pktDecoder;
     }
};

}
#endif
