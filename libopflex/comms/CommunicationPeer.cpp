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

#include <cctype>

namespace yajr {
    namespace internal {

        bool isLegitPunct(int c) {

            switch(c) {
                case '\0':
                case  ' ':
                case  '"':
                case  '%':
                case  ',':
                case  '-':
                case  '.':
                case  '/':
                case  ':':
                case  '[':
                case  ']':
                case  '_':
                case  '{':
                case  '|':
                case  '}':
                case  '\\':
                case  '(': 
                case  ')': 
                case  '<': 
                case  '>': 
                case  '$':
                case  '\n':
                case  '\t':
                case  '\r':
                case  '!':
                case  '#':
                case  '&':
                case  '*':
                case  '+':
                case  '\'':
                case  ';':
                case  '=':
                case  '?':
                case  '^':
                case  '`':
                case  '@':
                case  '~':
                    return true;

                default:
                    return (
                        (c & 0xe0)             // ASCII from 1 to 31 are BAD
                    &&
                        std::isalnum(c)        // alphanumeric values are GOOD
                    );
            }

        }
    }
    namespace comms {
        namespace internal {

#ifdef COMMS_DEBUG_OBJECT_COUNT
::boost::atomic<size_t> CommunicationPeer::counter(0);
#endif

void CommunicationPeer::startKeepAlive(
        uint64_t begin,
        uint64_t repeat,
        uint64_t interval) {

    VLOG(1)
        << this
        << " interval="
        <<   interval
        << " begin="
        <<   begin
        << " repeat="
        <<   repeat
    ;

    sendEchoReq();
    bumpLastHeard();

    keepAliveInterval_ = interval;
    uv_timer_start(&keepAliveTimer_, on_timeout, begin, repeat);
}

void CommunicationPeer::stopKeepAlive() {
    VLOG(1)
        << this
    ;
 // assert(keepAliveInterval_ && uv_is_active((uv_handle_t *)&keepAliveTimer_));

    uv_timer_stop(&keepAliveTimer_);
    keepAliveInterval_ = 0;
}

void CommunicationPeer::on_timeout(uv_timer_t * timer) {
    VLOG(6);

    get(timer)->timeout();
}

void CommunicationPeer::bumpLastHeard() const {

    VLOG(5)
        << this
        << " "
        << lastHeard_
        << " -> "
        << now()
    ;

    lastHeard_ = now();

}

void CommunicationPeer::onConnect() {
    connected_ = 1;
    status_ = internal::Peer::kPS_ONLINE;

    keepAliveTimer_.data = this;
    VLOG(1)
        << this
        << " up() for a timer init"
    ;
    up();
    uv_timer_init(getUvLoop(), &keepAliveTimer_);
    uv_unref((uv_handle_t*) &keepAliveTimer_);

    assert(__checkInvariants());

    connectionHandler_(this, data_, ::yajr::StateChange::CONNECT, 0);

    assert(__checkInvariants());

    /* some transports, like for example SSL/TLS, need to start talking
     * before there's anything to say */
    (void) write();

}

void CommunicationPeer::onDisconnect(bool now) {

    VLOG(1)
        << this
        << " connected_ = "
        << static_cast< bool >(connected_)
        << " now = "
        << now
    ;

 // if (connected_ || now) {
        if (!uv_is_closing((uv_handle_t*)getHandle())) {
            VLOG(2)
                << this
                << " issuing close for tcp handle"
            ;
            uv_close((uv_handle_t*)getHandle(), on_close);
         // uv_close((uv_handle_t*)getHandle(), connected_? on_close : NULL);
        }
 // }

    if (connected_) {

        /* wipe deque out and reset pendingBytes_ */
        s_.deque_.clear();
        pendingBytes_ = 0;

        connected_ = 0;

        resetSsIn();

        if (getKeepAliveInterval()) {
            stopKeepAlive();
        }

        if (!uv_is_closing((uv_handle_t*)&keepAliveTimer_)) {
            uv_close((uv_handle_t*)&keepAliveTimer_, on_close);
        }
     // uv_close((uv_handle_t*)getHandle(), on_close);

        /* FIXME: might be called too many times? */
        connectionHandler_(this, data_, ::yajr::StateChange::DISCONNECT, 0);

    }

    unlink();

    if (destroying_) {
        VLOG(2)
            << this
            << " already destroying"
        ;
        return;
    }

    if (!passive_) {
        VLOG(2)
            << this
            << " active => retry queue"
        ;
        /* we should attempt to reconnect later */
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
        status_ = kPS_DISCONNECTED;
    } else {
        VLOG(2)
            << this
            << " passive => eventually drop"
        ;
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
 // uv_close((uv_handle_t*)getHandle(), on_close);

    unlink();

    /* FIXME: might be called too many times? */
    connectionHandler_(this, data_, ::yajr::StateChange::DISCONNECT, 0);

    if (destroying_) {
        return;
    }

    if (!passive_) {
        VLOG(2)
            << this
            << " active => retry queue"
        ;
        /* we should attempt to reconnect later */
        insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
    } else {
        VLOG(2)
            << this
            << " passive => eventually drop";
        /* whoever it was, hopefully will reconnect again */
        insert(internal::Peer::LoopData::PENDING_DELETE);
    }
#endif

}

void CommunicationPeer::destroy(bool now) {
    VLOG(1)
        << this
    ;

 // Peer::destroy();
    destroying_ = 1;

    onDisconnect(now);

}

int CommunicationPeer::tcpInit() {

    int rc;

    if ((rc = uv_tcp_init(getUvLoop(), reinterpret_cast<uv_tcp_t *>(getHandle())))) {
        LOG(WARNING)
            << "uv_tcp_init: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
        return rc;
    }

 // LOG(DEBUG) << "{" << this << "}AP up() for a tcp init";
 // up();

    if ((rc = uv_tcp_keepalive(reinterpret_cast<uv_tcp_t *>(getHandle()), 1, 60))) {
        LOG(WARNING)
            << "uv_tcp_keepalive: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;
    }

    if ((rc = uv_tcp_nodelay(reinterpret_cast<uv_tcp_t *>(getHandle()), 1))) {
        LOG(WARNING)
            << "uv_tcp_nodelay: ["
            << uv_err_name(rc)
            << "] "
            <<
            uv_strerror(rc)
        ;
    }

    return 0;
}

void CommunicationPeer::readBuffer(
        char * buffer,
        size_t nread,
        bool canWriteJustPastTheEnd) const {

    VLOG(6)
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

    VLOG(6)
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

    while ((--nread > 0) && connected_) {
        chunk_size = readChunk(buffer);
        nread -= chunk_size++;

        VLOG(6)
            << "nread="
            << nread
            << " chunk_size="
            << chunk_size
        ;

        if(!nread) {

            break;

        }

        VLOG(5)
            << "got: "
            << chunk_size
        ;

        buffer += chunk_size;

        boost::scoped_ptr<yajr::rpc::InboundMessage> msg(
                parseFrame()
            );

        if (!msg) {
            LOG(ERROR)
                << "skipping inbound message"
            ;
            continue;
        }

        msg->process();

    }
}

void CommunicationPeer::dumpIov(std::stringstream & dbgLog, std::vector<iovec> const & iov) {
    dbgLog << "DUMP_IOV\n";
    for (size_t i=0; i < iov.size(); ++i) {
        iovec const & j = iov[i];
        dbgLog
            << "\n IOV "
            << i
            << ": "
            << j.iov_base
            << "+"
            << j.iov_len
        ;
        std::copy((char*)j.iov_base,
                  (char*)j.iov_base + j.iov_len,
                  std::ostream_iterator<char>(dbgLog, ""));
    }
}


std::ostream& operator << (
        std::ostream& os,
        std::vector<iovec> const & iov) {

    os << "INLINE_DUMP\n";

    for (size_t i = 0; i < iov.size(); ++i) {

        os
            << "("
            << i
            << "@"
            << std::hex
            << iov[i].iov_base
            << std::dec
            << "+"
            << iov[i].iov_len
        ;

        if (VLOG_IS_ON(7)) {
            size_t len = iov[i].iov_len;
            size_t offset = 0;
            std::string temp((const char*)iov[i].iov_base, len);

            do {
                os
                    << "("
                    << temp.c_str() + offset
                    << ")"
                    ;
                offset += strlen(temp.c_str()) + 1;
            } while (len > offset);
        }

        os << ")";

    }
    return os;
}

void CommunicationPeer::onWrite() {

    VLOG(5)
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

    assert(__checkInvariants());

    if (pendingBytes_) {

        VLOG(4)
            << this
            << "Waiting for "
            << pendingBytes_
            << " bytes to flush"
        ;
        return 0;
    }

    int retVal = transport_.callbacks_->sendCb_(this);

    assert(__checkInvariants());

    return retVal;

}

int CommunicationPeer::writeIOV(std::vector<iovec>& iov) const {

    VLOG(5)
        << this
        << " IOVEC of size "
        << iov.size()
    ;
    assert(iov.size());

    int rc;
    if ((rc = uv_write(
                    &write_req_,
                    (uv_stream_t*) getHandle(),
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
        onDisconnect();
    } else {
        const_cast<CommunicationPeer *>(this)->up();
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

#if 0
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

    VLOG(5)
        << this
        << " refcnt: "
        << uvRefCnt_
        << " lastHeard_: "
        <<   lastHeard_
        << " now(): "
        <<   now()
        << " rtt >= "
        <<   rtt
        << " keepAliveInterval_: "
        <<   keepAliveInterval_
        <<                          " getHandle()->flags: "
        << reinterpret_cast< void * >(getHandle()->flags)
    ;

    if (uvRefCnt_ == 1) {
        /* we already have a pending close */
        VLOG(4)
            << this
            << " Already closing"
        ;
        return;
    }

    if (rtt <= (keepAliveInterval_ >> 3) ) {

        VLOG(5)
            << this
            << " still waiting"
        ;

        return;

    }

    if (rtt > (keepAliveInterval_ << 3) ) {

        LOG(WARNING)
            << this
            << " tearing down the connection upon timeout"
        ;

        /* close the connection and hope for the best */
        onDisconnect();
     // if (!uv_is_closing((uv_handle_t*)getHandle())) {
     //     uv_close((uv_handle_t*)getHandle(), on_close);
     // }

        return;
    }

    /* send echo request */
    VLOG(5)
        << this
        << " sending a ping for keep-alive"
    ;

    sendEchoReq();
#if 0
    /* generate even more traffic */
    sendEchoReq();
    sendEchoReq();
    sendEchoReq();
#endif

}

int comms::internal::CommunicationPeer::choke() const {

    VLOG(4)
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

    if ((rc = uv_read_stop((uv_stream_t*) getHandle()))) {

        LOG(WARNING)
            << "uv_read_stop: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;

        /* FIXME: this might even not be a big issue if SSL is not involved */

        onError(rc);
        onDisconnect();

    } else {

        choked_ = 1;

    }

    return rc;

}

int comms::internal::CommunicationPeer::unchoke() const {

    VLOG(4)
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
                    (uv_stream_t*) getHandle(),
                    transport_.callbacks_->allocCb_,
                    transport_.callbacks_->onRead_)
    )) {

        LOG(WARNING)
            << "uv_read_start: ["
            << uv_err_name(rc)
            << "] "
            << uv_strerror(rc)
        ;

        onError(rc);
        onDisconnect();

    } else {

        choked_ = 0;

    }

    return rc;

}

yajr::rpc::InboundMessage * comms::internal::CommunicationPeer::parseFrame() const {

    VLOG(6)
        << this
        << " About to parse: ("
        << ssIn_.str()
        << ") from "
        << ssIn_.str().size()
        << " bytes at "
        << reinterpret_cast<void const *>(ssIn_.str().data())
    ;

    bumpLastHeard();

    /* empty frames are legal too */
    if (!ssIn_.str().size()) {
        return NULL;
    }

    yajr::rpc::InboundMessage * ret = NULL;
    yajr::comms::internal::wrapper::IStreamWrapper is(ssIn_);

    docIn_.GetAllocator().Clear();

    docIn_.ParseStream(is);
    if (docIn_.HasParseError()) {
        rapidjson::ParseErrorCode e = docIn_.GetParseError();
        size_t o = docIn_.GetErrorOffset();

        LOG(ERROR)
            << "Error: "
            << rapidjson::GetParseError_En(e)
            << " at offset "
            << o
            << " of message: ("
            << ssIn_.str()
            << ")"
        ;

        if (ssIn_.str().data()) {
            onError(UV_EPROTO);
            onDisconnect();
        }

        // ret stays set to NULL

    } else {

        /* don't clean up ssIn_ yet. yes, it's technically a "dead" variable here,
         * but we might need to inspect it from gdb to make our life easier when
         * getInboundMessage() isn't happy :)
         */
        ret = yajr::rpc::MessageFactory::getInboundMessage(*this, docIn_);

        // assert(ret);
        if (!ret) {
            onError(UV_EPROTO);
            onDisconnect();
        }
    }

    resetSsIn();

    return ret;
}

#ifdef EXTRA_CHECKS
bool CommunicationPeer::__checkInvariants() const {

    bool result = true;

    if (!internal::Peer::__checkInvariants()) {
        result = false;
    }

    if (!!connected_ != !!(status_ == kPS_ONLINE)) {
        VLOG(7)  // should be an ERROR but we need to first clean things up
            << this
            << " has incongruent state: connected_ = "
            << static_cast< bool >(connected_)
            << " status_ = "
            << static_cast< int >(status_)
        ;

        /* TODO: clean up status book-keeping */
     // result = false;
    }

    if (getHandle()->data != this) {
        LOG(ERROR)
            << this
            << " getHandle()->data = "
            <<   getHandle()->data
            << " should be "
            << reinterpret_cast< void const * >(this)
        ;

        result = false;
    }

 // if (status_ != kPS_ONLINE) {
    if (connected_) {

        if (keepAliveTimer_.data != this) {
            LOG(ERROR)
                << this
                << " keepAliveTimer_.data = "
                <<   keepAliveTimer_.data
                << " should be "
                << reinterpret_cast< void const * >(this)
            ;

            result = false;
        }

        if (!!keepAliveInterval_ != !!uv_is_active((uv_handle_t *)&keepAliveTimer_)) {
            LOG(ERROR)
                << this
                << " keepAliveInterval_ = "
                <<   keepAliveInterval_
                <<                             " keepAliveTimer_ = "
                << (uv_is_active((uv_handle_t *)&keepAliveTimer_) ? "" : "in")
                << "active"
            ;

            result = false;
        }

    } else {

        VLOG(5)
            << this
            << " status = "
            << static_cast< int >(status_)
            << " connected_ = "
            << static_cast< int >(connected_)
            << " just check for Peer's invariants"
        ;


    }

    return result;
}
#endif

#if 0
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

