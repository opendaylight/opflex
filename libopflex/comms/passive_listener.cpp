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


#include <opflex/yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <sys/un.h>

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

    ::yajr::comms::internal::ListeningTcpPeer * peer = NULL;
#if __cpp_exceptions || __EXCEPTIONS
    try {
#endif
        peer = new ::yajr::comms::internal::ListeningTcpPeer(
            connectionHandler,
            acceptHandler,
            data,
            listenerUvLoop,
            uvLoopSelector);
#if __cpp_exceptions || __EXCEPTIONS
    } catch(const std::bad_alloc&) {
    }
#endif

    if (!peer) {
        LOG(WARNING) << "out of memory, unable to create listener";
        return NULL;
    }

    int rc;

    if ((rc = peer->setAddrFromIpAndPort(ip_address, port))) {
        LOG(WARNING)
            << "addr_from_ip_and_port: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        assert(0);
        peer->destroy();
        return NULL;
    }

    peer->insert(::yajr::comms::internal::Peer::LoopData::TO_LISTEN);
    return peer;
}

::yajr::Listener * ::yajr::Listener::create(
        const std::string& socketName,
        ::yajr::Peer::StateChangeCb connectionHandler,
        ::yajr::Listener::AcceptCb acceptHandler,
        void * data,
        uv_loop_t * listenerUvLoop,
        ::yajr::Peer::UvLoopSelector uvLoopSelector
    ) {

    VLOG(1)
        << socketName
    ;

    ::yajr::comms::internal::ListeningUnixPeer * peer = NULL;
#if __cpp_exceptions || __EXCEPTIONS
    try {
#endif
        peer = new ::yajr::comms::internal::ListeningUnixPeer(
                socketName,
                connectionHandler,
                acceptHandler,
                data,
                listenerUvLoop,
                uvLoopSelector);
#if __cpp_exceptions || __EXCEPTIONS
    } catch(const std::bad_alloc&) {
    }
#endif

    if (!peer) {
        LOG(WARNING) << "out of memory, unable to create listener";
        return NULL;
    }

    peer->insert(::yajr::comms::internal::Peer::LoopData::TO_LISTEN);
    return peer;
}

