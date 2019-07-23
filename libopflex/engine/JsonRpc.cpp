/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of opflex messages for engine
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <set>
#include <tuple>

#include <yajr/rpc/methods.hpp>

#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/ProcessorMessage.h"
#include "opflex/engine/internal/JsonRpc.h"
#include "opflex/logging/internal/logging.hpp"
#include <yajr/yajr.hpp>
#include <yajr/rpc/rpc.hpp>

using  rapidjson::Writer;
using rapidjson::Value;

namespace opflex {
namespace engine {
namespace internal {


TransactReq::TransactReq(const string& oper,
                 const string& tab,
                 const set<tuple<string, string, string>> cond)
            : OpflexMessage("transact", REQUEST), operation(oper), table(tab),
              conditions(cond) {}
/*
TransactReq::TransactReq(const TransactReq* req) {
    TransactReq(req->operation, req->table, req->columns);
}
*/

void TransactReq::serializePayload(yajr::rpc::SendHandler& writer) {
            (*this)(writer);
        }

void TransactReq::serializePayload(MessageWriter& writer) {
            (*this)(writer);
        }



void sendTransaction(TransactReq& req, yajr::Peer *p, uint64_t reqId) {
    VLOG(5) << "send transaction";
        yajr::rpc::MethodName method(req.getMethod().c_str());
        PayloadWrapper wrapper(&req);
        yajr::rpc::OutboundRequest outm(wrapper, &method, reqId, p);
        outm.send();
}
    }
}
}

