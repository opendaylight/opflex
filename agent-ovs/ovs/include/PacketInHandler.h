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
                        public PortStatusListener,
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
     * Reconcile the provided reactive flow against the current system
     * state.
     *
     * @param fe the flow entry to reconcile
     * @return true if the flow should be ignored during reconcilation (and
     * therefore left as is), false if it must be compared with expected flows
     */
    bool reconcileReactiveFlow(const FlowEntryPtr& fe);

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

    // ******************
    // PortStatusListener
    // ******************

    void portStatusUpdate(const std::string& portName,
                          uint32_t portNo, bool fromDesc);

private:
    /**
     * Handle a packet-in for the learning table.  Perform MAC
     * learning for flood domains that require flooding for unknown
     * unicast traffic.  Note that this will only manage the reactive
     * flows associated with MAC learning; there are static flows to
     * enabling MAC learning elsewhere.  Any resulting flows or
     * packet-out messages are written directly to the connection.
     *
     * @param conn the openflow switch connection
     * @param pi the packet-in
     * @param pi_buffer_id the buffer ID associated with the packet-in
     * @param proto an openflow proto version
     * @param pkt the packet from the packet-in
     * @param flow the parsed flow from the packet
     */
    void handleLearnPktIn(SwitchConnection *conn,
                          struct ofputil_packet_in& pi,
                          uint32_t pi_buffer_id,
                          int proto,
                          const struct dp_packet* pkt,
                          struct flow& flow);
    bool writeLearnFlow(SwitchConnection *conn,
                        int proto,
                        struct ofputil_packet_in& pi,
                        struct flow& flow,
                        bool stage2);
    void dstFlowCb(const FlowEntryList& flows,
                   const opflex::modb::MAC& dstMac,
                   uint32_t outPort,
                   uint32_t fgrpId);
    void anyFlowCb(const FlowEntryList& flows);

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
