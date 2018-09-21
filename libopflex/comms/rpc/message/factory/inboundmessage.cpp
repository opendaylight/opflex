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


#include <yajr/rpc/methods.hpp>
#include <yajr/rpc/rpc.hpp>

namespace yajr {

namespace comms {
    namespace internal {
        class CommunicationPeer;
    }
}

namespace rpc {

yajr::rpc::InboundMessage *
MessageFactory::InboundMessage(
        yajr::Peer const & peer,
        rapidjson::Document const & doc) {

    /* IDs are tricky because they can be any JSON entity.
     * We need to preserve them as such.
     *
     * Robustness Principle: "Be liberal in what you accept,
     * and conservative in what you send".
     */

    /* NOTA BENE: this might assert() in debug builds, and
     * yield a NullValue in final builds. That's exactly what
     * we want and it allow us to write much simpler code ;-) */
    const rapidjson::Value& id = doc["id"]; // <-- READ COMMENT ABOVE!!!

    /* we don't accept any notifications */
    if (id.IsNull()) {
        LOG(ERROR)
            << &peer
            << " Received frame with"
            << (doc.HasMember("id") ? " null" : "out")
            << " id. Dropping."
        ;

        dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
            ->onError(UV_EPROTO);

        return NULL;
    }

    if (doc.HasMember("method")) {
        const rapidjson::Value& methodValue = doc["method"];

        if (!methodValue.IsString()) {
            LOG(ERROR)
                << &peer
                << " Received request with non-string method. Dropping"
            ;

            dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
                ->onError(UV_EPROTO);
            dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
                ->onDisconnect();

#ifdef ASSERT_ON_PROTO_ERRORS
            assert(methodValue.IsString());
#endif

            return NULL;
        }

        const char * method = methodValue.GetString();

        /* Once again, be liberal with input... On a case by case
         * basis, the handler will respond with an error if it is
         * unacceptable to have no parameters for the invoked method.
         */
        const rapidjson::Value& params = doc[Message::kPayloadKey.params];

        return MessageFactory::InboundRequest(peer, params, method, id);
    }

    /* id must be an array for replies, and its first element must be a string */
    assert(id.IsArray());
    assert(id[rapidjson::SizeType(0)].IsString());
    if (!id.IsArray() || !id[rapidjson::SizeType(0)].IsString()) {

        dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
            ->onError(UV_EPROTO);
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
            ->onDisconnect();
#ifdef ASSERT_ON_PROTO_ERRORS
        assert(0);
#endif

        return NULL;
    }

    if (doc.HasMember(Message::kPayloadKey.result)) {
        const rapidjson::Value & result = doc[Message::kPayloadKey.result];

        return MessageFactory::InboundResult(peer, result, id);
    }

    if (doc.HasMember("error")) {
        const rapidjson::Value & error = doc["error"];
#ifdef ASSERT_ON_PROTO_ERRORS
        assert(error.IsObject());
#endif

        if (!error.IsObject()) {

            dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
                ->onError(UV_EPROTO);
            dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
                ->onDisconnect();

            return NULL;
        }

        return MessageFactory::InboundError(peer, error, id);
    }

    dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
        ->onError(UV_EPROTO);
    dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer)
        ->onDisconnect();
#ifdef ASSERT_ON_PROTO_ERRORS
    assert(0);
#endif

    return NULL;
}

yajr::rpc::InboundMessage *
MessageFactory::getInboundMessage(
        yajr::Peer const & peer,
        rapidjson::Document const & doc
        ) {

    VLOG(5);

    yajr::rpc::InboundMessage * msg =
        MessageFactory::InboundMessage(peer, doc);

    return msg;

}

}
}

