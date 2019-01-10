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
#include <yajr/transport/engine.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

std::ostream& operator << (
        std::ostream& os,
        ::yajr::comms::internal::Peer const * p)
                                        __attribute__((no_instrument_function));
std::ostream& operator << (
        std::ostream& os,
        ::yajr::comms::internal::Peer const * p
    ) {
    os
        << "{"
        << reinterpret_cast<void const *>(p)
        << "}"
        << "["
        << p->uvRefCnt_
        << "];handle@"
        << reinterpret_cast<void const *>(p->getHandle())
        << ";HD:"
        << p->getHandle()->data
        << "]"
    ;

    return os;
}

CommunicationPeer * Peer::get(uv_write_t * r) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(r->data);
    return peer;
}

CommunicationPeer * Peer::get(uv_timer_t * h) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(h->data);
    return peer;
}

ActivePeer * Peer::get(uv_connect_t * r) {
    ActivePeer * peer = Peer::get<ActivePeer>(r->handle);
    return peer;
}

ActiveTcpPeer * Peer::get(uv_getaddrinfo_t * r) {
    ActiveTcpPeer * peer = static_cast<ActiveTcpPeer *>(r->data);
    return peer;
}

void Peer::up() {

    VLOG(2)
        << this
        << " refcnt: "
        << uvRefCnt_
        << " -> "
        << uvRefCnt_ + 1
    ;

    ++uvRefCnt_;
}

bool Peer::down() {

    VLOG(2)
        << this 
        << " refcnt: "
        << uvRefCnt_
        << " -> "
        << uvRefCnt_ - 1
    ;

    if (--uvRefCnt_) {
        return false;
    }

    VLOG(1)
        << "deleting "
        << this
    ;

    onDelete();

    delete this;

    return true;
}

void Peer::insert(Peer::LoopData::PeerState peerState) {

    VLOG(3)
        << this
        << " is being inserted in "
        << peerState
    ;

    Peer::LoopData::getPeerList(getUvLoop(), peerState)->push_back(*this);

}

void Peer::unlink() {

    VLOG(4)
        << this
        << " manually unlinking"
    ;

    SafeListBaseHook::unlink();
}

Peer::~Peer() {
    if (createFail_) {
        return;
    }

    VLOG(5)
        << "{"
        << static_cast< void * >(this)
        << "}"
        << " flags="
        << std::hex
        << getHandle()->flags
        << " UV_CLOSING "
        << (getHandle()->flags & 0x00001)
        << " UV_CLOSED "
        << (getHandle()->flags & 0x00002)
        << " UV_STREAM_READING "
        << (getHandle()->flags & 0x00004)
        << " UV_STREAM_SHUTTING "
        << (getHandle()->flags & 0x00008)
        << " UV_STREAM_SHUT "
        << (getHandle()->flags & 0x00010)
        << " UV_STREAM_READABLE "
        << (getHandle()->flags & 0x00020)
        << " UV_STREAM_WRITABLE "
        << (getHandle()->flags & 0x00040)
        << " UV_STREAM_BLOCKING "
        << (getHandle()->flags & 0x00080)
        << " UV_STREAM_READ_PARTIAL "
        << (getHandle()->flags & 0x00100)
        << " UV_STREAM_READ_EOF "
        << (getHandle()->flags & 0x00200)
        << " UV_TCP_NODELAY "
        << (getHandle()->flags & 0x00400)
        << " UV_TCP_KEEPALIVE "
        << (getHandle()->flags & 0x00800)
        << " UV_TCP_SINGLE_ACCEPT "
        << (getHandle()->flags & 0x01000)
        << " UV_HANDLE_IPV6 "
        << (getHandle()->flags & 0x10000)
    ;
    assert(!uvRefCnt_);
    assert(!uv_is_active(getHandle()));

    getLoopData()->down();
}

} // namespace internal
} // namespace comms
} // namespace yajr

