/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ofpeerstatuslistener_c.h
 * @brief C wrapper for peer status listener
 */
/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include "ofcore_c.h"

#ifndef OPFLEX_C_OFPEERSTATUSLISTENER_H
#define OPFLEX_C_OFPEERSTATUSLISTENER_H

/**
 * @addtogroup cwrapper 
 * @{
 * @addtogroup cmodb
 * @{
 * @defgroup cofpeerstatuslistener Opflex peer status listener object
 * An object that monitors for updates in that data store for individual
 * connections to the opflex peers and overall status of the connection
 * pool. There are functions to create, destroy and register the object.
 * During creation, the caller must arrange to pass two C function pointers
 * which the C++ member listener functions of the object will call to
 * send back the update information to the callers C code domain.
 * @{
 */

/**
 * the peer is disconnected and not trying to connect
 */
#define OF_PEERSTATUS_DISCONNECTED 0
/**
 * the peer is connecting 
 */
#define OF_PEERSTATUS_CONNECTING   1
/**
 * the peer is connected but not yet ready
 */
#define OF_PEERSTATUS_CONNECTED    2
/**
 * The peer is connected and ready
 */
#define OF_PEERSTATUS_READY        3
/**
 * the peer connection is closing 
 */
#define OF_PEERSTATUS_CLOSING      4
/**
 * error code
 */
#define OF_PEERSTATUS_ERROR        -1

/**
 * there is no ready opflex peer connection
 */
#define OF_CONNHEALTH_DOWN         0
/**
 * at least one opflex peer is in a state other than READY
 */
#define OF_CONNHEALTH_DEGRADED     1
/**
 * all opflex peers are connected and ready
 */
#define OF_CONNHEALTH_HEALTHY      2
/**
 * error code
 */
#define OF_CONNHEALTH_ERROR        -1



#ifdef __cplusplus
extern "C" {
#endif

    /** 
     * pointer to callback function in C caller code for handling the 
     * peer connection status callback from modb
     * 
     * @param peerhostname hostname of the peer
     * @param port peer port
     * @param peerconn_status_code a status code
     */
    typedef void (*ofpeerconnstatus_callback_p)(const char *peerhostname, 
                                                int port, 
                                                int peerconn_status_code);
    /** 
     * pointer to callback function in C caller code for handling the 
     * overall connection pool health callback from modb
     *
     * @param connpoolhealth_status_code status code for connection pool
     */
    typedef void (*ofconnpoolhealth_callback_p)(int connpoolhealth_status_code);

    /**
     * Creates peer status listener object
     * @param obj return the pointer to created object
     * @param cpeerconnstatus_callback caller provided function pointer for 
     *        handling status callback of any peer
     * @param cconnpoolhealth_callback caller provided functional pointer for
     *        handling peer connection pool health callback
     * @return a status code
     */
    ofstatus ofpeerstatuslistener_create(
                         /* out */ ofpeerstatuslistener_p *obj,
                         ofpeerconnstatus_callback_p cpeerconnstatus_callback,
                         ofconnpoolhealth_callback_p cconnpoolhealth_callback);
    /**
     * destroy the PeerStatusListener object used in register function earlier
     *
     * @param obj peer status listener obj
     * @return a status code
     */
    ofstatus ofpeerstatuslistener_destroy(/* out */ ofpeerstatuslistener_p *obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} cofpeerstatuslistener */
/** @} cmodb */
/** @} cwrapper */

#endif /* OPFLEX_C_OFPEERSTATUSLISTENER_H */
