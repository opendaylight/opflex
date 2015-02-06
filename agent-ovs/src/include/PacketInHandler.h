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

#include "SwitchConnection.h"
#include "Agent.h"

namespace ovsagent {

class FlowManager;

/**
 * Handler for packet-in messages arriving from the switch
 */
class PacketInHandler : public MessageHandler {
public:
    /**
     * Construct a PacketInHandler
     */
    PacketInHandler(Agent& agent, FlowManager& flowManager);

    // **************
    // MessageHandler
    // **************
    virtual void Handle(SwitchConnection *swConn, ofptype msgType,
                        ofpbuf *msg);

private:
    Agent& agent;
    FlowManager& flowManager;
};
} /* namespace ovsagent */

#endif /* OVSAGENT_PACKETINHANDLER_H */
