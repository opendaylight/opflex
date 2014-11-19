/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <yajr/internal/comms.hpp>
#include <opflex/logging/internal/logging.hpp>

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

::yajr::Peer * ::yajr::Peer::create(
        std::string const & host,
        std::string const & service,
        ::yajr::Peer::StateChangeCb connectionHandler,
        void * data,
        UvLoopSelector uvLoopSelector
    ) {

    LOG(INFO) << host << ":" << service;

    ::yajr::comms::internal::ActivePeer * peer;
    if (!(peer = new (std::nothrow) ::yajr::comms::internal::ActivePeer(
                    host,
                    service,
                    connectionHandler,
                    data,
                    uvLoopSelector))) {
        LOG(WARNING) << ": out of memory, dropping new peer on the floor";
        return NULL;
    }

    LOG(DEBUG) << peer << " queued up for resolution";
    peer->insert(::yajr::comms::internal::Peer::LoopData::TO_RESOLVE);

    return peer;
}

namespace yajr { namespace comms {

using namespace yajr::comms::internal;

void ::yajr::comms::internal::ActivePeer::retry() {

    if (destroying_) {

        LOG(INFO) << this << "Not retrying because of pending destroy";

        return;

    }

    struct addrinfo const hints = (struct addrinfo){
        /* .ai_flags    = */ 0,
        /* .ai_family   = */ AF_UNSPEC,
        /* .ai_socktype = */ SOCK_STREAM,
        /* .ai_protocol = */ IPPROTO_TCP,
    };

    /* potentially switch uv loop if some rebalancing is needed */
    getLoopData()->down();
    handle_.loop = uvLoopSelector_(getData());
    getLoopData()->up();

    int rc;
    if ((rc = uv_getaddrinfo(
                    getUvLoop(),
                    &dns_req_,
                    on_resolved,
                    getHostname(),
                    getService(),
                    &hints))) {
        LOG(WARNING) << "uv_getaddrinfo: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        onError(rc);
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
    } else {
        LOG(DEBUG) << this << " up() for a pending getaddrinfo()";
        up();
        insert(internal::Peer::LoopData::ATTEMPTING_TO_CONNECT);
    }

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

    if (peer->destroying_) {
        LOG(INFO) << peer << " peer is being destroyed. down() it";
        peer->down();
        return;
    }

    void retry_later(ActivePeer * peer);

    int rc;
    if (status < 0) {
        LOG(WARNING) << "connect: [" << uv_err_name(status) <<
            "] " << uv_strerror(status);
        if ((rc = connect_to_next_address(peer))) {
            LOG(WARNING) << "connect: no more resolved addresses";
            retry_later(peer);
        }
        return;
    }

    if(peer->_.ai) { /* we succeeded connecting before the last attempt */
        uv_freeaddrinfo(peer->_.ai);
    }
#ifdef OVERZEALOUS_ABOUT_CLEANNESS
    peer->ai_next = NULL;
#endif

    peer->unlink();
    peer->insert(internal::Peer::LoopData::ONLINE);

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
    assert(!peer->passive_);
    assert(peer->peerType()[0]=='A');

    void retry_later(ActivePeer * peer);

    if (peer->destroying_) {
        LOG(INFO) << peer << " peer is being destroyed. down() it";
        peer->down();
        return;
    }

    if (status < 0) {
        LOG(WARNING) << "getaddrinfo callback error: [" << uv_err_name(status) <<
            "] " << uv_strerror(status);
        peer->status_ = Peer::kPS_FAILED_TO_RESOLVE;
        uv_freeaddrinfo(resp);

        return retry_later(peer);
    }

    peer->status_ = Peer::kPS_RESOLVED;

    debug_resolution_entries(resp);

    int rc;

    /* FIXME: pass the loop along */
    if ((rc = peer->tcpInit())) {
        return retry_later(peer);
    }

    peer->_.ai_next = peer->_.ai = resp;
    if ((rc = connect_to_next_address(peer))) {
        LOG(WARNING) << "connect_to_next_address: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        return retry_later(peer);
    }

    peer->status_ = Peer::kPS_CONNECTING;

    LOG(DEBUG) << peer << " waiting for connection completion";

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

void retry_later(ActivePeer * peer) {

    LOG(DEBUG) << peer;

    peer->unlink();
    peer->insert(internal::Peer::LoopData::RETRY_TO_CONNECT);

    LOG(DEBUG) << peer << " down() for a retry_later()";
    peer->down();

    return;
}

void swap_stack_on_close(uv_handle_t * h) {

    ActivePeer * peer = Peer::get<ActivePeer>(h);  // can't possibly crash yet

    LOG(DEBUG) << peer;

 // LOG(DEBUG) << "{" << peer << "}A down() for a swap_stack_on_close()";
 // peer->down();

    int rc;
    /* FIXME: pass the loop along */
    if ((rc = peer->tcpInit())) {

        retry_later(peer);

        return;
    }

    if ((rc = connect_to_next_address(peer, false))) {
        LOG(WARNING) << "connect_to_next_address: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        return retry_later(peer);
    }

}

int connect_to_next_address(ActivePeer * peer, bool swap_stack) {

    LOG(DEBUG) << peer;

    struct addrinfo const * ai = peer->_.ai_next;

    debug_address(ai);

    int rc = UV_EAI_FAIL;

    while (ai && (rc = uv_tcp_connect(&peer->connect_req_, &peer->handle_,
                ai->ai_addr, on_active_connection))) {
        LOG(ai ? INFO : WARNING) << "uv_tcp_connect: [" << uv_err_name(rc) <<
            "] " << uv_strerror(rc);

        if ((-EINVAL == rc) && swap_stack) {

            LOG(INFO) << "destroying socket and retrying";
            uv_close((uv_handle_t*)&peer->handle_, swap_stack_on_close);

            return 0;
        }

        ai = ai->ai_next;

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
        LOG(DEBUG) << "unable to issue a(nother) connect request";
    } else {
        LOG(DEBUG) << "issued a connect request";
    }

    return rc;
}

} /* yajr::comms::internal namespace */

}} /* yajr::comms and yajr namespaces */
