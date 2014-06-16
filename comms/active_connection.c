/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <noiro/policy-agent/comms-internal.h>
#include <noiro/debug_with_levels.h>

/*
                             _        _   _
                            / \   ___| |_(_)_   _____
                           / _ \ / __| __| \ \ / / _ \
                          / ___ \ (__| |_| |\ V /  __/
                         /_/   \_\___|\__|_| \_/ \___|

              ____                            _   _
             / ___|___  _ __  _ __   ___  ___| |_(_) ___  _ __  ___
            | |   / _ \| '_ \| '_ \ / _ \/ __| __| |/ _ \| '_ \/ __|
            | |__| (_) | | | | | | |  __/ (__| |_| | (_) | | | \__ \
             \____\___/|_| |_|_| |_|\___|\___|\__|_|\___/|_| |_|___/

                                                            (Active Connections)
*/



/*
   ____        _     _ _        _       _             __
  |  _ \ _   _| |__ | (_) ___  (_)_ __ | |_ ___ _ __ / _| __ _  ___ ___  ___
  | |_) | | | | '_ \| | |/ __| | | '_ \| __/ _ \ '__| |_ / _` |/ __/ _ \/ __|
  |  __/| |_| | |_) | | | (__  | | | | | ||  __/ |  |  _| (_| | (_|  __/\__ \
  |_|    \__,_|_.__/|_|_|\___| |_|_| |_|\__\___|_|  |_|  \__,_|\___\___||___/

                                                             (Public interfaces)
*/

