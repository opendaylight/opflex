/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__NOIRO__POLICY_AGENT__COMMS_H
#define _INCLUDE__NOIRO__POLICY_AGENT__COMMS_H

#include <uv.h>

typedef struct peer_ peer_t;
typedef union peer_db_ peer_db_t;

extern peer_db_t peers;

typedef uv_loop_t * (*uv_loop_selector_fn)(void);

#include <noiro/tricks/overload.h>

int comms_passive_listener(char const * ip_address, uint16_t port,
        uv_loop_t * listener_uv_loop, uv_loop_selector_fn uv_loop_selector,
        peer_t * peer);
#define comms_passive_listener(...) \
    VARIADIC_FN(comms_passive_listener, ##__VA_ARGS__)
#define comms_passive_listener_05args comms_passive_listener
#define comms_passive_listener_04args(_1, _2, _3, _4)           \
               comms_passive_listener(_1, _2, _3, _4, NULL)
#define comms_passive_listener_03args(_1, _2, _3)               \
               comms_passive_listener(_1, _2, _3, NULL, NULL)
#define comms_passive_listener_02args(_1, _2)                   \
               comms_passive_listener(_1, _2, NULL, NULL, NULL)

int comms_active_connection(char const * host, char const * service,
        uv_loop_selector_fn uv_loop_selector, peer_t * peer);
#define comms_active_connection(...) \
    VARIADIC_FN(comms_active_connection, ##__VA_ARGS__)
#define comms_active_connection_04args comms_active_connection
#define comms_active_connection_03args(_1, _2, _3)               \
               comms_active_connection(_1, _2, _3, NULL)
#define comms_active_connection_02args(_1, _2)                   \
               comms_active_connection(_1, _2, NULL, NULL)
#define comms_active_connection_01args(peer)                     \
               comms_active_connection(NULL, NULL, NULL, peer)

#endif /* _INCLUDE__NOIRO__POLICY_AGENT__COMMS_H */
