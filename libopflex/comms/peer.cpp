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

#include <opflex/yajr/internal/comms.hpp>
#include <opflex/yajr/transport/engine.hpp>
#include <opflex/logging/internal/logging.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

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

    VLOG(7)
        << this
        << " refcnt: "
        << uvRefCnt_
        << " -> "
        << uvRefCnt_ + 1
    ;

    ++uvRefCnt_;
}

bool Peer::down() {

    VLOG(7)
        << this 
        << " refcnt: "
        << uvRefCnt_
        << " -> "
        << uvRefCnt_ - 1
    ;

    if (--uvRefCnt_) {
        return false;
    }

    onDelete();

    delete this;

    return true;
}

uv_mutex_t Peer::LoopData::peerMutex{};

void Peer::insert(Peer::LoopData::PeerState peerState) {

    VLOG(3)
        << this
        << " is being inserted in "
        << peerState
    ;

    Peer::LoopData::addPeer(getUvLoop(), peerState, this);
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

    assert(!uvRefCnt_);
    assert(!uv_is_active(getHandle()));

    getLoopData()->down();
}

} // namespace internal
} // namespace comms
} // namespace yajr

