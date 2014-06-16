/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <uv.h>
#include <noiro/adt/intrusive_dlist.h>
#include <noiro/tricks/compiler.h>
#include <stdlib.h>
#include <noiro/debug_with_levels.h>
#include <noiro/policy-agent/comms-internal.h>

#include <stdio.h>
/*
        ____                  ____        _        _
       |  _ \ ___  ___ _ __  |  _ \  __ _| |_ __ _| |__   __ _ ___  ___
       | |_) / _ \/ _ \ '__| | | | |/ _` | __/ _` | '_ \ / _` / __|/ _ \
       |  __/  __/  __/ |    | |_| | (_| | || (_| | |_) | (_| \__ \  __/
       |_|   \___|\___|_|    |____/ \__,_|\__\__,_|_.__/ \__,_|___/\___|

                                                                 (Peer Database)
*/

peer_db_t peers;

void __comms_init_peer_db() __attribute__((constructor));

void __comms_init_peer_db(){
  STATIC_ASSERT(!(sizeof(peer_db_t)%sizeof(d_intr_head_t)),
          Peer_DB_looks_different_than_usual);

  for(ssize_t i = (sizeof(peer_db_t)/sizeof(d_intr_head_t)); i>=0; --i) {
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

    DBUG_ENTER(__func__);

    *buf = uv_buf_init((char*) malloc(size), size);

    DBUG_VOID_RETURN;
}

void on_close(uv_handle_t * h) {

    DBUG_ENTER(__func__);

    peer_t * peer = PEER_FROM_HANDLE(h);

    d_intr_list_unlink(&peer->peer_hook);
    if (!peer->passive) {
        /* we should attempt to reconnect later */
        d_intr_list_insert(&peer->peer_hook, &peers.retry);
    } else {
        /* whoever it was, hopefully will reconnect again */
        free(peer);
    }

    DBUG_VOID_RETURN;
}

void on_write(uv_write_t *req, int status) {

    DBUG_ENTER(__func__);

    if (status == UV_ECANCELED || status == UV_ECONNRESET) {
        DBUG_PRINT(kDBG_I, ("%s: [%s] %s", __func__, uv_err_name(status),
                    uv_strerror(status)));
        DBUG_VOID_RETURN;  /* Handle has been closed. */
    }

    /* FIXME: once we have the ring-buffers in place,
     * see here if more output can be dequeued

    peer_t * peer = PEER_FROM_WRQ(req);
    ...

    */

    DBUG_VOID_RETURN;
}

void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf)
{

    DBUG_ENTER(__func__);

    peer_t *peer = PEER_FROM_HANDLE(h);

    DBUG_PRINT(kDBG_I, ("%s: read %d", __func__, nread));
    if (nread > 0) {
        /* FIXME: do the real processing via a parser */
        do_the_needful_for_identity(peer, nread, buf->base);
    }

    if (buf->base) {
        free(buf->base);
    }

    if (nread < 0) {
        DBUG_PRINT(kDBG_I, ("nread = %d [%s] %s => closing", nread,
                uv_err_name(nread), uv_strerror(nread)));
        uv_close((uv_handle_t*) h, on_close);

        DBUG_VOID_RETURN;
    }
}

