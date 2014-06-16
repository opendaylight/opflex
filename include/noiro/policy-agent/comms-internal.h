/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__NOIRO__POLICY_AGENT__COMMS_INTERNAL_H
#define _INCLUDE__NOIRO__POLICY_AGENT__COMMS_INTERNAL_H

#include <stdlib.h>  /* for malloc() and free() */
#include <stdbool.h> /* for true and false */
#include <noiro/adt/intrusive_dlist.h>
#include <noiro/policy-agent/comms.h>
#include <noiro/tricks/container_of.h>

/* we pick the storage class specifier here, and omit it at the definitions */
void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf);
void on_close(uv_handle_t * h);
void on_write(uv_write_t *req, int status);
void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);
void on_passive_connection(uv_stream_t * server_handle, int status);
int connect_to_next_address(peer_t * peer);
void on_active_connection(uv_connect_t *req, int status);
void on_resolved(uv_getaddrinfo_t * req, int status, struct addrinfo *resp);
int addr_from_ip_and_port(const char * ip_address, uint16_t port,
        struct sockaddr_storage * addr);



typedef struct listen_on_ listen_on_t;

struct peer_ {
    d_intr_hook_t peer_hook;
    uv_tcp_t handle;
    union {
        uv_write_t write_req;
        uv_connect_t connect_req;
        uv_getaddrinfo_t dns_req;
    };
    union {
        struct {
            struct addrinfo * ai;
            struct addrinfo const * ai_next;
        };
        struct {
            uv_loop_t * uv_loop;
        } listener;
    } _;
    uv_loop_selector_fn uv_loop_selector;
    unsigned char passive   :1;
    unsigned char got_resp  :1;
    unsigned char status    :5;
};

typedef struct listening_peer_ {
    struct peer_ peer;
    struct sockaddr_storage listen_on;
} listening_peer_t;

union peer_db_ {
    struct {
        d_intr_head_t online;
        d_intr_head_t retry;  /* periodically re-attempt to reconnect */
        d_intr_head_t listening;
        d_intr_head_t retry_listening;
    };
    d_intr_head_t __all_doubly_linked_lists[0];
};

struct listen_on_ {
    uint16_t port;
    char const ip_address[0];
};

typedef enum {
    kPS_ONLINE            = 0,  /* <--- don't touch these! */
    kPS_LISTENING         = 0,  /* <--- don't touch these! */
    kPS_RESOLVING,
    kPS_RESOLVED,
    kPS_CONNECTING,

    kPS_UNINITIALIZED,
    kPS_FAILED_TO_RESOLVE,
    kPS_FAILED_BINDING,
    kPS_FAILED_LISTENING,
} peer_status_t;

#define PEER_FROM_HANDLE(h) container_of(h, peer_t, handle)
#define PEER_FROM_HOOK(h)   container_of(h, peer_t, peer_hook)
#define PEER_FROM_WRQ(r)    container_of(r, peer_t, write_req)
#define PEER_FROM_CRQ(r)    container_of(r, peer_t, connect_req)
#define PEER_FROM_DRQ(r)    container_of(r, peer_t, dns_req)



/* FIXME: remove all of these later */
//char * bsd_strnstr(const char *s, const char *find, size_t slen)
int send_identity(peer_t * peer, char const * domain, char const * name);
int send_identity_response(peer_t * peer, char const * domain,
        char const * name);

void do_the_needful_for_identity(peer_t * peer, ssize_t nread, char * buf);

#endif /* _INCLUDE__NOIRO__POLICY_AGENT__COMMS_INTERNAL_H */
