/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <yajr/rpc/internal/json_stream_wrappers.hpp>
#include <yajr/rpc/message_factory.inl.hpp>
#include <yajr/internal/comms.hpp>
#include <yajr/transport/engine.hpp>

#include <rapidjson/error/en.h>

namespace yajr { namespace comms { namespace internal {

#ifdef COMMS_DEBUG_OBJECT_COUNT
::boost::atomic<size_t> Peer::counter(0);
::boost::atomic<size_t> Peer::LoopData::counter(0);
::boost::atomic<size_t> CommunicationPeer::counter(0);
::boost::atomic<size_t> ActivePeer::counter(0);
::boost::atomic<size_t> PassivePeer::counter(0);
::boost::atomic<size_t> ListeningPeer::counter(0);
#endif

std::ostream& operator<< (
        std::ostream& os,
        ::yajr::comms::internal::Peer const * p)
                                        __attribute__((no_instrument_function));
std::ostream& operator<< (
        std::ostream& os,
        Peer::LoopData const * lD)
                                        __attribute__((no_instrument_function));

std::ostream& operator<< (
        std::ostream& os,
        ::yajr::comms::internal::Peer const * p
    ) {
    os
        << "{" << reinterpret_cast<void const *>(p) << "}"
#ifndef NDEBUG
        << p->peerType()
#endif
        << "[" << p->uvRefCnt_ << "];handle@"
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

std::ostream& operator<< (std::ostream& os, Peer::LoopData const * lD) {
    return os << "{" << reinterpret_cast<void const *>(lD) << "}"
        << "[" << lD->refCount_ << "]";
}

CommunicationPeer * Peer::get(uv_write_t * r) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(r->data);

    LOG(DEBUG)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    peer->__checkInvariants();

    return peer;
}

CommunicationPeer * Peer::get(uv_timer_t * h) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(h->data);

    LOG(DEBUG)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    peer->__checkInvariants();

    return peer;
}

ActivePeer * Peer::get(uv_connect_t * r) {

    ActivePeer * peer = Peer::get<ActivePeer>(r->handle);

    LOG(DEBUG)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    peer->__checkInvariants();

    return peer;
}

ActivePeer * Peer::get(uv_getaddrinfo_t * r) {
    ActivePeer * peer = static_cast<ActivePeer *>(r->data);

    LOG(DEBUG)
        << "peer {"
        << reinterpret_cast<void *>(peer)
        << "} is about to have its invariants checked"
    ;
    peer->__checkInvariants();

    return peer;
}

class EchoGen {
  public:
    explicit EchoGen(CommunicationPeer const & peer) : peer_(peer) {}

    bool operator()(rpc::SendHandler & handler) {

        if (!handler.StartArray()) {
            return false;
        }

        if (!handler.Uint64(peer_.now())) {
            return false;
        }

        return handler.EndArray();
    }

