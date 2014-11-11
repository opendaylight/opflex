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

#include <vector>
#include <utility>

#include <boost/foreach.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/Processor.h"

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

void OpflexHandler::handleUnexpected(const rapidjson::Value& id,
                                     const string& type) {
    LOG(ERROR) << "Unexpected message of type " << type;
    sendErrorRes(id, "ESTATE", "Unexpected message");
    conn->disconnect();
}

void OpflexHandler::handleUnsupportedReq(const rapidjson::Value& id,
                                         const string& type) {
    LOG(WARNING) << "Ignoring unsupported request of type " << type;
    sendErrorRes(id, "EUNSUPPORTED", "Unsupported request");
}

void OpflexHandler::handleError(const rapidjson::Value& id,
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
    LOG(ERROR) << "Remote peer returned error with message (" << type
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

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(Writer<T> & handler) {
        handler.StartObject();
        handler.String("code");
        handler.String(code.c_str());
        handler.String("message");
        handler.String(message.c_str());
        return true;
    }

    const string& code;
    const string& message;
};

void OpflexHandler::sendErrorRes(const Value& id,
                                 const string& code,
                                 const string& message) {
    ErrorRes res(id, code, message);
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    id.Accept(writer);
    LOG(ERROR) << "Error processing message " << sb.GetString()
               << ": " << code << ": " << message;
    getConnection()->write(res.serialize());
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

namespace yajr {
namespace rpc {

// Following are definitions that forward the message handlers from
// the opflex RPC library into our own handler

template<>
void InbReq<&yajr::rpc::method::send_identity>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::send_identity>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::send_identity>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::policy_resolve>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::policy_resolve>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::policy_resolve>::process() const {

}


template<>
void InbReq<&yajr::rpc::method::policy_unresolve>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::policy_unresolve>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::policy_unresolve>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::policy_update>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::policy_update>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::policy_update>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::endpoint_declare>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::endpoint_declare>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::endpoint_declare>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::endpoint_undeclare>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::endpoint_undeclare>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::endpoint_undeclare>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::endpoint_resolve>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::endpoint_resolve>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::endpoint_resolve>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::endpoint_unresolve>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::endpoint_unresolve>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::endpoint_unresolve>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::endpoint_update>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::endpoint_update>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::endpoint_update>::process() const {

}

template<>
void InbReq<&yajr::rpc::method::state_report>::process() const {

}
template<>
void InbRes<&yajr::rpc::method::state_report>::process() const {

}
template<>
void InbErr<&yajr::rpc::method::state_report>::process() const {

}


} /* namespace rpc */
} /* namespace yajr */
