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

#include <opflex/logging/internal/logging.hpp>

#include <uv.h>

namespace {

    void prepareAgainCB(uv_timer_t *) { VLOG(6); }

}

namespace yajr {
    namespace comms {
        namespace internal {

std::ostream& operator << (
        std::ostream& os,
        Peer::LoopData const * lD);


void internal::Peer::LoopData::onPrepareLoop() {

    if (destroying_ && !refCount_) {

        CountHandle countHandle = { this, 0 };

        uv_walk(prepare_.loop, walkAndCountHandlesCb, &countHandle);

        if (countHandle.counter) {
            LOG(INFO) << "Still waiting on " << countHandle.counter << " handles";
            return;
        }

        LOG(INFO) << this << " Stopping and closing loop watcher";
        uv_prepare_stop(&prepare_);
        uv_close((uv_handle_t*)&prepare_, &fini);

        assert(prepare_.data == this);

        return;
    }

    uint64_t now = uv_now(prepare_.loop);

    peers[TO_LISTEN]
        .clear_and_dispose(RetryPeer());

    peers[TO_RESOLVE]
        .clear_and_dispose(RetryPeer());

    if (now - lastRun_ < 750) {
        goto prepared;
    }

    if (peers[RETRY_TO_CONNECT].begin() !=
        peers[RETRY_TO_CONNECT].end()) {

        LOG(INFO) << "retrying first RETRY_TO_CONNECT peer";

        /* retry just the first active peer in the queue */
        peers[RETRY_TO_CONNECT]
            .erase_and_dispose(peers[RETRY_TO_CONNECT].begin(), RetryPeer());

        uv_timer_stop(&prepareAgain_);
        /* if there are more, we will uv_timer_start() down below */
    }

    if ((now - lastRun_ > 15000) && !peers[RETRY_TO_LISTEN].empty()) {

        LOG(INFO)
            << "retrying all "
            << peers[RETRY_TO_LISTEN].size()
            << "RETRY_TO_LISTEN peers"
        ;

        /* retry all listeners */
        peers[RETRY_TO_LISTEN]
            .clear_and_dispose(RetryPeer());
    }

    lastRun_ = now;

prepared:
    if (peers[RETRY_TO_CONNECT].begin() !=
        peers[RETRY_TO_CONNECT].end()) {
        /* We need to make sure we unblock */

        if (!uv_is_active((uv_handle_t *)&prepareAgain_)) {
            VLOG(4)
                << " Starting prepareAgain_ @"
                << reinterpret_cast<void *>(&prepareAgain_)
            ;
        } // else we are just pushing it out in time :)
        uv_timer_start(&prepareAgain_, prepareAgainCB, 1250, 0);
    }
}

void internal::Peer::LoopData::onPrepareLoop(uv_prepare_t * h) {
    static_cast< ::yajr::comms::internal::Peer::LoopData *>(h->data)
        ->onPrepareLoop();
}

void internal::Peer::LoopData::fini(uv_handle_t * h) {
    LOG(INFO);
    delete static_cast< ::yajr::comms::internal::Peer::LoopData *>(h->data);
}

void internal::Peer::LoopData::destroy(bool now) {
    LOG(INFO);
    assert(!destroying_);
    if (destroying_) {
        LOG(WARNING) << this << " Double destroy() detected";
        return;
    }

    destroying_ = true;
    down();

    for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
        peers[Peer::LoopData::PeerState(i)]
            .clear_and_dispose(PeerDisposer(now));
    }
}

std::ostream& operator << (std::ostream& os, Peer::LoopData const * lD) {
    return os
        << "{"
        << reinterpret_cast<void const *>(lD)
        << "}"
        << "["
        << lD->refCount_
        << "]"
    ;
}

void internal::Peer::LoopData::walkAndCloseHandlesCb(
        uv_handle_t* h,
        void* opaqueCloseHandle) {

    CloseHandle const * closeHandle = static_cast<  CloseHandle const *  >(
            opaqueCloseHandle
        );

    if (uv_is_closing(h) ||
            reinterpret_cast<uv_handle_t const *>(&closeHandle->loopData->prepare_)
            ==
            const_cast<uv_handle_t const *>(h)) {
        return;
    }

    LOG(INFO)
        << closeHandle->loopData
        << " issuing uv_close() for handle of type "
        << getUvHandleType(h)
        << " @"
        << reinterpret_cast<void const *>(h);

    uv_close(h, closeHandle->closeCb);

}


void internal::Peer::LoopData::walkAndCountHandlesCb(
        uv_handle_t* h,
        void* opaqueCountHandle) {

    CountHandle * countHandle = static_cast<  CountHandle *  >(
            opaqueCountHandle
        );

    if (
            reinterpret_cast<uv_handle_t const *>(&countHandle->loopData->prepare_)
            ==
            const_cast<uv_handle_t const *>(h)) {
        return;
    }

    ++countHandle->counter;

    LOG(INFO)
        << countHandle->loopData
        << " still waiting on pending handle of type "
        << getUvHandleType(h)
        << " @"
        << reinterpret_cast<void const *>(h)
        << " which is "
        << (uv_is_closing(h) ? "" : "not ")
        << "closing"
        ;

    /** we assert() here, but everything will be cleaned up in production code */
    assert(uv_is_closing(h));

}

void Peer::LoopData::RetryPeer::operator () (Peer *peer)
{
    peer->retry();
}

void Peer::LoopData::PeerDeleter::operator () (Peer *peer)
{
    VLOG(1) << peer << " deleting abruptedly";
    assert(!"peers should never get deleted this way");
    delete peer;
}

void Peer::LoopData::up() {
    VLOG(2)
        << this
        << " LoopRefCnt: "
        << refCount_
        << " -> "
        << refCount_ + 1
    ;

    ++refCount_;
}

void Peer::LoopData::down() {

    assert(refCount_);

    --refCount_;

    if (destroying_ && !refCount_) {
        CloseHandle closeHandle = { this, NULL };
        uv_walk(prepare_.loop, walkAndCloseHandlesCb, &closeHandle);
    }
}

Peer::LoopData::~LoopData() {

    VLOG(1) << this << " is being deleted" ;

    for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
        assert(peers[Peer::LoopData::PeerState(i)].empty());
        /* delete all peers for final builds */
        peers[Peer::LoopData::PeerState(i)]
            .clear_and_dispose(PeerDeleter());
    }
}

void Peer::LoopData::PeerDisposer::operator () (Peer *peer)
{
    LOG(INFO) << peer << " destroy() because this communication thread is shutting down";
    peer->destroy(now_);
}

Peer::LoopData::PeerDisposer::PeerDisposer(bool now)
    :
        now_(now)
    {}

} /* yajr::comms::internal namespace */
} /* yajr::comms namespace */
} /* yajr namespace */

