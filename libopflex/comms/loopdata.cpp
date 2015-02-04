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

#include <yajr/internal/walkAndDumpHandlesCb.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <uv.h>

namespace {

    void prepareAgainCB(uv_timer_t *) { LOG(DEBUG); }

}

namespace yajr {
    namespace comms {
        namespace internal {

#ifdef COMMS_DEBUG_OBJECT_COUNT
::boost::atomic<size_t> Peer::LoopData::counter(0);
#endif

std::ostream& operator << (
        std::ostream& os,
        Peer::LoopData const * lD)
                                        __attribute__((no_instrument_function));


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

    peers[TO_RESOLVE]
        .clear_and_dispose(RetryPeer());

    peers[TO_LISTEN]
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

        LOG(DEBUG)
            << " Stopping prepareAgain_ @"
            << reinterpret_cast<void *>(&prepareAgain_)
        ;

        uv_timer_stop(&prepareAgain_);
        /* if there are more, we will uv_timer_start() down below */
    }

    if (now - lastRun_ > 15000) {

        LOG(INFO) << "retrying all RETRY_TO_LISTEN peers";

        /* retry all listeners */
        peers[RETRY_TO_LISTEN]
            .clear_and_dispose(RetryPeer());
    }

    lastRun_ = now;

prepared:
    uv_walk(prepare_.loop, walkAndDumpHandlesCb<DEBUG>, this);

    if (peers[RETRY_TO_CONNECT].begin() !=
        peers[RETRY_TO_CONNECT].end()) {
        /* We need to make sure we unblock */

        if (!uv_is_active((uv_handle_t *)&prepareAgain_)) {
            LOG(DEBUG)
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

    assert(prepare_.data == this);

    assert(!destroying_);
    if (destroying_) {
        LOG(WARNING)
            << this
            << " Double destroy() detected"
        ;

        return;
    }

    destroying_ = 1;
    down();

    for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
        peers[Peer::LoopData::PeerState(i)]
            .clear_and_dispose(PeerDisposer(now));

    }
}

std::ostream& operator<< (std::ostream& os, Peer::LoopData const * lD) {
    return os << "{" << reinterpret_cast<void const *>(lD) << "}"
        << "[" << lD->refCount_ << "]";
}

void internal::Peer::LoopData::walkAndCloseHandlesCb(
        uv_handle_t* h,
        void* opaqueCloseHandle) {

    CloseHandle const * closeHandle = static_cast<  CloseHandle const *  >(
            opaqueCloseHandle
        );

    char const * type;

    switch (h->type) {
#define X(uc, lc)                               \
        case UV_##uc: type = #lc;               \
            break;
      UV_HANDLE_TYPE_MAP(X)
#undef X
      default: type = "<unknown>";
    }

    if (uv_is_closing(h) ||
            reinterpret_cast<uv_handle_t const *>(&closeHandle->loopData->prepare_)
            ==
            const_cast<uv_handle_t const *>(h)) {
        return;
    }

    LOG(INFO)
        << closeHandle->loopData
        << " issuing uv_close() for handle of type "
        << type
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

    char const * type;

    switch (h->type) {
#define X(uc, lc)                               \
        case UV_##uc: type = #lc;               \
            break;
      UV_HANDLE_TYPE_MAP(X)
#undef X
      default: type = "<unknown>";
    }

    LOG(INFO)
        << countHandle->loopData
        << " still waiting on pending handle of type "
        << type
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
    LOG(DEBUG) << peer << " retry";
    peer->retry();
}

void Peer::LoopData::up() {
    LOG(DEBUG) << this
        << " LoopRefCnt: " << refCount_ << " -> " << refCount_ + 1;

    ++refCount_;
}

void Peer::LoopData::down() {
    LOG(DEBUG) << " Down() on Loop";
    LOG(DEBUG) << this
        << " LoopRefCnt: " << refCount_ << " -> " << refCount_ - 1;

    assert(refCount_);

    --refCount_;

    if (destroying_ && !refCount_) {
        LOG(INFO) << this << " walking uv_loop before stopping it";
        uv_walk(prepare_.loop, walkAndDumpHandlesCb< ERROR >, this);

        CloseHandle closeHandle = { this, NULL };

        uv_walk(prepare_.loop, walkAndCloseHandlesCb, &closeHandle);

    }
}

Peer::LoopData::~LoopData() {
    LOG(DEBUG) << "Delete on Loop";
    LOG(DEBUG) << this << " is being deleted";
#ifdef COMMS_DEBUG_OBJECT_COUNT
    --counter;
#endif
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

void Peer::up() {

    LOG(DEBUG) << this
        << " refcnt: " << uvRefCnt_ << " -> " << uvRefCnt_ + 1;

    ++uvRefCnt_;
}

} /* yajr::comms::internal namespace */
} /* yajr::comms namespace */
} /* yajr namespace */

