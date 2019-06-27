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

#include <uv.h>

int ::yajr::initLoop(uv_loop_t * loop) {

    if (!(loop->data = new (std::nothrow)
                ::yajr::comms::internal::Peer::LoopData(loop))) {
        LOG(WARNING)
            << ": out of memory, cannot instantiate uv_loop LoopData"
        ;
        return UV_ENOMEM;
    }

    return 0;

}

void ::yajr::finiLoop(uv_loop_t * loop) {
    LOG(INFO);

    static_cast< ::yajr::comms::internal::Peer::LoopData *>(loop->data)->destroy();

}


namespace yajr {
    namespace comms {

using namespace yajr::comms::internal;



/*
                    ____
                   / ___|___  _ __ ___  _ __ ___   ___  _ __
                  | |   / _ \| '_ ` _ \| '_ ` _ \ / _ \| '_ \
                  | |__| (_) | | | | | | | | | | | (_) | | | |
                   \____\___/|_| |_| |_|_| |_| |_|\___/|_| |_|

                     ____      _ _ _                _
                    / ___|__ _| | | |__   __ _  ___| | _____
                   | |   / _` | | | '_ \ / _` |/ __| |/ / __|
                   | |__| (_| | | | |_) | (_| | (__|   <\__ \
                    \____\__,_|_|_|_.__/ \__,_|\___|_|\_\___/

                                                              (Common Callbacks)
*/

namespace internal {

char const * getUvHandleType(uv_handle_t * h) {

    char const * type;

    switch (h->type) {
#define X(uc, lc)                               \
        case UV_##uc: type = #lc;               \
            break;
      UV_HANDLE_TYPE_MAP(X)
#undef X
      default: type = "<unknown>";
    }

    return type;

}

char const * getUvHandleField(uv_handle_t * h, internal::Peer * peer) {

    char const * hType = "???";

    if (h == reinterpret_cast< uv_handle_t * >(&peer->keepAliveTimer_)) {
        hType = "keepAliveTimer";
    } else {
        if (h == peer->getHandle()) {
            hType = "TCP";
        }
    }

    return hType;

}

void on_close(uv_handle_t * h) {

    if (!h) {
        VLOG(ERROR)
            << "NULL handle"
        ;
        return;
    }

    CommunicationPeer * peer = Peer::get<CommunicationPeer>(h);

    VLOG(1)
        << peer
        << " down() for an on_close("
        << static_cast< void * >(h)
        <<") "
        << getUvHandleField(h, peer)
        << " handle of type "
        << getUvHandleType(h)
    ;

    peer->choked_ = 1;

    peer->down();
}

void on_write(uv_write_t *req, int status) {

    VLOG(5);

    CommunicationPeer * peer = Peer::get(req);

    peer->down();

    if (status == UV_ECANCELED || status == UV_ECONNRESET) {
        LOG(INFO)
            << "["
            << uv_err_name(status)
            << "] "
            << uv_strerror(status)
        ;
        return;  /* Handle has been closed. */
    }

    /* see here if more output can be dequeued */

    peer->onWrite();
}

} /* yajr::comms::internal namespace */

} /* yajr::comms namespace */
} /* yajr namespace */

#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{

void throw_exception(std::exception const & e) {
    assert(0);

    LOG(FATAL)
        << e.what()
    ;

    exit(127);
}

} /* boost namespace */

#endif

