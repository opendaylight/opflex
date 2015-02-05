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


#include <yajr/rpc/gen/echo.hpp>
#include <yajr/rpc/internal/json_stream_wrappers.hpp>
#include <yajr/rpc/message_factory.inl.hpp>
#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <rapidjson/error/en.h>

namespace yajr {
    namespace comms {
        namespace internal {

#ifdef COMMS_DEBUG_OBJECT_COUNT
::boost::atomic<size_t> CommunicationPeer::counter(0);
#endif

void CommunicationPeer::startKeepAlive(
        uint64_t begin,
        uint64_t repeat,
        uint64_t interval) {

    LOG(DEBUG) << this
        << " interval=" << interval
        << " begin=" << begin
        << " repeat=" << repeat
    ;

    sendEchoReq();
    bumpLastHeard();

    keepAliveInterval_ = interval;
    uv_timer_start(&keepAliveTimer_, on_timeout, begin, repeat);
}

void CommunicationPeer::stopKeepAlive() {
    LOG(DEBUG) << this;
 // assert(keepAliveInterval_ && uv_is_active((uv_handle_t *)&keepAliveTimer_));

    uv_timer_stop(&keepAliveTimer_);
    keepAliveInterval_ = 0;
}

void CommunicationPeer::on_timeout(uv_timer_t * timer) {
    LOG(DEBUG);

    get(timer)->timeout();
}

void CommunicationPeer::bumpLastHeard() const {
    LOG(DEBUG) << this << " " << lastHeard_ << " -> " << now();
    lastHeard_ = now();
}

void CommunicationPeer::onConnect() {
    connected_ = 1;
    keepAliveTimer_.data = this;
    LOG(DEBUG) << this << " up() for a timer init";
    up();
    uv_timer_init(getUvLoop(), &keepAliveTimer_);
    uv_unref((uv_handle_t*) &keepAliveTimer_);

    connectionHandler_(this, data_, ::yajr::StateChange::CONNECT, 0);

    /* some transports, like for example SSL/TLS, need to start talking
     * before there's anything to say */
    (void) write();
}

void CommunicationPeer::onDisconnect(bool now) {
    LOG(DEBUG)
        << this
        << " connected_ = "
        << static_cast< bool >(connected_)
        << " now = "
        << now
    ;

    if (connected_ || now) {
        LOG(DEBUG)
            << this
            << " issuing close for tcp handle"
        ;
        if (!uv_is_closing((uv_handle_t*)&handle_)) {
            uv_close((uv_handle_t*)&handle_, on_close);
         // uv_close((uv_handle_t*)&handle_, connected_? on_close : NULL);
        }
    }

    if (connected_) {

        /* wipe ssIn_ out */
        // std::stringstream().swap(ssIn_); // C++11 only
        ssIn_.~basic_stringstream();
        new ((void *) &ssIn_) std::stringstream();

        /* wipe deque out and reset pendingBytes_ */
        s_.deque_.clear();
        pendingBytes_ = 0;

        connected_ = 0;

        if (getKeepAliveInterval()) {
            stopKeepAlive();
        }

        if (!uv_is_closing((uv_handle_t*)&keepAliveTimer_)) {
            uv_close((uv_handle_t*)&keepAliveTimer_, on_close);
        }
     // uv_close((uv_handle_t*)&handle_, on_close);

        /* FIXME: might be called too many times? */
        connectionHandler_(this, data_, ::yajr::StateChange::DISCONNECT, 0);

    }

    unlink();

    if (destroying_) {
        LOG(DEBUG)
            << this
            << " already destroying"
        ;
        return;
    }

    if (!passive_) {
        LOG(DEBUG) << this << " active => retry queue";
        /* we should attempt to reconnect later */
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
        status_ = kPS_DISCONNECTED;
    } else {
        LOG(DEBUG) << this << " passive => eventually drop";
        /* whoever it was, hopefully will reconnect again */
        insert(internal::Peer::LoopData::PENDING_DELETE);
        status_ = kPS_PENDING_DELETE;
    }
#ifdef OLD_VERSION
    if (!connected_) {
        return;
    }

    connected_ = 0;

    if (getKeepAliveInterval()) {
        stopKeepAlive();
    }

    if (!uv_is_closing((uv_handle_t*)&keepAliveTimer_)) {
        uv_close((uv_handle_t*)&keepAliveTimer_, on_close);
    }
 // uv_close((uv_handle_t*)&handle_, on_close);

    unlink();

    /* FIXME: might be called too many times? */
    connectionHandler_(this, data_, ::yajr::StateChange::DISCONNECT, 0);

    if (destroying_) {
        return;
    }

    if (!passive_) {
        LOG(DEBUG) << this << " active => retry queue";
        /* we should attempt to reconnect later */
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
    } else {
        LOG(DEBUG) << this << " passive => eventually drop";
        /* whoever it was, hopefully will reconnect again */
        insert(internal::Peer::LoopData::PENDING_DELETE);
    }
#endif

}

void CommunicationPeer::destroy(bool now) {
    LOG(DEBUG) << this;

 // Peer::destroy();
    destroying_ = 1;

    onDisconnect(now);

}

int CommunicationPeer::tcpInit() {

    int rc;

    if ((rc = uv_tcp_init(getUvLoop(), &handle_))) {
        LOG(WARNING) << "uv_tcp_init: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
        return rc;
    }

 // LOG(DEBUG) << "{" << this << "}AP up() for a tcp init";
 // up();

    if ((rc = uv_tcp_keepalive(&handle_, 1, 60))) {
        LOG(WARNING) << "uv_tcp_keepalive: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
    }

    if ((rc = uv_tcp_nodelay(&handle_, 1))) {
        LOG(WARNING) << "uv_tcp_nodelay: [" << uv_err_name(rc) << "] " <<
            uv_strerror(rc);
    }

    return 0;
}

void CommunicationPeer::readBuffer(
        char * buffer,
        size_t nread,
        bool canWriteJustPastTheEnd) const {

    LOG(DEBUG)
        << "nread "
        << nread
        << " @"
        << static_cast< void *>(buffer)
        << ", canWriteJustPastTheEnd = "
        << canWriteJustPastTheEnd
    ;

    assert(nread);

    if (!nread) {
        return;
    }

    char lastByte[2];

    if (!canWriteJustPastTheEnd) {

        lastByte[0] = buffer[nread-1];

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

    LOG(DEBUG)
        << "nread="
        << nread
        << " first "
        << 1 + strlen(buffer)
        << " bytes at @"
        << static_cast< void const * >(buffer)
        << ": ("
        << buffer
        << ")"
    ;

    while (--nread > 0) {
        chunk_size = readChunk(buffer);
        nread -= chunk_size++;

        LOG(DEBUG)
            << "nread="
            << nread
            << " chunk_size="
            << chunk_size
        ;

        if(!nread) {

            break;

        }

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

bool EchoGen::operator () (rpc::SendHandler & handler) {

    if (!handler.StartArray()) {
        return false;
    }

    if (!handler.Uint64(peer_.now())) {
        return false;
    }

#ifndef NDEBUG
    for(size_t i = 0; i < kNcanaries; ++i) {
        if (!handler.String(canary)) {
            LOG(ERROR)
                << "Error emitting canary"
            ;
            assert(0);
            return false;
        }
    }
#endif

    return handler.EndArray();
}

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
#ifndef NDEBUG
    /* generate even more traffic */
    sendEchoReq();
    sendEchoReq();
    sendEchoReq();
#endif

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
        << " About to parse: ("
        << ssIn_.str()
        << ") from "
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
            << " of message: (" << ssIn_.str() << ")"
        ;

        assert(!ssIn_.str().data());
    }

    /* don't clean up ssIn_ yet. yes, it's technically a "dead" variable here,
     * but we might need to inspect it from gdb to make our life easier when
     * getInboundMessage() isn't happy :)
     */
    yajr::rpc::InboundMessage * ret =
        yajr::rpc::MessageFactory::getInboundMessage(*this, docIn_);

    assert(ret);

    /* wipe ssIn_ out */
    // std::stringstream().swap(ssIn_); // C++11 only
    ssIn_.~basic_stringstream();
    new ((void *) &ssIn_) std::stringstream();

    return ret;
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

char const EchoGen::canary[] =
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
                    "_01_qwertyuiopasdfghjklzxcvbnm"
                    "_02_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_03_qwertyuiopasdfghjklzxcvbnm"
                    "_04_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_05_qwertyuiopasdfghjklzxcvbnm"
                    "_06_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_07_qwertyuiopasdfghjklzxcvbnm"
                    "_08_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_09_qwertyuiopasdfghjklzxcvbnm"
                    "_10_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_11_qwertyuiopasdfghjklzxcvbnm"
                    "_12_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_13_qwertyuiopasdfghjklzxcvbnm"
                    "_14_QWERTYUIOPASDFGHJKLZXCVBNM"
                    "_15_qwertyuiopasdfghjklzxcvbnm"
                    "_16_QWERTYUIOPASDFGHJ"
;
size_t const EchoGen::kNcanaries = 20;
#endif

} // namespace internal
} // namespace comms
} // namespace yajr

