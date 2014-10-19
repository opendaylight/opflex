/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/comms/comms-internal.hpp>
#include <opflex/rpc/message_factory.inl.hpp>

namespace opflex { namespace comms { namespace internal {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"

CommunicationPeer * Peer::get(uv_write_t * r) {
    return static_cast<CommunicationPeer *>(r->data);
}

CommunicationPeer * Peer::get(uv_timer_t * h) {
    return static_cast<CommunicationPeer *>(h->data);
}

ActivePeer * Peer::get(uv_connect_t * r) {
    /* the following would also work */
    return Peer::get<ActivePeer>(r->handle);
}

ActivePeer * Peer::get(uv_getaddrinfo_t * r) {
    return static_cast<ActivePeer *>(r->data);
}

#pragma clang diagnostic pop

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
    opflex::rpc::OutReq<&rpc::method::echo> * req =
        opflex::rpc::MessageFactory::
        newReq<&rpc::method::echo>(*this, EchoGen(*this));

    req -> send();
}

void CommunicationPeer::timeout() {

    LOG(INFO) << this
        << " refcnt: " << uvRefCnt_
        << " lastHeard_: " << lastHeard_
        << " now(): " << now()
        << " now() - lastHeard_: " << now() - lastHeard_
        << " keepaliveInterval_: " << keepaliveInterval_
    ;

    if (uvRefCnt_ == 1) {
        /* we already have a pending close */
        LOG(INFO) << this << " Already closing";
        return;
    }

    if (now() - lastHeard_ < (keepaliveInterval_ >> 1) ) {

        LOG(DEBUG) << this << " still waiting";

        return;

    }

    if (now() - lastHeard_ > (keepaliveInterval_ << 1) ) {

        LOG(INFO) << this << " tearing down the connection upon timeout";

        /* close the connection and hope for the best */
        uv_close((uv_handle_t*)&handle, on_close);

        return;
    }

    /* send echo request */
    LOG(DEBUG) << "sending a ping for keep-alive";
    sendEchoReq();

}

opflex::rpc::InboundMessage * comms::internal::CommunicationPeer::parseFrame() {

    LOG(DEBUG)
        << "peer = " << this
        << " About to parse: \"" << ssIn_.str() << "\""
    ;

    bumpLastHeard();
    opflex::comms::internal::wrapper::IStreamWrapper is(ssIn_);

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

    return opflex::rpc::MessageFactory::getInboundMessage(*this, docIn_);
}

}}}
