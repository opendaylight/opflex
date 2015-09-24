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

#ifndef OVSAGENT_PACKETINHANDLER_H
#define OVSAGENT_PACKETINHANDLER_H

#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include "SwitchConnection.h"
#include "PortMapper.h"
#include "FlowReader.h"
#include "TableState.h"
#include "Agent.h"

namespace ovsagent {

class FlowManager;

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
    PacketInHandler(Agent& agent, FlowManager& flowManager);

    /**
     * Set the port mapper to use
     * @param m the port mapper
     */
    void setPortMapper(PortMapper* m) { portMapper = m; }

    /**
     * Set the flow reader to use
     * @param r the flow reader
     */
    void setFlowReader(FlowReader* r) { flowReader = r; }

    /**
     * Set the switch connection to use
     * @param c the switch connection
     */
    void registerConnection(SwitchConnection* c) { switchConnection = c; }

    /**
     * Unset the switch connection
     */
    void unregisterConnection() { switchConnection = NULL; }

    /**
     * Reconcile the provided reactive flow against the current system
     * state.
     *
     * @param fe the flow entry to reconcile
     * @return true if the flow should be ignored during reconcilation (and
     * therefore left as is), false if it must be compared with expected flows
     */
    bool reconcileReactiveFlow(const FlowEntryPtr& fe);

    // **************
    // MessageHandler
    // **************

    virtual void Handle(SwitchConnection *swConn, ofptype msgType,
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
     * @param proto an openflow proto object
     * @param pkt the packet from the packet-in
     * @param flow the parsed flow from the packet
     */
    void handleLearnPktIn(SwitchConnection *conn,
                          struct ofputil_packet_in& pi,
                          ofputil_protocol& proto,
                          struct dp_packet& pkt,
                          struct flow& flow);
    bool writeLearnFlow(SwitchConnection *conn,
                        ofputil_protocol& proto,
                        struct ofputil_packet_in& pi,
                        struct flow& flow,
                        bool stage2);
    void dstFlowCb(const FlowEntryList& flows,
                   const opflex::modb::MAC& dstMac,
                   uint32_t outPort,
                   uint32_t fgrpId);
    void anyFlowCb(const FlowEntryList& flows);

    Agent& agent;
    FlowManager& flowManager;
    PortMapper* portMapper;
    FlowReader* flowReader;
    SwitchConnection* switchConnection;
};
} /* namespace ovsagent */

#endif /* OVSAGENT_PACKETINHANDLER_H */
