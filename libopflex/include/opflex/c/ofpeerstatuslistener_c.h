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
 * @addtogroup ccore
 * @{
 */

/**
 * @defgroup cofpeerstatuslistener Peer Status Listener
 * An object that monitors for updates in that data store for individual
 * connections to the opflex peers and overall status of the connection
 * pool. There are functions to create, destroy and register the object.
 * During creation, the caller must arrange to pass two C function pointers
 * which the C++ member listener functions of the object will call to
 * send back the update information to the callers C code domain.
 * @{
 */

/**
 * @defgroup cpeerstatus Peer Status Codes
 * Defines status codes for peer status updates
 * @{
 */

/**
 * The peer is disconnected and not trying to connect
 */
#define OF_PEERSTATUS_DISCONNECTED 0
/**
 * The peer is connecting 
 */
#define OF_PEERSTATUS_CONNECTING 1
/**
 * The peer is connected but not yet ready
 */
#define OF_PEERSTATUS_CONNECTED 2
/**
 * The peer is connected and ready
 */
#define OF_PEERSTATUS_READY 3
/**
 * The peer connection is closing 
 */
#define OF_PEERSTATUS_CLOSING 4
/**
 * An error occurred
 */
#define OF_PEERSTATUS_ERROR -1

/**
 * @} cpeerstatus
 *
 * @defgroup cpoolhealth Connection Pool Health Status Codes
 * Defines status codes for connection pool health status updates
 * @{
 */

/**
 * There is no ready opflex peer connection
 */
#define OF_POOLHEALTH_DOWN 0
/**
 * At least one opflex peer is in a state other than READY
 */
#define OF_POOLHEALTH_DEGRADED 1
/**
 * All opflex peers are connected and ready
 */
#define OF_POOLHEALTH_HEALTHY 2
/**
 * An error occurred
 */
#define OF_POOLHEALTH_ERROR -1

/**
 * @} cpoolhealth
 */

/**
 * A pointer to peer status listener object
 */
typedef ofobj_p ofpeerstatuslistener_p;

#ifdef __cplusplus
extern "C" {
#endif

    /** 
     * A function pointer to receive peer status updates.  Called when
     * the connection state changes for a given Opflex peer.  Will not
     * be called concurrently.
     * 
     * @param user_data a pointer to an opaque user data structure
     * @param peerhostname the hostname of the peer that was updated
     * @param port the port number of the peer that was updated
     * @param status the new status of the Opflex peer
     * @see cpeerstatus
     */
    typedef void (*ofpeerstatus_peer_p)(void* user_data, 
                                        const char *peerhostname, 
                                        int port, 
                                        int status);

    /** 
     * A function pointer to recieve connection pool health status
     * updates.  Called when the overall health of the opflex
     * connection pool changes.  Will not be called concurrently.
     *
     * @param user_data a pointer to an opaque user data structure
     * @param health status code for connection pool health.
     * @see cpoolhealth
     */
    typedef void (*ofpeerstatus_health_p)(void* user_data, 
                                          int health);

    /**
     * Creates peer status listener object.  You must eventually call
     * @ref ofpeerstatuslistener_destroy on the returned object.
     *
     * @param obj return the pointer to created object
     * @param user_data an opaque data blob that will be passed to
     * your handler
     * @param peer_callback caller provided function pointer for
     * handling status callback of any peer
     * @param health_callback caller provided functional pointer for
     * handling peer connection pool health callback
     * @return a status code
     */
    ofstatus ofpeerstatuslistener_create(void* user_data,
                                         ofpeerstatus_peer_p peer_callback,
                                         ofpeerstatus_health_p health_callback,
                                         /* out */ ofpeerstatuslistener_p *obj);

    /**
     * Destroy the peer status listener object, and zero the pointer.
     * You must ensure that the listener is no longer being used by
     * any framework objects.
     *
     * @param obj peer status listener obj
     * @return a status code
     */
    ofstatus ofpeerstatuslistener_destroy(/* out */ ofpeerstatuslistener_p *obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} cofpeerstatuslistener */
/** @} ccore */
/** @} cwrapper */

#endif /* OPFLEX_C_OFPEERSTATUSLISTENER_H */
