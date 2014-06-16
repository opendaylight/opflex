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
                         ____               _
                        |  _ \ __ _ ___ ___(_)_   _____
                        | |_) / _` / __/ __| \ \ / / _ \
                        |  __/ (_| \__ \__ \ |\ V /  __/
                        |_|   \__,_|___/___/_| \_/ \___|

                    _     _     _
                   | |   (_)___| |_ ___ _ __   ___ _ __ ___
                   | |   | / __| __/ _ \ '_ \ / _ \ '__/ __|
                   | |___| \__ \ ||  __/ | | |  __/ |  \__ \
                   |_____|_|___/\__\___|_| |_|\___|_|  |___/

                                                             (Passive Listeners)
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
int comms_passive_listener(char const * ip_address, uint16_t port,
        uv_loop_t * listener_uv_loop, uv_loop_selector_fn uv_loop_selector,
        peer_t * peer) {

    DBUG_ENTER(__func__);

    int rc;

    /* well, this is not really a 'peer' */
    if (peer) {
        d_intr_list_unlink(&peer->peer_hook);
        listener_uv_loop = peer->_.listener.uv_loop ? : listener_uv_loop;
        uv_loop_selector = peer->uv_loop_selector ? : uv_loop_selector;
    } else {
        if (!(peer = malloc(sizeof(listening_peer_t)))) {
            DBUG_PRINT(kDBG_E, ("out of memory, unable to create listener"));
            DBUG_RETURN(UV_ENOMEM);
        }
        if ((rc = addr_from_ip_and_port(ip_address, port,
                        &((listening_peer_t *)peer)->listen_on))) {
            DBUG_PRINT(kDBG_E, ("addr_from_ip_and_port: [%s] %s",
                        uv_err_name(rc), uv_strerror(rc)));
            DBUG_RETURN(rc);
        }
    }

    *peer = (peer_t){  /* doesn't nuke 'listen_on' field, because it's extra */
        .status           = kPS_UNINITIALIZED,
        .passive          = false,  /* yes, it is listening but not "passive" */
        .uv_loop_selector = uv_loop_selector ? : &uv_default_loop,
        ._                = {
            .listener     = {
                .uv_loop  = listener_uv_loop ? : uv_default_loop(),
            },
        },
    };

    if ((rc = uv_tcp_init(peer->uv_loop_selector(), &peer->handle))) {
        DBUG_PRINT(kDBG_E, ("uv_tcp_init: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc))); 
        goto failed_tcp_init;
    }

    if ((rc = uv_tcp_bind(&peer->handle,
                (struct sockaddr *) &((listening_peer_t *)peer)->listen_on,
                0))) {
        DBUG_PRINT(kDBG_E, ("uv_tcp_bind: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc))); 
        peer->status = kPS_FAILED_BINDING;
        goto failed_bind;
    }

    if ((rc = uv_listen((uv_stream_t*) &peer->handle, 1024,
                on_passive_connection))) {
        DBUG_PRINT(kDBG_E, ("uv_tcp_listen: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc))); 
        peer->status = kPS_FAILED_LISTENING;
        goto failed_bind;
    }

    peer->status = kPS_LISTENING;

    d_intr_list_insert(&peer->peer_hook, &peers.listening);

    DBUG_RETURN(0);

failed_bind:
    uv_close((uv_handle_t*) &peer->handle, NULL);

failed_tcp_init:
    d_intr_list_insert(&peer->peer_hook, &peers.retry_listening);

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

void on_passive_connection(uv_stream_t * server_handle, int status)
{

    DBUG_ENTER(__func__);

    assert(status == 0);
    peer_t *peer;

    if (!(peer = malloc(sizeof(*peer)))) {
        DBUG_PRINT(kDBG_E, ("out of memory, dropping new peer on the floor"));
        DBUG_VOID_RETURN;
    }

    *peer = (peer_t){
        .passive    = true,
        .status     = kPS_UNINITIALIZED,
    };

    /* FIXME: pass the loop along */
    uv_tcp_init(uv_default_loop(), &peer->handle);

    int rc;
    if ((rc = uv_accept(server_handle, (uv_stream_t*) &peer->handle))) {
        DBUG_PRINT(kDBG_D, ("uv_accept()=%d", rc));
        free(peer);
        DBUG_VOID_RETURN;
    }

    if ((rc = uv_read_start((uv_stream_t*) &peer->handle, alloc_cb, on_read))) {
        DBUG_PRINT(kDBG_E, ("uv_read_start: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc)));
        uv_close((uv_handle_t*)&peer->handle, on_close);
        DBUG_VOID_RETURN;
    }

    d_intr_list_insert(&peer->peer_hook, &peers.online);

    /* FIXME: remove hardcoding and move to proper parser */
    if ((rc = send_identity(peer, "noironetworks.com", "bogus [P]"))) {
        DBUG_PRINT(kDBG_E, ("send_identity: [%s] %s", uv_err_name(rc),
                    uv_strerror(rc)));
        DBUG_VOID_RETURN;
    }

    DBUG_VOID_RETURN;
}



/*
     _   _ _   _ _ _ _            __                  _   _
    | | | | |_(_) (_) |_ _   _   / _|_   _ _ __   ___| |_(_) ___  _ __  ___
    | | | | __| | | | __| | | | | |_| | | | '_ \ / __| __| |/ _ \| '_ \/ __|
    | |_| | |_| | | | |_| |_| | |  _| |_| | | | | (__| |_| | (_) | | | \__ \
     \___/ \__|_|_|_|\__|\__, | |_|  \__,_|_| |_|\___|\__|_|\___/|_| |_|___/
                         |___/
*/

int addr_from_ip_and_port(const char * ip_address, uint16_t port,
        struct sockaddr_storage * addr) {

    DBUG_ENTER(__func__);

    int rc;
    struct sockaddr_in* addr4;
    struct sockaddr_in6* addr6;

    addr4 = (struct sockaddr_in*) addr;
    addr6 = (struct sockaddr_in6*) addr;

    if (!(rc = uv_inet_pton(AF_INET, ip_address, &addr4->sin_addr))) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);
    } else {
        addr6->sin6_family = AF_INET6;
        if (!(rc = uv_inet_pton(AF_INET6, ip_address, &addr6->sin6_addr))) {
            addr6->sin6_port = htons(port);
        }
    }

    DBUG_RETURN(rc);
}