/* only pass peer when re-attempting to listen following a failure */
int comms_active_connection(char const * host, char const * service,
        uv_loop_selector_fn uv_loop_selector, peer_t * peer) {

    DBUG_ENTER(__func__);

    struct addrinfo const hints = {
        .ai_family = PF_INET6,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_flags = 0,
    };

    if (peer) {
        d_intr_list_unlink(&peer->peer_hook);
        uv_loop_selector = peer->uv_loop_selector ? : uv_loop_selector;
    } else {
        if (!(peer = malloc(sizeof(*peer)))) {
            DBUG_PRINT(kDBG_E, ("out of memory, dropping new peer on the "
                        " floor"));
            DBUG_RETURN(UV_ENOMEM);
        }
    }

    *peer = (peer_t){
        .passive          = false,
        .status           = kPS_RESOLVING,
        .uv_loop_selector = uv_loop_selector ? : &uv_default_loop,
    };

    int rc;
    if ((rc = uv_getaddrinfo(peer->uv_loop_selector(), &peer->dns_req,
                    on_resolved, host, service, &hints))) {
        DBUG_PRINT(kDBG_E, ("uv_getaddrinfo: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc))); 
    }

    DBUG_RETURN(rc);
}



/*
                     ____      _ _ _                _
                    / ___|__ _| | | |__   __ _  ___| | _____
                   | |   / _` | | | '_ \ / _` |/ __| |/ / __|
                   | |__| (_| | | | |_) | (_| | (__|   <\__ \
                    \____\__,_|_|_|_.__/ \__,_|\___|_|\_\___/

                                                                     (Callbacks)
*/

void on_active_connection(uv_connect_t *req, int status) {

    DBUG_ENTER(__func__);

    peer_t * peer = PEER_FROM_HANDLE(req->handle);

    int rc;
    if (status == -1) {
        DBUG_PRINT(kDBG_E, ("connect: %s", uv_err_name(status)));
        if ((rc = connect_to_next_address(peer))) {
            DBUG_PRINT(kDBG_E, ("connect: no more resolved addresses"));
            uv_close((uv_handle_t*)req->handle, on_close);
        }
        DBUG_VOID_RETURN;
    }

    if(peer->ai) { /* we succeeded connecting before the last attempt */
        uv_freeaddrinfo(peer->ai);
    }
#ifdef OVERZEALOUS_ABOUT_CLEANNESS
    peer->ai_next = NULL;
#endif

    d_intr_list_insert(&peer->peer_hook, &peers.online);

    if ((rc = uv_read_start(req->handle, alloc_cb, on_read))) {
        DBUG_PRINT(kDBG_E, ("uv_read_start: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc)));
        uv_close((uv_handle_t*)req->handle, on_close);
        DBUG_VOID_RETURN;
    }

    /* FIXME: remove hard-coding */
    if ((rc = send_identity(peer, "noironetworks.com", "bogus"))) {
        DBUG_PRINT(kDBG_E, ("send_identity: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc)));
        DBUG_VOID_RETURN;
    }
}

void on_resolved(uv_getaddrinfo_t * req, int status, struct addrinfo *resp) {

    DBUG_ENTER(__func__);

    peer_t * peer = PEER_FROM_DRQ(req);
    assert(!peer->passive);

    if (status == -1) {
        DBUG_PRINT(kDBG_E,
                ("getaddrinfo callback error %s", uv_err_name(status)));
        peer->status = kPS_FAILED_TO_RESOLVE;
        uv_freeaddrinfo(resp);
        DBUG_VOID_RETURN;
    }

    peer->status = kPS_RESOLVED;

    int rc;

    /* FIXME: pass the loop along */
    if ((rc = uv_tcp_init(uv_default_loop(), &peer->handle))) {
        DBUG_PRINT(kDBG_E,
                ("uv_tcp_init: [%s] %s", uv_err_name(rc), uv_strerror(rc)));
        DBUG_VOID_RETURN;
    }

    if ((rc = uv_tcp_keepalive(&peer->handle, 1, 60))) {
        DBUG_PRINT(kDBG_E,
                ("uv_tcp_keepalive: [%s] %s", uv_err_name(rc),
                 uv_strerror(rc)));
        DBUG_VOID_RETURN;
    }

    peer->ai_next = peer->ai = resp;
    if ((rc = connect_to_next_address(peer))) {
        DBUG_PRINT(kDBG_E,
                ("connect_to_next_address: [%s] %s", uv_err_name(rc),
                 uv_strerror(rc)));
        DBUG_VOID_RETURN;
    }

    peer->status = kPS_CONNECTING;

    /* TODO: plug-in parser */
}



/*
     _   _ _   _ _ _ _            __                  _   _
    | | | | |_(_) (_) |_ _   _   / _|_   _ _ __   ___| |_(_) ___  _ __  ___
    | | | | __| | | | __| | | | | |_| | | | '_ \ / __| __| |/ _ \| '_ \/ __|
    | |_| | |_| | | | |_| |_| | |  _| |_| | | | | (__| |_| | (_) | | | \__ \
     \___/ \__|_|_|_|\__|\__, | |_|  \__,_|_| |_|\___|\__|_|\___/|_| |_|___/
                         |___/
                                                             (Utility functions)
*/

int connect_to_next_address(peer_t * peer) {

    DBUG_ENTER(__func__);

    struct addrinfo const * ai = peer->ai_next;

    int rc = UV_EAI_FAIL;

    while (ai && (rc = uv_tcp_connect(&peer->connect_req, &peer->handle,
             // (struct sockaddr const *)&dest, on_active_connection)) {
                ai->ai_addr, on_active_connection))) {
        ai = ai->ai_next;
        DBUG_PRINT(ai ? kDBG_W : kDBG_E,
                ("uv_tcp_connect: [%s] %s", uv_err_name(rc), uv_strerror(rc)));
    }

    /* 'ai' is either NULL or the one that is pending... */
    if (!(peer->ai_next = ai ? ai->ai_next : NULL)) {
        if(peer->ai) { /* on_active_connection(*, -1) could call us again */
            uv_freeaddrinfo(peer->ai);
        }
        peer->ai = NULL;
    }

    DBUG_RETURN(rc);
}

