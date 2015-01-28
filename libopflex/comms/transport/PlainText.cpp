/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <yajr/transport/PlainText.hpp>
#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <sys/uio.h>

#include <iovec-utils.hh>

namespace yajr {
    namespace transport {

using namespace yajr::comms::internal;

static TransportEngine< PlainText > PlainTextCookieCutterEngine(NULL);

TransportEngine< PlainText > & PlainText::getPlainTextTransport() {
    return PlainTextCookieCutterEngine;
}

template<>
int Cb< PlainText >::send_cb(CommunicationPeer const * peer) {

    LOG(DEBUG) << peer;

    assert(!peer->pendingBytes_);
    peer->pendingBytes_ = peer->s_.deque_.size();

    if (!peer->pendingBytes_) {
        /* great success! */
        LOG(DEBUG) << "Nothing left to be sent!";

        return 0;
    }

    std::vector<iovec> iov =
        more::get_iovec(
                peer->s_.deque_.begin(),
                peer->s_.deque_.end()
        );

    assert (iov.size());

    return peer->writeIOV(iov);
}

template<>
void Cb< PlainText >::on_sent(CommunicationPeer const * peer) {

    LOG(DEBUG) << peer;

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

    LOG(DEBUG)
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

    LOG(DEBUG) << peer;

    if (nread < 0) {

        LOG(DEBUG)
            << "nread = "
            << nread
            << " ["
            << uv_err_name(nread)
            << "] "
            << uv_strerror(nread)
            << " => closing"
        ;

        peer->onDisconnect();
    }

    if (nread > 0) {

        LOG(DEBUG)
            << peer
            << " read "
            << nread
            << " into buffer of size "
            << buf->len
        ;

        peer->readBuffer(buf->base, nread, (buf->len > nread));

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

