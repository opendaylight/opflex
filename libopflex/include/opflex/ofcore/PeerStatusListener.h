/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file PeerStatusListener.h
 * @brief Interface definition for peer status
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_CORE_PEERSTATUSLISTENER_H
#define OPFLEX_CORE_PEERSTATUSLISTENER_H

namespace opflex {
namespace ofcore {

/**
 * \addtogroup ofcore
 * @{
 */

/**
 * An interface for a listener that will get status update events for
 * peer connection state.
 */
class PeerStatusListener {
public:
    /**
     * The status of a particular peer, which will be sent in the peer
     * status update callback.
     */
    enum PeerStatus {
        /**
         * The peer is disconnected and not trying to connect
         */
        DISCONNECTED = 0,
        /**
         * The peer is connecting
         */
        CONNECTING = 1,
        /**
         * The peer is connected but not yet ready
         */
        CONNECTED = 2,
        /**
         * The peer is connected and ready
         */
        READY = 3,
        /**
         * The peer connection is closing
         */
        CLOSING = 4
    };

    /**
     * Called when the connection state changes for a given Opflex
     * peer.  Will not be called concurrently.
     *
     * @param peerHostname the hostname of the peer that was updated
     * @param peerPort the port number of the peer that was updated
     * @param peerStatus the new status of the Opflex peer
     */
    virtual void peerStatusUpdated(const std::string& peerHostname,
                                   int peerPort,
                                   PeerStatus peerStatus) {}

    /**
     * Represents the overall health of the opflex connection pool
     */
    enum Health {
        /**
         * There is no ready Opflex peer connection
         */
        DOWN = 0,
        /**
         * At least one opflex peer is in a state other than READY
         */
        DEGRADED = 1,
        /**
         * All opflex peers are connected and ready
         */
        HEALTHY = 2
    };

    /**
     * Called when the overall health of the opflex connection pool
     * changes.  Will not be called concurrently.
     *
     * @param health the new health value
     */
    virtual void healthUpdated(Health health) {}
};

/** @} ofcore */

} /* namespace ofcore */
} /* namespace opflex */

#endif /* OPFLEX_CORE_PEERSTATUSLISTENER_H */
