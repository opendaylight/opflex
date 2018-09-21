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

#ifdef EXTRA_CHECKS
# include <unistd.h>
#endif

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
#ifdef EXTRA_CHECKS
        << getpid()
#endif
        << "{"
        << reinterpret_cast<void const *>(p)
        << "}"
#ifdef EXTRA_CHECKS
        << p->peerType()
#endif
        << "["
        << p->uvRefCnt_
        << "];handle@"
        << reinterpret_cast<void const *>(p->getHandle())
        << ";HD:"
        << p->getHandle()->data
        << "];timer@"
//        << reinterpret_cast<void const *>(&p->keepAliveTimer_)
//        << ";TD:"
//        << p->keepAliveTimer_.data
    ;
#ifdef EXTRA_CHECKS
    if ('L' != *p->peerType()) {
        ::yajr::comms::internal::CommunicationPeer const * cP =
            static_cast< ::yajr::comms::internal::CommunicationPeer const * >(p);
        os
            << ";|"
            << cP->s_.deque_.size()
            << "->|->"
            << cP->pendingBytes_
            << "|"
            << cP->getPendingBytes()
            << "->"
        ;
    }
    os
        << " PIDs"
    ;
    for (std::vector<Peer::PidSequence>::iterator it = p->pidSeq_.begin()
            ;
        it != p->pidSeq_.end()
            ;
        ++it) {
        os
            << ":"
            << it->count
            << "x"
            << it->pid
        ;
    }
#endif

    return os;
}

CommunicationPeer * Peer::get(uv_write_t * r) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(r->data);

    VLOG(7)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

CommunicationPeer * Peer::get(uv_timer_t * h) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(h->data);

    VLOG(7)
#ifdef EXTRA_CHECKS
        << getpid()
#endif
        << "{"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

ActivePeer * Peer::get(uv_connect_t * r) {

    ActivePeer * peer = Peer::get<ActivePeer>(r->handle);

    VLOG(7)
#ifdef EXTRA_CHECKS
        << getpid()
#endif
        << "{"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

ActiveTcpPeer * Peer::get(uv_getaddrinfo_t * r) {
    ActiveTcpPeer * peer = static_cast<ActiveTcpPeer *>(r->data);

    VLOG(7)
#ifdef EXTRA_CHECKS
        << getpid()
#endif
        << "{"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    assert(peer->__checkInvariants());

    return peer;
}

bool Peer::__checkInvariants()
#ifdef EXTRA_CHECKS
    const
{
    VLOG(7)
        << this
    ;
    appendPID();
#else
{
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
        << " ("
        << internal::Peer::LoopData::getPSStr(peerState)
        << ")"
    ;

    Peer::LoopData::getPeerList(getUvLoop(), peerState)->push_back(*this);

}

void Peer::unlink() {

    VLOG(4)
        << this
        << " manually unlinking"
    ;

    SafeListBaseHook::unlink();

    return;

}

Peer::~Peer() {

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
    assert(!uv_is_active(reinterpret_cast< uv_handle_t * >(getHandle())));

    getLoopData()->down();

#ifdef COMMS_DEBUG_OBJECT_COUNT
    --counter;
#endif
}

char const * internal::Peer::LoopData::getPSStr(
        internal::Peer::LoopData::PeerState s
    ) {
#ifdef EXTRA_CHECKS
    if (s >= TOTAL_STATES) {
        return "<too_large>";
    }

    return kPSStr[s];
#else
    return "";
#endif
}

#ifdef EXTRA_CHECKS
char const * const internal::Peer::LoopData::kPSStr[] = {
#define XX(_, s) s,
    PEER_STATE_MAP(XX)
#undef XX
};

void Peer::appendPID() const {
    pid_t pid = getpid();

    if (!pidSeq_.size() || pidSeq_.back().pid != pid) {
        pidSeq_.push_back((PidSequence){pid, 1});
    } else {
        ++pidSeq_.back().count;
    }
}
#endif

} // namespace internal
} // namespace comms
} // namespace yajr

