/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of opflex messages for engine
 *
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


#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/Processor.h"
#include "opflex/engine/internal/OvsdbConnection.h"

#include "opflex/logging/internal/logging.hpp"
#include <yajr/yajr.hpp>
#include <yajr/rpc/rpc.hpp>
#include <yajr/rpc/methods.hpp>

namespace opflex {
namespace engine {
namespace internal {

using std::string;
using rapidjson::Value;
using rapidjson::Writer;

void OpflexHandler::setState(ConnectionState state_) {
    state = state_;
    if (state == READY)
        conn->notifyReady();
    else if (state == FAILED)
        conn->notifyFailed();
}

void OpflexHandler::handleUnexpected(const string& type) {
    LOG(ERROR) << "Unexpected message of type " << type;
    conn->disconnect();
}

void OpflexHandler::handleUnsupportedReq(const rapidjson::Value& id,
                                         const string& type) {
    LOG(WARNING) << "[" << getConnection()->getRemotePeer() << "] "
                 << "Ignoring unsupported request of type " << type;
    sendErrorRes(id, "EUNSUPPORTED", "Unsupported request");
}

void OpflexHandler::handleError(uint64_t reqId,
                                const rapidjson::Value& payload,
                                const string& type) {
    string code;
    string message;
    if (payload.HasMember("code")) {
        const Value& v = payload["code"];
        if (v.IsString())
            code = v.GetString();
    }
    if (payload.HasMember("message")) {
        const Value& v = payload["message"];
        if (v.IsString())
            message = v.GetString();
    }
    LOG(ERROR) << "[" << getConnection()->getRemotePeer() << "] "
               << "Remote peer returned error with message ("
               << reqId << "," << type
               << "): " << code << ": " << message;
}

class ErrorRes : public OpflexMessage {
public:
    ErrorRes(const Value& id,
             const string& code_,
             const string& message_)
        : OpflexMessage("", ERROR_RESPONSE, &id),
          code(code_), message(message_) {

    }

    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual ErrorRes* clone() {
        return new ErrorRes(*this);
    }

    template <typename T>
    bool operator()(Writer<T> & handler) {
        handler.StartObject();
        handler.String("code");
        handler.String(code.c_str());
        handler.String("message");
        handler.String(message.c_str());
        handler.EndObject();
        return true;
    }

