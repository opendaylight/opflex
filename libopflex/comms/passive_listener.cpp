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
int comms_passive_listener(
        ConnectionHandler & connectionHandler,
        char const * ip_address, uint16_t port,
        uv_loop_t * listener_uv_loop, uv_loop_selector_fn uv_loop_selector,
        ListeningPeer * peer) {

    LOG(INFO) << ip_address << ":" << port;

    int rc;

    /* well, this is not really a 'peer' */
    if (peer) {
        peer->reset(listener_uv_loop, uv_loop_selector);
    } else {
        if (!(peer = new (std::nothrow) ListeningPeer(
                        connectionHandler,
                        listener_uv_loop,
                        uv_loop_selector))) {
            LOG(WARNING) <<  "out of memory, unable to create listener";
            return UV_ENOMEM;
        }
        if ((rc = addr_from_ip_and_port(ip_address, port,
                        &((ListeningPeer *)peer)->listen_on))) {
            LOG(WARNING) << "addr_from_ip_and_port: [" << uv_err_name(rc) <<
                "] " << uv_strerror(rc);
            return rc;
        }
    }

    if ((rc = uv_tcp_init(peer->uv_loop_selector(), &peer->handle))) {
        LOG(WARNING) << "uv_tcp_init: [" << uv_err_name(rc) << "]" <<
            uv_strerror(rc);
        goto failed_tcp_init;
    }

    if ((rc = uv_tcp_bind(&peer->handle,
                (struct sockaddr *) &((ListeningPeer *)peer)->listen_on,
                0))) {
        LOG(WARNING) << "uv_tcp_bind: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        peer->status = Peer::kPS_FAILED_BINDING;
        goto failed_bind;
    }

    if ((rc = uv_listen((uv_stream_t*) &peer->handle, 1024,
                on_passive_connection))) {
        LOG(WARNING) << "uv_tcp_listen: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        peer->status = Peer::kPS_FAILED_LISTENING;
        goto failed_bind;
    }

    peer->up();

    peer->status = Peer::kPS_LISTENING;

    d_intr_list_insert(&peer->peer_hook, &peers.listening);

    LOG(DEBUG) << "listening!";

    return 0;

failed_bind:
    uv_close((uv_handle_t*) &peer->handle, NULL);

failed_tcp_init:
    d_intr_list_insert(&peer->peer_hook, &peers.retry_listening);

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

void on_passive_connection(uv_stream_t * server_handle, int status)
{

    LOG(DEBUG);

    assert(status == 0);
    ListeningPeer * listener = Peer::get<ListeningPeer>(server_handle);
    PassivePeer *peer;

    if (!(peer = new (std::nothrow) PassivePeer(listener->connectionHandler_))) {
        LOG(WARNING) << "out of memory, dropping new peer on the floor";
        return;
    }

    /* FIXME: check for failures */
    uv_tcp_init(listener->uv_loop_selector(), &peer->handle);
    int rc;
    if ((rc = uv_accept(server_handle, (uv_stream_t*) &peer->handle))) {
        LOG(DEBUG) << "uv_accept: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        peer->down();  // this is invoked with an intent to delete!
        return;
    }

    peer->up();

    if ((rc = uv_read_start((uv_stream_t*) &peer->handle, alloc_cb, on_read))) {
        LOG(WARNING) << "uv_read_start: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        uv_close((uv_handle_t*)&peer->handle, on_close);
        return;
    }

    d_intr_list_insert(&peer->peer_hook, &peers.online);

    /* kick the ball */
    peer->onConnect();

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

    LOG(DEBUG);

#ifndef NDEBUG
    /* make Valgrind happy */
    *addr = sockaddr_storage();
#endif

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

    return rc;
}

} /* opflex::comms::internal namespace */

}} /* opflex::comms and opflex naMespaces */
