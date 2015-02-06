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

#include <yajr/internal/walkAndDumpHandlesCb.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

#ifdef COMMS_DEBUG_OBJECT_COUNT
::boost::atomic<size_t> Peer::counter(0);
#endif

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
#ifndef NDEBUG
        << p->peerType()
#endif
        << "["
        << p->uvRefCnt_
        << "];handle@"
        << reinterpret_cast<void const *>(&p->handle_);
    ;
#ifndef NDEBUG
    if ('L' != *p->peerType()) {
        ::yajr::comms::internal::CommunicationPeer const * cP =
            static_cast< ::yajr::comms::internal::CommunicationPeer const * >(p);
        os
            << ";|"
            << cP->s_.deque_.size()
            << "->|->"
            << cP->pendingBytes_
            << "|"
        ;
    }
#endif

    return os;
}

CommunicationPeer * Peer::get(uv_write_t * r) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(r->data);

    VLOG(5)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

CommunicationPeer * Peer::get(uv_timer_t * h) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(h->data);

    VLOG(5)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

ActivePeer * Peer::get(uv_connect_t * r) {

    ActivePeer * peer = Peer::get<ActivePeer>(r->handle);

    VLOG(5)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

ActivePeer * Peer::get(uv_getaddrinfo_t * r) {
    ActivePeer * peer = static_cast<ActivePeer *>(r->data);

    VLOG(5)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

bool Peer::__checkInvariants()
#ifdef NDEBUG
{
#else
    const
{
    VLOG(5)
        << this
        << " ALWAYS = true"
    ;
#endif
    return true;
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

void Peer::down() {

    VLOG(2)
        << this 
        << " refcnt: "
        << uvRefCnt_
        << " -> "
        << uvRefCnt_ - 1
    ;

    if (--uvRefCnt_) {
        return;
    }

    VLOG(1)
        << "deleting "
        << this
    ;

    onDelete();

    delete this;
}

void Peer::insert(Peer::LoopData::PeerState peerState) {
    VLOG(2)
        << this
        << " is being inserted in "
        << peerState
    ;

    Peer::LoopData::getPeerList(getUvLoop(), peerState)->push_back(*this);
}

void Peer::unlink() {
    VLOG(2)
        << this
        << " manually unlinking"
    ;

    SafeListBaseHook::unlink();

    return;
}

} // namespace internal
} // namespace comms
} // namespace yajr

