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

    connectionHandler_(this, data_, ::yajr::StateChange::CONNECT, 0);

    /* some transports, like for example SSL/TLS, need to start talking
     * before there's anything to say */
    (void) write();

}

void CommunicationPeer::onDisconnect() {

    VLOG(1)
        << this
        << " connected_ = "
        << static_cast< bool >(connected_)
    ;

 // if (connected_) {
        if (!uv_is_closing(getHandle())) {
            VLOG(2)
                << this
                << " issuing close for tcp handle"
            ;
            uv_close(getHandle(), on_close);
         // uv_close(getHandle(), connected_? on_close : NULL);
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

}

void CommunicationPeer::destroy(bool now) {
    VLOG(1)
        << this
    ;

 // Peer::destroy();
    destroying_ = true;
    onDisconnect();
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

void CommunicationPeer::readBufNoNull(char* buffer,
                   size_t nread) {
    VLOG(6) << "nread " << nread;
    ssIn_.write(buffer, nread);

    boost::scoped_ptr<yajr::rpc::InboundMessage> msg(parseFrame());
    if (!msg) {
        LOG(ERROR) << "skipping inbound message";
        return;
    }

    msg->process();

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
    VLOG(4) << this;

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

    return retVal;

}

int CommunicationPeer::writeIOV(std::vector<iovec>& iov) const {

    VLOG(5)
        << this
        << " IOVEC of size "
        << iov.size()
        << " on_write "
        << (void*)on_write
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
        const_cast<CommunicationPeer *>(this)->onDisconnect();
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
        this->onDisconnect();
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
        const_cast<CommunicationPeer *>(this)->onDisconnect();

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
        const_cast<CommunicationPeer *>(this)->onDisconnect();

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
    if (ssIn_.str().empty()) {
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
            const_cast<CommunicationPeer *>(this)->onDisconnect();
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
            const_cast<CommunicationPeer *>(this)->onDisconnect();
        }
    }

    resetSsIn();

    return ret;
}

} // namespace internal
} // namespace comms
} // namespace yajr

