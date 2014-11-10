/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <uv.h>
#include <cstdlib>
#include <opflex/comms/comms-internal.hpp>
#include <opflex/logging/internal/logging.hpp>
#include <boost/scoped_ptr.hpp>
#include <opflex/rpc/rpc.hpp>

#include <cstdio>

namespace opflex { namespace comms {

using namespace opflex::comms::internal;

int initLoop(uv_loop_t * loop) {

    if (!(loop->data = new (std::nothrow)
                ::opflex::comms::internal::Peer::LoopData(loop))) {
        LOG(WARNING) <<
            ": out of memory, cannot instantiate uv_loop LoopData";
        return UV_ENOMEM;
    }

    return 0;

}

void internal::Peer::LoopData::onIdleLoop() {

    uint64_t now = uv_now(idle_.loop);

    peers[TO_RESOLVE]
        .clear_and_dispose(RetryPeer());

    peers[TO_LISTEN]
        .clear_and_dispose(RetryPeer());

    if (now - lastRun_ < 30000) {
        return;
    }

    LOG(DEBUG);

    lastRun_ = now;

    /* retry all listeners */
    peers[RETRY_TO_LISTEN]
        .clear_and_dispose(RetryPeer());

    /* retry just the first active peer in the queue */
    peers[RETRY_TO_CONNECT]
        .erase_and_dispose(peers[RETRY_TO_CONNECT].begin(), RetryPeer());

}

void internal::Peer::LoopData::onIdleLoop(uv_idle_t * h) {

    static_cast< ::opflex::comms::internal::Peer::LoopData *>(h->data)
        ->onIdleLoop();

}

void internal::Peer::LoopData::fini(uv_handle_t * h) {
    LOG(INFO);

    static_cast< ::opflex::comms::internal::Peer::LoopData *>(h->data)->down();
}

void internal::Peer::LoopData::destroy() {
    LOG(INFO);

    uv_idle_stop(&idle_);
    uv_close((uv_handle_t*)&idle_, &fini);

    assert(idle_.data == this);

    destroying_ = 1;

    for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
        Peer::LoopData::getPeerList(uv_default_loop(),
                    Peer::LoopData::PeerState(i))
            ->clear_and_dispose(PeerDisposer());

    }
}

void finiLoop(uv_loop_t * loop) {
    LOG(INFO);

    static_cast< ::opflex::comms::internal::Peer::LoopData *>(loop->data)->destroy();

}



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

void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf) {

    LOG(DEBUG);

    *buf = uv_buf_init((char*) malloc(size + 1), size);

    return;
}

void on_close(uv_handle_t * h) {

    CommunicationPeer * peer = Peer::get<CommunicationPeer>(h);

    LOG(DEBUG) << peer;

    LOG(DEBUG) << peer << " down() for an on_close()";
    peer->down();

    return;
}

void on_write(uv_write_t *req, int status) {

    LOG(DEBUG);

    if (status == UV_ECANCELED || status == UV_ECONNRESET) {
        LOG(INFO) << "[" << uv_err_name(status) << "] " << uv_strerror(status);
        return;  /* Handle has been closed. */
    }

    /* see here if more output can be dequeued */

    CommunicationPeer * peer = Peer::get(req);

    peer->onWrite();

    return;
}

void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf)
{

    CommunicationPeer * peer = Peer::get<CommunicationPeer>(h);

    LOG(DEBUG) << peer;

    if (nread < 0) {
        LOG(INFO) << "nread = " << nread << " [" << uv_err_name(nread) <<
            "] " << uv_strerror(nread) << " => closing";
        peer->onDisconnect();
    }

    if (nread > 0) {

        LOG(INFO) << peer
            << " read " << nread << " into buffer of size " << buf->len;

        char * buffer = buf->base;
        size_t chunk_size;
        buffer[nread++]='\0';

        while (--nread > 0) {
            chunk_size = peer->readChunk(buffer);
            nread -= chunk_size++;

            LOG(DEBUG) << "nread=" << nread << " chunk_size=" << chunk_size;

            if(nread) {

                LOG(DEBUG) << "got: " << chunk_size;

                buffer += chunk_size;

                boost::scoped_ptr<opflex::rpc::InboundMessage> msg(
                        peer->parseFrame()
                    );

                if (!msg) {
                    LOG(ERROR) << "skipping inbound message";
                    continue;
                }

                msg->process();

            }
        }
    }

    if (buf->base) {
        free(buf->base);
    }

}

} /* opflex::comms::internal namespace */

}} /* opflex::comms and opflex namespaces */
