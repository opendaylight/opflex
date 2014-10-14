/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <uv.h>
#include <tricks/compiler.h>
#include <cstdlib>
#include <opflex/comms/comms-internal.hpp>
#include <opflex/logging/internal/logging.hpp>
#include <boost/scoped_ptr.hpp>
#include <opflex/rpc/rpc.hpp>

#include <cstdio>

namespace opflex { namespace comms {

using namespace opflex::comms::internal;

int initCommunicationLoop(uv_loop_t * loop) {

    if (!(loop->data = new (std::nothrow) rapidjson::MemoryPoolAllocator<>())) {
        LOG(WARNING) <<
            ": out of memory, cannot instantiate uv_loop rapidjson allocator";
        return UV_ENOMEM;
    }

    return 0;

}

void finiCommunicationLoop(uv_loop_t * loop) {

    delete static_cast<rapidjson::MemoryPoolAllocator<>*>(loop->data);

}

/*
        ____                  ____        _        _
       |  _ \ ___  ___ _ __  |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
       | |_) / _ \/ _ \ '__| | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
       |  __/  __/  __/ |    | |_| | (_| | || (_| | |_) | (_| \__ \  __/
       |_|   \___|\___|_|    |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|

                                                                 (Peer Database)
*/

namespace internal {

peer_db_t peers;

void __comms_init_peer_db() __attribute__((constructor));

void __comms_init_peer_db(){

    LOG(DEBUG);

    STATIC_ASSERT(!(sizeof(peer_db_t)%sizeof(d_intr_head_t)),
          Peer_DB_looks_different_than_usual);

    for(size_t i = (sizeof(peer_db_t)/sizeof(d_intr_head_t)); i-->0;) {
        d_intr_list_init(&peers.__all_doubly_linked_lists[i]);
    }
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

void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf) {

    LOG(DEBUG);

    *buf = uv_buf_init((char*) malloc(size + 1), size);

    return;
}

void on_close(uv_handle_t * h) {

    CommunicationPeer * peer = Peer::get<CommunicationPeer>(h);

    LOG(DEBUG) << peer;

    peer->stopKeepAlive();  // OK to invoke even if Keep-Alive was never started

    peer->down();  // will be invoked once for active, twice for passive

    d_intr_list_unlink(&peer->peer_hook);
    if (!peer->passive) {
        LOG(DEBUG) << "active => retry queue";
        /* we should attempt to reconnect later */
        d_intr_list_insert(&peer->peer_hook, &peers.retry);
    } else {
        LOG(DEBUG) << "passive => drop";
        /* whoever it was, hopefully will reconnect again */
        peer->down();  // this is invoked with an intent to delete!
    }

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

    peer->write(); /* kick the can */

    return;
}

void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf)
{

    LOG(DEBUG);

    if (nread < 0) {
        LOG(INFO) << "nread = " << nread << " [" << uv_err_name(nread) <<
            "] " << uv_strerror(nread) << " => closing";
        uv_close((uv_handle_t*) h, on_close);
    }

    if (nread > 0) {

        LOG(INFO) << "read " << nread << " into buffer of size " << buf->len;

        CommunicationPeer * peer = Peer::get<CommunicationPeer>(h);

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
