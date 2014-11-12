/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <yajr/internal/comms.hpp>
#include <yajr/rpc/message_factory.inl.hpp>
#include <rapidjson/error/en.h>
#include <yajr/rpc/internal/json_stream_wrappers.hpp>

namespace yajr { namespace comms { namespace internal {

::boost::atomic<size_t> Peer::counter(0);
::boost::atomic<size_t> Peer::LoopData::counter(0);
::boost::atomic<size_t> CommunicationPeer::counter(0);
::boost::atomic<size_t> ActivePeer::counter(0);
::boost::atomic<size_t> PassivePeer::counter(0);
::boost::atomic<size_t> ListeningPeer::counter(0);

std::ostream& operator<< (std::ostream& os, Peer const * p) {
    return os << "{" << reinterpret_cast<void const *>(p) << "}"
#ifndef NDEBUG
        << p->peerType()
#endif
        << "[" << p->uvRefCnt_ << "]";
}

std::ostream& operator<< (std::ostream& os, Peer::LoopData const * lD) {
    return os << "{" << reinterpret_cast<void const *>(lD) << "}"
        << "[" << lD->refCount_ << "]";
}

CommunicationPeer * Peer::get(uv_write_t * r) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(r->data);

    peer->__checkInvariants();

    return peer;
}

CommunicationPeer * Peer::get(uv_timer_t * h) {
    CommunicationPeer * peer = static_cast<CommunicationPeer *>(h->data);

    peer->__checkInvariants();

    return peer;
}

ActivePeer * Peer::get(uv_connect_t * r) {
    /* the following would also work */
    ActivePeer * peer = Peer::get<ActivePeer>(r->handle);

    peer->__checkInvariants();

    return peer;
}

ActivePeer * Peer::get(uv_getaddrinfo_t * r) {
    ActivePeer * peer = static_cast<ActivePeer *>(r->data);

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
    yajr::rpc::OutReq<&rpc::method::echo> * req =
        yajr::rpc::MessageFactory::
        newReq<&rpc::method::echo>(*this, EchoGen(*this));

    req -> send();
}

void CommunicationPeer::timeout() {

    uint64_t rtt = now() - lastHeard_;

    LOG(INFO) << this
        << " refcnt: " << uvRefCnt_
        << " lastHeard_: " << lastHeard_
        << " now(): " << now()
        << " rtt >= " << rtt
        << " keepAliveInterval_: " << keepAliveInterval_
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
        uv_close((uv_handle_t*)&handle_, on_close);

        return;
    }

    /* send echo request */
    LOG(DEBUG) << this << " sending a ping for keep-alive";
    sendEchoReq();

}

yajr::rpc::InboundMessage * comms::internal::CommunicationPeer::parseFrame() {

    LOG(DEBUG)
        << "peer = " << this
        << " About to parse: \"" << ssIn_.str() << "\""
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
            << " near '" << std::string(ssIn_.str()).substr(o, 10) << "...'"
        ;
    }

    /* wipe ssIn_ out */
    // std::stringstream().swap(ssIn_); // C++11 only
    ssIn_.~basic_stringstream();
    new ((void *) &ssIn_) std::stringstream();

    return yajr::rpc::MessageFactory::getInboundMessage(*this, docIn_);
}

bool Peer::__checkInvariants() const {
    return true;
}

bool CommunicationPeer::__checkInvariants() const {

    if (status_ != kPS_ONLINE) {
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

void CommunicationPeer::onWrite() {

    LOG(DEBUG) << "Write completed for " << pendingBytes_ << " bytes";

    s_.deque_.erase(
            s_.deque_.begin(),
            s_.deque_.begin() + pendingBytes_
    );

    pendingBytes_ = 0;

    write(); /* kick the can */
}

int CommunicationPeer::write() const {

    if (pendingBytes_) {

        LOG(DEBUG) << "Waiting for " << pendingBytes_ << " bytes to flush";
        return 0;
    }

    pendingBytes_ = s_.deque_.size();

    if (!pendingBytes_) {

        LOG(DEBUG) << "Nothing left to be sent!";

        return 0;
    }

    std::vector<iovec> iov =
        more::get_iovec(
                s_.deque_.begin(),
                s_.deque_.end()
        );

    logDeque();

    /* FIXME: handle errors!!! */
    return uv_write(&write_req_,
            (uv_stream_t*) &handle_,
            (uv_buf_t*)&iov[0],
            iov.size(),
            on_write);

}

}}}
