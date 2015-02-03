/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

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

::yajr::Listener * ::yajr::Listener::create(
        const std::string& ip_address,
        uint16_t port,
        ::yajr::Peer::StateChangeCb connectionHandler,
        ::yajr::Listener::AcceptCb acceptHandler,
        void * data,
        uv_loop_t * listenerUvLoop,
        ::yajr::Peer::UvLoopSelector uvLoopSelector
    ) {

    LOG(INFO) << ip_address << ":" << port;

    ::yajr::comms::internal::ListeningPeer * peer;
    if (!(peer = new (std::nothrow) ::yajr::comms::internal::ListeningPeer(
            connectionHandler,
            acceptHandler,
            data,
            listenerUvLoop,
            uvLoopSelector))) {
        LOG(WARNING) <<  "out of memory, unable to create listener";
        return NULL;
    }

    int rc;

    if ((rc = peer->setAddrFromIpAndPort(ip_address, port))) {
        LOG(WARNING) << "addr_from_ip_and_port: [" << uv_err_name(rc) <<
            "] " << uv_strerror(rc);
        assert(0);
        peer->destroy();
        return NULL;
    }

    LOG(DEBUG) << peer << " queued up for listening";
    peer->insert(::yajr::comms::internal::Peer::LoopData::TO_LISTEN);

    return peer;
}

namespace yajr {
    namespace comms {

using namespace yajr::comms::internal;

void ::yajr::comms::internal::ListeningPeer::retry() {

    if (destroying_) {

        LOG(INFO) << this << "Not retrying because of pending destroy";

        return;

    }

    int rc;

    if ((rc = uv_tcp_init(_.listener_.uvLoop_, &handle_))) {
        LOG(WARNING) << "uv_tcp_init: [" << uv_err_name(rc) << "]" <<
            uv_strerror(rc);
        goto failed_tcp_init;
    }

    LOG(DEBUG) << this << " up() for listening tcp init";
    up();

    if ((rc = uv_tcp_bind(&handle_,
                (struct sockaddr *) &listen_on_,
                0))) {
        LOG(WARNING) << "uv_tcp_bind: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        status_ = Peer::kPS_FAILED_BINDING;
        goto failed_after_init;
    }

    if ((rc = uv_listen((uv_stream_t*) &handle_, 1024,
                on_passive_connection))) {
        LOG(WARNING) << "uv_tcp_listen: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        status_ = Peer::kPS_FAILED_LISTENING;
        goto failed_after_init;
    }

    status_ = Peer::kPS_LISTENING;

    insert(internal::Peer::LoopData::LISTENING);

    LOG(DEBUG2) << "listening!";

    connected_ = 1;

    return;

failed_after_init:
    LOG(DEBUG) << "closing tcp handle because of immediate failure after init";
    uv_close((uv_handle_t*) &handle_, on_close);

failed_tcp_init:
    insert(internal::Peer::LoopData::RETRY_TO_LISTEN);

    onError(rc);
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

    LOG(DEBUG4);

    assert(status == 0);
    ListeningPeer * listener = Peer::get<ListeningPeer>(server_handle);
    PassivePeer *peer;

    if (!(peer = new (std::nothrow) PassivePeer(
                    listener->getConnectionHandler(),
                    listener->getConnectionHandlerData(),
                    listener->getUvLoopSelector()
                    ))) {
        LOG(WARNING) << "out of memory, dropping new peer on the floor";
        return;
    }

    int rc;
    if ((rc = peer->tcpInit())) {
        peer->onError(rc);
        peer->down();  // this is invoked with an intent to delete!
        return;
    }

    if ((rc = uv_accept(server_handle, (uv_stream_t*) &peer->handle_))) {
        LOG(DEBUG) << "uv_accept: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        peer->onError(rc);
        uv_close((uv_handle_t*)&peer->handle_, on_close);
        return;
    }

    if (peer->unchoke()) {
        return;
    }

    peer->insert(internal::Peer::LoopData::ONLINE);

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

int ::yajr::comms::internal::ListeningPeer::setAddrFromIpAndPort(
        const std::string& ip_address,
        uint16_t port) {

    LOG(DEBUG4);

#ifndef NDEBUG
    /* make Valgrind happy */
    listen_on_ = sockaddr_storage();
#endif

    int rc;
    struct sockaddr_in* addr4;
    struct sockaddr_in6* addr6;

    addr4 = (struct sockaddr_in*) &listen_on_;
    addr6 = (struct sockaddr_in6*) &listen_on_;

    if (!(rc = uv_inet_pton(AF_INET, ip_address.c_str(), &addr4->sin_addr))) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);
    } else {
        addr6->sin6_family = AF_INET6;
        if (!(rc = uv_inet_pton(AF_INET6, ip_address.c_str(), &addr6->sin6_addr))) {
            addr6->sin6_port = htons(port);
        }
    }

    return rc;
}

} /* yajr::comms::internal namespace */

} /* yajr::comms namespace */
} /* yajr namespace */