    const string code;
    const string message;
};

void OpflexHandler::sendErrorRes(const Value& id,
                                 const string& code,
                                 const string& message) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    id.Accept(writer);
    LOG(ERROR) << "Error processing message " << sb.GetString()
               << ": " << code << ": " << message;
    getConnection()->sendMessage(new ErrorRes(id, code, message), true);
}

bool OpflexHandler::requireReadyReq(const Value& id,
                                    const std::string& method) {
    if (isReady()) return true;

    sendErrorRes(id, "ESTATE", "Unexpected request of type " + method);
    handleUnexpected(method);
    return false;
}

bool OpflexHandler::requireReadyRes(uint64_t reqId,
                                    const std::string& method) {
    if (isReady()) return true;
    handleUnexpected(method);
    return false;
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

namespace yajr {
namespace rpc {

// Following are definitions that forward the message handlers from
// the opflex RPC library into our own handler

#define HANDLE_REQ_BASE(name) \
    ((opflex::engine::internal::OpflexConnection*)getPeer()->getData())\
    ->getHandler()->handle##name(getRemoteId(), getPayload())
#define HANDLE_RES_BASE(name) \
    ((opflex::engine::internal::OpflexConnection*)getPeer()->getData())\
    ->getHandler()->handle##name(getLocalId().id_, getPayload())

#define HANDLE_REQ(name)                                                \
    if (((opflex::engine::internal::OpflexConnection*)getPeer()->getData()) \
        ->getHandler()->requireReadyReq(getRemoteId(),                  \
                                        std::string(#name)))            \
        HANDLE_REQ_BASE(name)
#define HANDLE_RES(name)                                                \
    if (((opflex::engine::internal::OpflexConnection*)getPeer()->getData()) \
        ->getHandler()->requireReadyRes(getLocalId().id_,               \
                                        std::string(#name)))            \
        HANDLE_RES_BASE(name)

template<>
void InbReq<&yajr::rpc::method::send_identity>::process() const {
    HANDLE_REQ_BASE(SendIdentityReq);
}
template<>
void InbRes<&yajr::rpc::method::send_identity>::process() const {
    HANDLE_RES_BASE(SendIdentityRes);
}
template<>
void InbErr<&yajr::rpc::method::send_identity>::process() const {
    HANDLE_RES_BASE(SendIdentityErr);
}

template<>
void InbReq<&yajr::rpc::method::policy_resolve>::process() const {
    HANDLE_REQ(PolicyResolveReq);
}
template<>
void InbRes<&yajr::rpc::method::policy_resolve>::process() const {
    HANDLE_RES(PolicyResolveRes);
}
template<>
void InbErr<&yajr::rpc::method::policy_resolve>::process() const {
    HANDLE_RES(PolicyResolveErr);
}

template<>
void InbReq<&yajr::rpc::method::policy_unresolve>::process() const {
    HANDLE_REQ(PolicyUnresolveReq);
}
template<>
void InbRes<&yajr::rpc::method::policy_unresolve>::process() const {
    HANDLE_RES(PolicyUnresolveRes);
}
template<>
void InbErr<&yajr::rpc::method::policy_unresolve>::process() const {
    HANDLE_RES(PolicyUnresolveErr);
}

template<>
void InbReq<&yajr::rpc::method::policy_update>::process() const {
    HANDLE_REQ(PolicyUpdateReq);
}
template<>
void InbRes<&yajr::rpc::method::policy_update>::process() const {
    HANDLE_RES(PolicyUpdateRes);
}
template<>
void InbErr<&yajr::rpc::method::policy_update>::process() const {
    HANDLE_RES(PolicyUpdateErr);
}

template<>
void InbReq<&yajr::rpc::method::endpoint_declare>::process() const {
    HANDLE_REQ(EPDeclareReq);
}
template<>
void InbRes<&yajr::rpc::method::endpoint_declare>::process() const {
    HANDLE_RES(EPDeclareRes);
}
template<>
void InbErr<&yajr::rpc::method::endpoint_declare>::process() const {
    HANDLE_RES(EPDeclareErr);
}

template<>
void InbReq<&yajr::rpc::method::endpoint_undeclare>::process() const {
    HANDLE_REQ(EPUndeclareReq);
}
template<>
void InbRes<&yajr::rpc::method::endpoint_undeclare>::process() const {
    HANDLE_RES(EPUndeclareRes);
}
template<>
void InbErr<&yajr::rpc::method::endpoint_undeclare>::process() const {
    HANDLE_RES(EPUndeclareErr);
}

template<>
void InbReq<&yajr::rpc::method::endpoint_resolve>::process() const {
    HANDLE_REQ(EPResolveReq);
}
template<>
void InbRes<&yajr::rpc::method::endpoint_resolve>::process() const {
    HANDLE_RES(EPResolveRes);
}
template<>
void InbErr<&yajr::rpc::method::endpoint_resolve>::process() const {
    HANDLE_RES(EPResolveErr);
}

template<>
void InbReq<&yajr::rpc::method::endpoint_unresolve>::process() const {
    HANDLE_REQ(EPUnresolveReq);
}
template<>
void InbRes<&yajr::rpc::method::endpoint_unresolve>::process() const {
    HANDLE_RES(EPUnresolveRes);
}
template<>
void InbErr<&yajr::rpc::method::endpoint_unresolve>::process() const {
    HANDLE_RES(EPUnresolveErr);
}

template<>
void InbReq<&yajr::rpc::method::endpoint_update>::process() const {
    HANDLE_REQ(EPUpdateReq);
}
template<>
void InbRes<&yajr::rpc::method::endpoint_update>::process() const {
    HANDLE_RES(EPUpdateRes);
}
template<>
void InbErr<&yajr::rpc::method::endpoint_update>::process() const {
    HANDLE_RES(EPUpdateErr);
}

template<>
void InbReq<&yajr::rpc::method::state_report>::process() const {
    HANDLE_REQ(StateReportReq);
}
template<>
void InbRes<&yajr::rpc::method::state_report>::process() const {
    HANDLE_RES(StateReportRes);
}
template<>
void InbErr<&yajr::rpc::method::state_report>::process() const {
    HANDLE_RES(StateReportErr);
}

template<>
void InbReq<&yajr::rpc::method::custom>::process() const {
    HANDLE_REQ_BASE(CustomReq);
}
template<>
void InbRes<&yajr::rpc::method::custom>::process() const {
    HANDLE_RES_BASE(CustomRes);
}
template<>
void InbErr<&yajr::rpc::method::custom>::process() const {
    HANDLE_RES_BASE(CustomErr);
}

/*
 * method implementations for JSON/RPC messages not used with
 * opflex protocol.
 */
template<>
void InbRes<&yajr::rpc::method::transact>::process() const {
    VLOG(5) << "calling InbRes transact";
    ((opflex::engine::internal::OvsdbConnection*)getPeer()->getData())
            ->handleTransaction(getLocalId().id_, getPayload());
}

template<>
void InbReq<&yajr::rpc::method::transact>::process() const {
    VLOG(5) << "calling InbReq transact";
    getPeer()->getData();
}

template<>
void InbErr<&yajr::rpc::method::transact>::process() const {
    VLOG(5) << "calling InbErr transact";
}


} /* namespace rpc */
} /* namespace yajr */
