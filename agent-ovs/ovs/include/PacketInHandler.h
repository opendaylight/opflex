/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for PacketInHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_PACKETINHANDLER_H
#define OPFLEXAGENT_PACKETINHANDLER_H

#include <boost/noncopyable.hpp>

#include "SwitchConnection.h"
#include "PortMapper.h"
#include "FlowReader.h"
#include "TableState.h"
#include <opflexagent/Agent.h>

struct dp_packet;
struct flow;
struct ofputil_packet_in;

namespace opflexagent {

class IntFlowManager;

/**
 * Handler for packet-in messages arriving from the switch
 */
class PacketInHandler : public MessageHandler,
                        private boost::noncopyable {
public:
    /**
     * Construct a PacketInHandler
     */
    PacketInHandler(Agent& agent, IntFlowManager& intFlowManager);

    /**
     * Set the port mapper to use
     * @param intMapper the integration bridge port mapper
     * @param accessMapper the access bridge port mapper
     */
    void setPortMapper(PortMapper* intMapper,
                       PortMapper* accessMapper);

    /**
     * Set the integration bridge flow reader to use
     * @param r the flow reader
     */
    void setFlowReader(FlowReader* r) { intFlowReader = r; }

    /**
     * Set the switch connections to use
     * @param intConnection the integration bridge switch connection
     * @param accessConnection the access bridge switch connection
     */
    void registerConnection(SwitchConnection* intConnection,
                            SwitchConnection* accessConnection);

    /**
     * Start the packet in handler
     */
    void start();

    /**
     * Stop the packet in handler
     */
    void stop();

    // **************
    // MessageHandler
    // **************

    virtual void Handle(SwitchConnection *swConn, int msgType,
                        ofpbuf *msg);

private:
    Agent& agent;
    IntFlowManager& intFlowManager;
    PortMapper* intPortMapper;
    PortMapper* accessPortMapper;
    FlowReader* intFlowReader;
    SwitchConnection* intSwConnection;
    SwitchConnection* accSwConnection;
};
} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PACKETINHANDLER_H */