  private:
    CommunicationPeer const & peer_;
};

void CommunicationPeer::sendEchoReq() {

    yajr::rpc::OutReq< &rpc::method::echo > (
            EchoGen(*this),
            this
        )
        . send();

}

void CommunicationPeer::timeout() {

    uint64_t rtt = now() - lastHeard_;

    LOG(INFO) << this
        << " refcnt: " << uvRefCnt_
        << " lastHeard_: " << lastHeard_
        << " now(): " << now()
        << " rtt >= " << rtt
        << " keepAliveInterval_: " << keepAliveInterval_
        << " handle_.flags: " << reinterpret_cast< void * >(handle_.flags)
    ;

    if (uvRefCnt_ == 1) {
        /* we already have a pending close */
        LOG(INFO) << this << " Already closing";
        return;
    }

    if (rtt <= (keepAliveInterval_ >> 2) ) {

        LOG(DEBUG) << this << " still waiting";

        return;

    }

    if (rtt > (keepAliveInterval_ << 2) ) {

        LOG(INFO) << this << " tearing down the connection upon timeout";

        /* close the connection and hope for the best */
        if (!uv_is_closing((uv_handle_t*)&handle_)) {
            uv_close((uv_handle_t*)&handle_, on_close);
        }

        return;
    }

    /* send echo request */
    LOG(DEBUG) << this << " sending a ping for keep-alive";
    sendEchoReq();

}

int comms::internal::CommunicationPeer::choke() const {

    LOG(DEBUG)
        << this
    ;

    if (choked_) {

        LOG(WARNING)
            << this
            << " already choked"
        ;

        return 0;
    }

    int rc;

    if ((rc = uv_read_stop((uv_stream_t*) &handle_))) {

        LOG(WARNING) << "uv_read_stop: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);

        /* FIXME: this might even not be a big issue if SSL is not involved */

        onError(rc);

        if (!uv_is_closing((uv_handle_t*)&handle_)) {
            uv_close((uv_handle_t*)&handle_, on_close);
        }

    } else {

        choked_ = 1;

    }

    return rc;

}

int comms::internal::CommunicationPeer::unchoke() const {

    LOG(DEBUG)
        << this
    ;

    if (!choked_) {

        LOG(WARNING)
            << this
            << " already unchoked"
        ;

        return 0;
    }

    int rc;

    if ((rc = uv_read_start(
                    (uv_stream_t*) &handle_,
                    transport_.callbacks_->allocCb_,
                    transport_.callbacks_->onRead_)
    )) {

        LOG(WARNING) << "uv_read_start: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);

        onError(rc);

        if (!uv_is_closing((uv_handle_t*)&handle_)) {
            uv_close((uv_handle_t*)&handle_, on_close);
        }

    } else {

        choked_ = 0;

    }

    return rc;

}

yajr::rpc::InboundMessage * comms::internal::CommunicationPeer::parseFrame() const {

    LOG(DEBUG)
        << this
        << " About to parse: \""
        << ssIn_.str()
        << "\" from "
        << ssIn_.str().size()
        << " bytes at "
        << reinterpret_cast<void const *>(ssIn_.str().data())
    ;

    bumpLastHeard();
    yajr::comms::internal::wrapper::IStreamWrapper is(ssIn_);

    docIn_.ParseStream(is);
    if (docIn_.HasParseError()) {
        rapidjson::ParseErrorCode e = docIn_.GetParseError();
        size_t o = docIn_.GetErrorOffset();

        LOG(ERROR)
            << "Error: " << rapidjson::GetParseError_En(e)
            << " at offset " << o
        ;
    }

    /* wipe ssIn_ out */
    // std::stringstream().swap(ssIn_); // C++11 only
    ssIn_.~basic_stringstream();
    new ((void *) &ssIn_) std::stringstream();

    return yajr::rpc::MessageFactory::getInboundMessage(*this, docIn_);
}

bool Peer::__checkInvariants()
#ifndef NDEBUG
    const
#endif
{
    LOG(DEBUG)
        << this
        << " true = true"
    ;
    return true;
}

#ifndef NDEBUG
bool CommunicationPeer::__checkInvariants() const {

    if (status_ != kPS_ONLINE) {
        LOG(DEBUG)
            << this
            << " status = "
            << static_cast< int >(status_)
            << " just check for Peer's invariants"
        ;
        return internal::Peer::__checkInvariants();
    }

    if (!!keepAliveInterval_ != !!uv_is_active((uv_handle_t *)&keepAliveTimer_)) {
        LOG(DEBUG) << this
            << " keepAliveInterval_ = " << keepAliveInterval_
            << " keepAliveTimer_ = " << (
            uv_is_active((uv_handle_t *)&keepAliveTimer_) ? "" : "in")
            << "active"
        ;
    }
    return
        (!!keepAliveInterval_ == !!uv_is_active((uv_handle_t *)&keepAliveTimer_))

        &&

        internal::Peer::__checkInvariants();
}

bool ActivePeer::__checkInvariants() const {
    return CommunicationPeer::__checkInvariants();
}

bool PassivePeer::__checkInvariants() const {
    return CommunicationPeer::__checkInvariants();
}

bool ListeningPeer::__checkInvariants() const {
    return internal::Peer::__checkInvariants();
}
#endif

void CommunicationPeer::readBuffer(
        char * buffer,
        size_t nread,
        bool canWriteJustPastTheEnd) const {

    assert(nread);

    if (!nread) {
        return;
    }

    char lastByte[2];

    if (!canWriteJustPastTheEnd) {

        lastByte[0] = buffer[nread];

        if (nread > 1) {  /* just as an optimization */
            buffer[nread-1] = '\0';
            readBufferZ(buffer, nread);
        }

        nread = 1;
        buffer = lastByte;

    }

    buffer[nread++] = '\0';
    readBufferZ(buffer, nread);

}

void CommunicationPeer::readBufferZ(char const * buffer, size_t nread) const {

    size_t chunk_size;

    while (--nread > 0) {
        chunk_size = readChunk(buffer);
        nread -= chunk_size++;

        LOG(DEBUG)
            << "nread="
            << nread
            << " chunk_size="
            << chunk_size
        ;

        if(nread) {

            LOG(DEBUG) << "got: " << chunk_size;

            buffer += chunk_size;

            boost::scoped_ptr<yajr::rpc::InboundMessage> msg(
                    parseFrame()
                );

            if (!msg) {
                LOG(ERROR) << "skipping inbound message";
                continue;
            }

            msg->process();

        }
    }
}

void CommunicationPeer::dumpIov(std::stringstream & dbgLog, std::vector<iovec> const & iov) {
    for (size_t i=0; i < iov.size(); ++i) {
        iovec const & j = iov[i];
        dbgLog
            << "\n IOV "
            << i << ": "
            << j.iov_base
            << "+"
            << j.iov_len
        ;
        std::copy((char*)j.iov_base,
                  (char*)j.iov_base + j.iov_len,
                  std::ostream_iterator<char>(dbgLog, ""));
    }
}

#ifdef    NEED_DESPERATE_CPU_BOGGING_AND_THREAD_UNSAFE_DEBUGGING
// Not thread-safe. Build only as needed for ad-hoc debugging builds.
void CommunicationPeer::logDeque() const {
    static std::string oldDbgLog;
    std::stringstream dbgLog;
    std::string newDbgLog;

    dbgLog
        << "\n IOV "
        << this
    ;

    if (pendingBytes_) {
        dbgLog << "\n IOV Pending:";
        dumpIov(dbgLog,
            more::get_iovec(
                s_.deque_.begin(),
                s_.deque_.begin() + pendingBytes_
            )
        );
    }

    dbgLog << "\n IOV Full:";
    dumpIov(dbgLog, more::get_iovec(
                s_.deque_.begin(),
                s_.deque_.end()));

    newDbgLog = dbgLog.str();

    if (oldDbgLog != newDbgLog) {
        oldDbgLog = newDbgLog;

        LOG(DEBUG) << newDbgLog;
    }
}
#endif // NEED_DESPERATE_CPU_BOGGING_AND_THREAD_UNSAFE_DEBUGGING

void CommunicationPeer::onWrite() {

    LOG(DEBUG)
        << this
        << " Write completed for "
        << pendingBytes_
        << " bytes"
    ;

    transport_.callbacks_->onSent_(this);

    pendingBytes_ = 0;

    write(); /* kick the can */

}

int CommunicationPeer::write() const {

    if (pendingBytes_) {

        LOG(DEBUG)
            << this
            << "Waiting for "
            << pendingBytes_
            << " bytes to flush"
        ;
        return 0;
    }

    return transport_.callbacks_->sendCb_(this);

}

int CommunicationPeer::writeIOV(std::vector<iovec>& iov) const {

    LOG(DEBUG)
        << this
        << " IOVEC of size "
        << iov.size()
    ;
    assert(iov.size());

    int rc;
    if ((rc = uv_write(
                    &write_req_,
                    (uv_stream_t*) &handle_,
                    (uv_buf_t*)&iov[0],
                    iov.size(),
                    on_write))) {
        LOG(ERROR)
            << this
            << "uv_write: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        onError(rc);
        /* FIXME: is this enough to handle the error? */
    }

    return rc;

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

}}}
