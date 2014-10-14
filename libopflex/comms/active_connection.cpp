/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/comms/comms-internal.hpp>
#include <opflex/logging/internal/logging.hpp>

namespace opflex { namespace comms {

using namespace opflex::comms::internal;

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
int comms_active_connection(
        ConnectionHandler & connectionHandler,
        char const * host, char const * service,
        uv_loop_selector_fn uv_loop_selector, ActivePeer * peer) {

    LOG(INFO) << host << ":" << service;

    struct addrinfo const hints = (struct addrinfo){
        /* .ai_flags    = */ 0,
        /* .ai_family   = */ AF_UNSPEC,
        /* .ai_socktype = */ SOCK_STREAM,
        /* .ai_protocol = */ IPPROTO_TCP,
    };

    if (peer) {
        peer->reset(uv_loop_selector);
    } else {
        if (!(peer = new (std::nothrow) ActivePeer(
                        connectionHandler,
                        uv_loop_selector))) {
            LOG(WARNING) << ": out of memory, dropping new peer on the floor";
            return UV_ENOMEM;
        }
    }

    d_intr_list_insert(&peer->peer_hook, &peers.attempting);

    int rc;
    if ((rc = uv_getaddrinfo(peer->uv_loop_selector(), &peer->dns_req,
                    on_resolved, host, service, &hints))) {
        LOG(WARNING) << "uv_getaddrinfo: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
    } else peer->up();

    return rc;
}



namespace internal {
/*
                     ____      _ _ _                _
                    / ___|__ _| | | |__   __ _  ___| | _____
                   | |   / _` | | | '_ \ / _` |/ __| |/ / __|
                   | |__| (_| | | | |_) | (_| | (__|   <\__ \
                    \____\__,_|_|_|_.__/ \__,_|\___|_|\_\___/

                                                                     (Callbacks)
*/

void on_active_connection(uv_connect_t *req, int status) {

    ActivePeer * peer = Peer::get(req);  // can't possibly crash yet

    LOG(DEBUG) << peer;

    int rc;
    if (status < 0) {
        LOG(WARNING) << "connect: [" << uv_err_name(status) <<
            "] " << uv_strerror(status);
        if ((rc = connect_to_next_address(peer))) {
            LOG(WARNING) << "connect: no more resolved addresses";
            uv_close((uv_handle_t*)req->handle, on_close);
        }
        return;
    }

    if(peer->_.ai) { /* we succeeded connecting before the last attempt */
        uv_freeaddrinfo(peer->_.ai);
    }
#ifdef OVERZEALOUS_ABOUT_CLEANNESS
    peer->ai_next = NULL;
#endif

    d_intr_list_unlink(&peer->peer_hook);
    d_intr_list_insert(&peer->peer_hook, &peers.online);

    if ((rc = uv_read_start(req->handle, alloc_cb, on_read))) {
        LOG(WARNING) << "uv_read_start: [" << uv_err_name(rc) << "] " <<
                    uv_strerror(rc);
        uv_close((uv_handle_t*)req->handle, on_close);
        return;
    }

    /* kick the ball */
    peer->onConnect();

}

void debug_resolution_entries(struct addrinfo const * ai);

void on_resolved(uv_getaddrinfo_t * req, int status, struct addrinfo *resp) {

    LOG(DEBUG);

    ActivePeer * peer = Peer::get(req);
    assert(!peer->passive);

    if (status < 0) {
        LOG(WARNING) << "getaddrinfo callback error: [" << uv_err_name(status) <<
            "] " << uv_strerror(status);
        peer->status = Peer::kPS_FAILED_TO_RESOLVE;
        uv_freeaddrinfo(resp);

        goto retry;
    }

    peer->status = Peer::kPS_RESOLVED;

    debug_resolution_entries(resp);

    int rc;

    /* FIXME: pass the loop along */
    if ((rc = uv_tcp_init(uv_default_loop(), &peer->handle))) {
        LOG(WARNING) << "uv_tcp_init: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        goto retry;
    }

    if ((rc = uv_tcp_keepalive(&peer->handle, 1, 60))) {
        LOG(WARNING) << "uv_tcp_keepalive: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        goto retry;
    }

    peer->_.ai_next = peer->_.ai = resp;
    if ((rc = connect_to_next_address(peer))) {
        LOG(WARNING) << "connect_to_next_address: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        goto retry;
    }

    peer->status = Peer::kPS_CONNECTING;

    return;

retry:
    d_intr_list_unlink(&peer->peer_hook);
    d_intr_list_insert(&peer->peer_hook, &peers.retry);

    peer->down();

    return;
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

void debug_address(struct addrinfo const * ai, size_t m = 0) {

    if (!LOG_SHOULD_EMIT(DEBUG)) {
        return;
    }

    /* most importantly! */
    if (!ai) {
        return;
    }

    char host[48] = "<n/a>";
    char service[6] = "<n/a>";

    static char const * msg[2][2] = {
        {
            "Attempting connect to host ",
            " on port ",
        },
        {
            "Resolved to ip ",
            " and port ",
        },
    };

    /* this won't block, thanks to the flags */
    (void) getnameinfo(ai->ai_addr,
                       ai->ai_addrlen,
                       host,
                       sizeof(host),
                       service,
                       sizeof(service),
                       NI_NUMERICHOST | NI_NUMERICSERV
                      );

    LOG(DEBUG)
        << msg[m][0] << host
        << msg[m][1] << service
    ;
    LOG(DEBUG)
        << " ai_flags=" << ai->ai_flags
    ;
    LOG(DEBUG)
        << " ai_family=" << ai->ai_family
    ;
    LOG(DEBUG)
        << " ai_socktype=" << ai->ai_socktype
    ;
    LOG(DEBUG)
        << " ai_protocol=" << ai->ai_protocol
    ;
    LOG(DEBUG)
        << " ai_addrlen=" << ai->ai_addrlen
    ;
    LOG(DEBUG)
        << " ai_canonname=" << (ai->ai_canonname ?: "")
    ;

}

void debug_resolution_entries(struct addrinfo const * ai) {

    if (!LOG_SHOULD_EMIT(DEBUG)) {
        return;
    }

    do {
        debug_address(ai, 1);
    } while ((ai = ai->ai_next));

}

int connect_to_next_address(ActivePeer * peer) {

    LOG(DEBUG);

    struct addrinfo const * ai = peer->_.ai_next;

    debug_address(ai);

    int rc = UV_EAI_FAIL;

    while (ai && (rc = uv_tcp_connect(&peer->connect_req, &peer->handle,
                ai->ai_addr, on_active_connection))) {
        ai = ai->ai_next;
        LOG(ai ? INFO : WARNING) << "uv_tcp_connect: [" << uv_err_name(rc) <<
            "] " << uv_strerror(rc);

        debug_address(ai);
    }

    /* 'ai' is either NULL or the one that is pending... */
    if (!(peer->_.ai_next = ai ? ai->ai_next : NULL)) {
        if(peer->_.ai) { /* on_active_connection(*, -1) could call us again */
            uv_freeaddrinfo(peer->_.ai);
        }
        peer->_.ai = NULL;
    }

    if (rc) {
        d_intr_list_unlink(&peer->peer_hook);
        d_intr_list_insert(&peer->peer_hook, &peers.retry);
    }

    return rc;
}

} /* opflex::comms::internal namespace */

}} /* opflex::comms and opflex namespaces */
