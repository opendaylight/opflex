/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for InspectorServerHandler
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

#include <sstream>

#include <boost/foreach.hpp>

#include "opflex/logging/internal/logging.hpp"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/InspectorServerHandler.h"
#include "opflex/engine/Inspector.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using rapidjson::Writer;
using ofcore::OFConstants;

void InspectorServerHandler::connected() {

}

void InspectorServerHandler::disconnected() {

}

void InspectorServerHandler::ready() {

}

class PolicyQueryRes : public OpflexMessage {
public:
    PolicyQueryRes(const rapidjson::Value& id,
                   Inspector* inspector_,
                   const std::vector<modb::reference_t>& mos_,
                   bool recursive_)
        : OpflexMessage("custom", RESPONSE, &id),
          inspector(inspector_),
          mos(mos_), recursive(recursive_) {}

    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual PolicyQueryRes* clone() {
        return new PolicyQueryRes(*this);
    }

    template <typename T>
    bool operator()(Writer<T> & writer) {
        MOSerializer& serializer = inspector->getSerializer();
        modb::mointernal::StoreClient& client =
            inspector->getStore().getReadOnlyStoreClient();

        writer.StartObject();
        writer.String("method");
        writer.String("org.opendaylight.opflex.policy_query");
        writer.String("result");
        writer.StartObject();
        writer.String("policy");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, mos) {
            try {
                serializer.serialize(p.first, p.second,
                                     client, writer,
                                     recursive);
            } catch (std::out_of_range e) {
                // policy doesn't exist locally
            }
        }
        writer.EndArray();
        writer.EndObject();
        writer.EndObject();
        return true;
    }

    Inspector* inspector;
    std::vector<modb::reference_t> mos;
    bool recursive;
};

void InspectorServerHandler::handlePolicyQueryReq(const Value& id,
                                                  const Value& payload) {
    Value::ConstValueIterator it;
    std::vector<modb::reference_t> mos;
    bool recursive = false;
    
    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", "Malformed message: not an object");
            return;
        }
        if (!it->HasMember("subject")) {
            sendErrorRes(id, "ERROR", "Malformed message: no endpoint");
            return;
        }

        const Value& subjectv = (*it)["subject"];
        const Value& recursivev = (*it)["recursive"];
        if (!subjectv.IsString()) {
            sendErrorRes(id, "ERROR",
                         "Malformed message: subject is not a string");
            return;
        }
        if (recursivev.IsBool()) {
            recursive = recursivev.GetBool();
        }

        try {
            const modb::ClassInfo& ci =
                inspector->db->getClassInfo(subjectv.GetString());
            if (it->HasMember("policy_uri")) {
                const Value& puriv = (*it)["policy_uri"];
                if (!puriv.IsString()) {
                    sendErrorRes(id, "ERROR",
                                 "Malformed message: policy_uri is not a string");
                    return;
                }
                modb::URI puri(puriv.GetString());
                modb::reference_t mo(ci.getId(), puri);
                mos.push_back(mo);
            } else {
                OF_UNORDERED_SET<modb::URI> uris;
                inspector->db->getReadOnlyStoreClient()
                    .getObjectsForClass(ci.getId(), uris);
                BOOST_FOREACH(const modb::URI& uri, uris) {
                    modb::reference_t mo(ci.getId(), uri);
                    mos.push_back(mo);
                }
            }
        } catch (std::out_of_range e) {
            sendErrorRes(id, "ERROR",
                         std::string("Unknown subject: ") +
                         subjectv.GetString());
            return;
        }
    }

    PolicyQueryRes* res =
        new PolicyQueryRes(id, inspector, mos, recursive);
    getConnection()->sendMessage(res, true);
}

const std::string
InspectorServerHandler::POLICY_QUERY("org.opendaylight.opflex.policy_query");

void InspectorServerHandler::handleCustomReq(const Value& id,
                                             const Value& payload) {
    Value::ConstValueIterator it;

    for (it = payload.Begin(); it != payload.End(); ++it) {
        if (!it->IsObject()) {
            sendErrorRes(id, "ERROR", "Malformed custom message: not an object");
            continue;
        }
        if (!it->HasMember("method")) {
            sendErrorRes(id, "ERROR", "Malformed custom message: no method");
            continue;
        }
        if (!it->HasMember("params")) {
            sendErrorRes(id, "ERROR", "Malformed custom message: no params");
            continue;
        }
        const Value& methodv = (*it)["method"];
        const Value& paramsv = (*it)["params"];

        if (!methodv.IsString()) {
            sendErrorRes(id, "ERROR",
                         "Malformed custom message: method is not a string");
            continue;
        }
        if (!paramsv.IsArray()) {
            sendErrorRes(id, "ERROR",
                         "Malformed custom message: params is not an array");
            continue;
        }
        if (POLICY_QUERY == methodv.GetString()) {
            handlePolicyQueryReq(id, paramsv);
        } else {
            sendErrorRes(id, "ERROR",
                         "Malformed custom message: unknown method: " +
                         std::string(methodv.GetString()));
        }
    }
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
