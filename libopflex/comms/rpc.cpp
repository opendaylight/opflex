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
#include <yajr/rpc/methods.hpp>
#include <yajr/rpc/rpc.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

namespace yajr {
    namespace rpc {

Message::PayloadKeys Message::kPayloadKey = {
    "params",
    "result",
    "error"
};

bool operator< (rapidjson::Value const & l, rapidjson::Value const & r) {

    rapidjson::Type lt = l.GetType();
    rapidjson::Type rt = r.GetType();

    if (lt < rt) {
        return true;
    }

    if (lt > rt) {
        return false;
    }

    /* same type */
    std::size_t lh = hash_value(l);
    std::size_t rh = hash_value(r);

    return (lh < rh);
}

#if THREAD_LOCAL_DEBUGS_READY
// TODO: make thread-local
static ::yajr::comms::internal::CommunicationPeer const * latestCp = NULL;
#  define __t_assert(arg) assert(arg)
#else
#  define __t_assert(arg) do{} while(0)
#endif

bool LocalIdentifier::emitId(yajr::rpc::SendHandler & h) const {

    VLOG(4);

#if THREAD_LOCAL_DEBUGS_READY
    ::yajr::comms::internal::CommunicationPeer const * cP = latestCp;
#endif

    __t_assert(cP->__checkInvariants());

    if (!h.StartArray()) {

        __t_assert(cP->__checkInvariants());

        return false;
    }

    __t_assert(cP->__checkInvariants());

    if (!h.String(requestMethod())) {

        __t_assert(cP->__checkInvariants());

        return false;
    }

    __t_assert(cP->__checkInvariants());

    if (!h.Uint64(id_)) {

        __t_assert(cP->__checkInvariants());

        return false;
    }

    __t_assert(cP->__checkInvariants());

    if (!h.EndArray()) {

        __t_assert(cP->__checkInvariants());

        return false;
    }

    __t_assert(cP->__checkInvariants());

    return true;

}

bool RemoteIdentifier::emitId(yajr::rpc::SendHandler & h) const {

    VLOG(4);

#if THREAD_LOCAL_DEBUGS_READY
    ::yajr::comms::internal::CommunicationPeer const * cP = latestCp;

    VLOG(5) << latestCp;
#endif

    __t_assert(cP->__checkInvariants());

    bool ok = id_.Accept(h);

    __t_assert(cP->__checkInvariants());

    return ok;
}

bool OutboundMessage::Accept(yajr::rpc::SendHandler& handler) {

    VLOG(5);

#if THREAD_LOCAL_DEBUGS_READY
    ::yajr::comms::internal::CommunicationPeer const * cP = latestCp;

    VLOG(5) << latestCp;
#endif

    __t_assert(cP->__checkInvariants());

    if (!handler.StartObject()) {
        __t_assert(cP->__checkInvariants());
        return false;
    }

    __t_assert(cP->__checkInvariants());
    if (!handler.String("id")) {
        __t_assert(cP->__checkInvariants());
        return false;
    }

    __t_assert(cP->__checkInvariants());
    if (!emitId(handler)) {
        __t_assert(cP->__checkInvariants());
        return false;
    }
    __t_assert(cP->__checkInvariants());
    if (!emitMethod(handler)) {
        __t_assert(cP->__checkInvariants());
        return false;
    }

    __t_assert(cP->__checkInvariants());
    if (!handler.String(getPayloadKey())) {
        __t_assert(cP->__checkInvariants());
        return false;
    }
    __t_assert(cP->__checkInvariants());
    if (!payloadGenerator_(handler)) {
        __t_assert(cP->__checkInvariants());
        return false;
    }

    __t_assert(cP->__checkInvariants());
    if (!handler.EndObject()) {
        __t_assert(cP->__checkInvariants());
        return false;
    }

    return true;

}

bool OutboundMessage::send() {

    VLOG(5);

    ::yajr::comms::internal::CommunicationPeer const * cP =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer const * >
        (peer_);

    assert(cP &&
            "peer needs to be set "
            "to a proper communication peer "
            "before outbound messages are sent");

    VLOG(5) << cP;

    unsigned char connected = cP->connected_;

    if (!connected) {
        cP->onError(UV_ENOTCONN);

     // assert(connected);

        return false;
    }

    assert(cP->__checkInvariants());

#if THREAD_LOCAL_DEBUGS_READY
    latestCp = cP;
#endif
    bool ok = Accept(cP->getWriter());

    if (!ok) {
        LOG(ERROR)
            << cP
            << " problem Accept()ing a message"
        ;

        assert(ok);
    }

    assert(cP->__checkInvariants());

    cP->delimitFrame();

    assert(cP->__checkInvariants());

    cP->write();

    assert(cP->__checkInvariants());

    return true;

}

bool OutboundRequest::send(
        ::yajr::Peer const & peer
    ) {

    VLOG(3);

    setPeer(&peer);

    return send();
}

InboundResponse::InboundResponse(
    yajr::Peer const & peer,             /**< [in] where we received from */
    rapidjson::Value const & response,/**< [in] the error/result received */
    rapidjson::Value const & id          /**< [in] the id of this message */
    )
    :
      InboundMessage(peer, response),
      LocalIdentifier(
              MessageFactory::lookupMethod(
                  id[rapidjson::SizeType(0)].GetString()
                ),
              id[1].GetUint64()
          )
    {}

uv_loop_t const *
yajr::rpc::Message::getUvLoop() const {
    return dynamic_cast< ::yajr::comms::internal::CommunicationPeer const * >
        (getPeer())->getUvLoop();
}

std::size_t hash_value(rapidjson::Value const& v) {

    /* switch(v.GetType()) { <-- would be more efficient but would have to
                                 rely on private stuff from Value */

    if(v.IsDouble()) {
        return boost::hash<double>()(v.GetDouble());
    }

    if(v.IsUint64()) {
        return boost::hash<uint64_t>()(v.GetUint64());
    }

    if(v.IsInt64()) {
        return boost::hash<int64_t>()(v.GetInt64());
    }

    if(v.IsBool()) {
        return boost::hash<bool>()(v.GetBool());
    }

    if(v.IsNull()) {
        return boost::hash<uintptr_t>()(0);
    }

    if(v.IsArray()) {
        std::size_t h = 0;

        for (rapidjson::Value::ConstValueIterator
                itr = v.Begin(); itr != v.End(); ++itr) {
            h ^= hash_value(*itr);
        }

        return h;
    }

    if(v.IsObject()) {
        std::size_t h = 0;

        for (rapidjson::Value::ConstMemberIterator
                itr = v.MemberBegin(); itr != v.MemberEnd(); ++itr) {
            h ^= boost::hash<char const *>()(itr->name.GetString());
            h ^= hash_value(itr->value);
        }

        return h;
    }

    if (v.IsString()) {
        return boost::hash<std::string>()(std::string(v.GetString()));
    }

    assert(0);

    return 0;
}

std::size_t hash_value(yajr::rpc::LocalId const & id) {
    return
        boost::hash<MethodName*>()(id.methodName_)
    ^
        boost::hash<uint64_t>()(id.id_)
    ;
}

std::size_t hash_value(yajr::rpc::LocalIdentifier const & id) {
    return boost::hash<yajr::rpc::LocalId>()(id.getLocalId());
}

std::size_t hash_value(yajr::rpc::RemoteIdentifier const & id) {
    return hash_value(id.getRemoteId());
}

MethodName method::unknown("unknown");
MethodName method::echo("echo");
MethodName method::send_identity("send_identity");
MethodName method::policy_resolve("policy_resolve");
MethodName method::policy_unresolve("policy_unresolve");
MethodName method::policy_update("policy_update");
MethodName method::endpoint_declare("endpoint_declare");
MethodName method::endpoint_undeclare("endpoint_undeclare");
MethodName method::endpoint_resolve("endpoint_resolve");
MethodName method::endpoint_unresolve("endpoint_unresolve");
MethodName method::endpoint_update("endpoint_update");
MethodName method::state_report("state_report");
MethodName method::custom("custom");

} /* yajr::rpc namespace */
} /* yajr namespace */

