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


#include <yajr/transport/PlainText.hpp>
#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <sys/uio.h>

namespace yajr {
    namespace transport {

using namespace yajr::comms::internal;

static TransportEngine< PlainText > PlainTextCookieCutterEngine(NULL);

TransportEngine< PlainText > & PlainText::getPlainTextTransport() {
    return PlainTextCookieCutterEngine;
}

template<>
int Cb< PlainText >::send_cb(CommunicationPeer const * peer) {

    VLOG(5)
        << peer
    ;

    assert(!peer->pendingBytes_);
    peer->pendingBytes_ = peer->s_.deque_.size();

    if (!peer->pendingBytes_) {
        /* great success! */
        VLOG(4)
            << "Nothing left to be sent!"
        ;

        return 0;
    }

    std::vector<iovec> iov =
        ::yajr::comms::internal::get_iovec(
                peer->s_.deque_.begin(),
                peer->s_.deque_.end()
        );

    assert (iov.size());

    return peer->writeIOV(iov);
}

template<>
void Cb< PlainText >::on_sent(CommunicationPeer const * peer) {

    VLOG(4)
        << peer
    ;

    peer->s_.deque_.erase(
            peer->s_.deque_.begin(),
            peer->s_.deque_.begin() + peer->pendingBytes_
    );

}

template<>
void Cb< PlainText >::alloc_cb(
        uv_handle_t * _
      , size_t size
      , uv_buf_t* buf
      ) {

    /* this is really up to us, looks like libuv always suggests 64kB anyway */
    size_t bufsize = (size > 4096) ? size : 4096;

    VLOG(5)
        << comms::internal::Peer::get<CommunicationPeer>(_)
        << " suggested size = "
        << size
        << " allocation size = "
        << bufsize;

    *buf = uv_buf_init((char*) malloc(size), size);

    return;
}

template<>
void Cb< PlainText >::on_read(
        uv_stream_t * h
      , ssize_t nread
      , uv_buf_t const * buf
      ) {

    CommunicationPeer * peer = comms::internal::Peer::get<CommunicationPeer>(h);

    VLOG(5)
        << peer
    ;

    if (!peer->connected_) {
        return;
    }

    if (nread < 0) {

        VLOG(2)
            << peer
            << " nread = "
            <<   nread
            << " ["
            << uv_err_name(nread)
            << "] "
            << uv_strerror(nread)
            << " => closing"
        ;

        peer->onDisconnect();
    }

    if (nread > 0) {

        VLOG(5)
            << peer
            << " nread "
            <<   nread
            << " into buffer of size "
            << buf->len
        ;

        if (peer->nullTermination) {
            peer->readBuffer(
                buf->base,
                nread,
                (buf->len > static_cast< size_t >(nread))
            );
        } else {
            peer->readBufNoNull(buf->base, nread);
        }
    }

    if (buf->base) {
        free(buf->base);
    }

}

template< >
TransportEngine< PlainText >::TransportEngine(PlainText * _ /* = NULL */)
    :
        Transport(
            &Cb< PlainText >::kCb
          , NULL
        )
    {
        assert(!_);
    }

} /* yajr::transport namespace */
} /* yajr namespace */

