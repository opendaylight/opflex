/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

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
        LOG(ERROR) << "Received frame with"
                   << (doc.HasMember("id") ? " null" : "out")
                   << " id. Dropping.";
        return NULL;
    }

    /* FIXME: TODO: add proper error handling to validation and
     * make sure to signal the error to the peer whenever possible
     */
    if (doc.HasMember("method")) {
        const rapidjson::Value& methodValue = doc["method"];

        if (!methodValue.IsString()) {
            LOG(ERROR) <<
                "Received request with non-string method. Dropping";
            /* FIXME: TODO: send error back */
            assert(methodValue.IsString());
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
        return NULL;
    }

    if (doc.HasMember(Message::kPayloadKey.result)) {
        const rapidjson::Value & result = doc[Message::kPayloadKey.result];

        return MessageFactory::InboundResult(peer, result, id);
    }

    if (doc.HasMember("error")) {
        const rapidjson::Value & error = doc["error"];
        assert(error.IsObject());

        return MessageFactory::InboundError(peer, error, id);
    }

    /* FIXME: what should we do? */
    assert(0);

    return NULL;
}

yajr::rpc::InboundMessage *
MessageFactory::getInboundMessage(
        yajr::Peer const & peer,
        rapidjson::Document const & doc
        ) {

    LOG(DEBUG);

    yajr::rpc::InboundMessage * msg =
        MessageFactory::InboundMessage(peer, doc);

    return msg;

}

}
}

