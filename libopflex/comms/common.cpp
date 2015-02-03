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


#include <yajr/rpc/rpc.hpp>
#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>
#include <boost/scoped_ptr.hpp>

#include <uv.h>

int ::yajr::initLoop(uv_loop_t * loop) {

    if (!(loop->data = new (std::nothrow)
                ::yajr::comms::internal::Peer::LoopData(loop))) {
        LOG(WARNING) <<
            ": out of memory, cannot instantiate uv_loop LoopData";
        return UV_ENOMEM;
    }

    return 0;

}

void ::yajr::finiLoop(uv_loop_t * loop) {
    LOG(DEBUG);

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

void on_close(uv_handle_t * h) {

    CommunicationPeer * peer = Peer::get<CommunicationPeer>(h);

    LOG(DEBUG) << peer << " down() for an on_close()";
    peer->down();

    peer->choked_ = 1;

    return;
}

void on_write(uv_write_t *req, int status) {

    LOG(DEBUG4);

    if (status == UV_ECANCELED || status == UV_ECONNRESET) {
        LOG(INFO) << "[" << uv_err_name(status) << "] " << uv_strerror(status);
        return;  /* Handle has been closed. */
    }

    /* see here if more output can be dequeued */

    CommunicationPeer * peer = Peer::get(req);

    peer->onWrite();

    return;
}

} /* yajr::comms::internal namespace */

} /* yajr::comms namespace */
} /* yajr namespace */

#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{

void throw_exception(std::exception const & e) {
    assert(0);

    LOG(FATAL) << e.what();

    exit(127);
}

} /* boost namespace */

#endif