namespace yajr {
    namespace comms {

using namespace yajr::comms::internal;

void ::yajr::comms::internal::ListeningTcpPeer::retry() {

    if (destroying_) {
        LOG(INFO) << this << "Not retrying because of pending destroy";
        return;
    }

    int rc;

    if ((rc = uv_tcp_init(_.listener_.uvLoop_,
                    reinterpret_cast<uv_tcp_t *>(getHandle())))) {
        LOG(WARNING)
            << "uv_tcp_init: ["
            << uv_err_name(rc)
            << "]"
            << uv_strerror(rc)
        ;
        goto failed_tcp_init;
    }

    up();

    if ((rc = uv_tcp_bind(reinterpret_cast<uv_tcp_t *>(getHandle()),
                (struct sockaddr *) &listen_on_,
                0))) {
        LOG(WARNING)
            << "uv_tcp_bind: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        status_ = Peer::kPS_FAILED_BINDING;
        goto failed_after_init;
    }

    if ((rc = uv_listen((uv_stream_t*) getHandle(), 1024,
                on_passive_connection))) {
        LOG(WARNING)
            << "uv_tcp_listen: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        status_ = Peer::kPS_FAILED_LISTENING;
        goto failed_after_init;
    }

    status_ = Peer::kPS_LISTENING;
    insert(internal::Peer::LoopData::LISTENING);
    connected_ = 1;

    return;

failed_after_init:
    VLOG(1) << "closing tcp handle because of immediate failure after init";
    uv_close(getHandle(), on_close);

failed_tcp_init:
    insert(internal::Peer::LoopData::RETRY_TO_LISTEN);
    onError(rc);
}

void ::yajr::comms::internal::ListeningUnixPeer::retry() {

    if (destroying_) {

        LOG(INFO)
            << this
            << "Not retrying because of pending destroy"
        ;

        return;

    }

    int rc;
    if ((rc = uv_pipe_init(
                    getUvLoop(),
                    reinterpret_cast<uv_pipe_t *>(getHandle()),
                    0))) {
        LOG(WARNING)
            << "uv_pipe_init: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
            ;
        goto failed_unix_init;
    }

    up();

    if ((rc = uv_pipe_bind(reinterpret_cast<uv_pipe_t *>(getHandle()),
                    socketName_.c_str()))) {
        LOG(WARNING)
            << "uv_pipe_bind: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        status_ = Peer::kPS_FAILED_BINDING;
        goto failed_after_init;
    }

    if ((rc = uv_listen((uv_stream_t*) getHandle(), 1024,
                on_passive_connection))) {
        LOG(WARNING)
            << "uv_listen: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        status_ = Peer::kPS_FAILED_LISTENING;
        goto failed_after_init;
    }

    status_ = Peer::kPS_LISTENING;
    insert(internal::Peer::LoopData::LISTENING);
    connected_ = 1;

    return;

failed_after_init:
    VLOG(1)
        << "closing pipe handle because of immediate failure after init"
    ;
    uv_close(getHandle(), on_close);

failed_unix_init:
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
    ListeningPeer * listener = Peer::get<ListeningPeer>(server_handle);

    if (status) {
        LOG(ERROR)
            << listener
            << "on_passive_connection: ["
            << uv_err_name(status)
            << "] "
            << uv_strerror(status)
        ;

        /* there could also be legitimate reasons for this, but we still have to
         * encounter them
         */
        assert(0);

        return;
    }

    PassivePeer *peer;

    if (!(peer = listener->getNewPassive())){
        return;
    }

    int rc;
    if ((rc = uv_accept(server_handle, (uv_stream_t*) peer->getHandle()))) {
        VLOG(1)
            << "uv_accept: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        peer->onError(rc);
        uv_close(peer->getHandle(), on_close);
        return;
    }

    if (peer->unchoke()) {
        return;
    }

    peer->insert(internal::Peer::LoopData::ONLINE);

    /* kick the ball */
    peer->onConnect();

}

::yajr::comms::internal::PassivePeer *
::yajr::comms::internal::ListeningTcpPeer::getNewPassive() {

    ::yajr::comms::internal::PassivePeer * peer = NULL;
#if __cpp_exceptions || __EXCEPTIONS
    try {
#endif
        peer = new PassivePeer(
                getConnectionHandler(),
                getConnectionHandlerData(),
                getUvLoopSelector());
#if __cpp_exceptions || __EXCEPTIONS
    } catch(const std::bad_alloc&) {
    }
#endif

    if (!peer) {
        LOG(WARNING) << "out of memory, dropping new peer on the floor";
        return NULL;
    }

    int rc;
    if ((rc = peer->tcpInit())) {
        peer->onError(rc);
        peer->down();  // this is invoked with an intent to delete!
        return NULL;
    }

    return peer;
}

class PassiveUnixPeer : public PassivePeer {
public:
    explicit PassiveUnixPeer(
            ::yajr::Peer::StateChangeCb connectionHandler,
            void * data,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            PassivePeer(connectionHandler,
                        data,
                        uvLoopSelector)
        { }

    virtual int getPeerName(struct sockaddr* remoteAddress, int* len) const {
        *len = sizeof(struct sockaddr_un);

        remoteAddress->sa_family = AF_UNIX;
        struct sockaddr_un* addr = (struct sockaddr_un*)remoteAddress;
        size_t size = sizeof(addr->sun_path);
        memset(addr->sun_path, 0, sizeof(addr->sun_path));
        int rc =
            uv_pipe_getsockname(reinterpret_cast<uv_pipe_t const *>(getHandle()),
                                addr->sun_path,
                                &size);
        addr->sun_path[sizeof(addr->sun_path)-1] = '\0';
        return rc;
    }
};

::yajr::comms::internal::PassivePeer *
::yajr::comms::internal::ListeningUnixPeer::getNewPassive() {

    ::yajr::comms::internal::PassivePeer * peer = NULL;
#if __cpp_exceptions || __EXCEPTIONS
    try {
#endif
        peer = new PassiveUnixPeer(
                getConnectionHandler(),
                getConnectionHandlerData(),
                getUvLoopSelector());
#if __cpp_exceptions || __EXCEPTIONS
    } catch(const std::bad_alloc&) {
    }
#endif

    if (!peer) {
        LOG(WARNING) << "out of memory, dropping new peer on the floor";
        return NULL;
    }

    int rc;
    if ((rc = uv_pipe_init(
                    getUvLoop(),
                    reinterpret_cast<uv_pipe_t *>(peer->getHandle()),
                    0))) {
        LOG(WARNING)
            << "uv_pipe_init: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
            ;
        peer->onError(rc);
        peer->down();  // this is invoked with an intent to delete!
        return NULL;
    }

    return peer;

}

/*
     _   _ _   _ _ _ _            __                  _   _
    | | | | |_(_) (_) |_ _   _   / _|_   _ _ __   ___| |_(_) ___  _ __  ___
    | | | | __| | | | __| | | | | |_| | | | '_ \ / __| __| |/ _ \| '_ \/ __|
    | |_| | |_| | | | |_| |_| | |  _| |_| | | | | (__| |_| | (_) | | | \__ \
     \___/ \__|_|_|_|\__|\__, | |_|  \__,_|_| |_|\___|\__|_|\___/|_| |_|___/
                         |___/
*/

int ::yajr::comms::internal::ListeningTcpPeer::setAddrFromIpAndPort(
        const std::string& ip_address,
        uint16_t port) {
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
