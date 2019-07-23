/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file JsonRpc.h
 * @brief Interface definition for various JSON/RPC messages used by the
 * engine
 */
/*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/engine/internal/OpflexMessage.h"
#include <condition_variable>
#include <mutex>

#pragma once
#ifndef OPFLEX_ENGINE_JSONRPC_H
#define OPFLEX_ENGINE_JSONRPC_H

namespace opflex {
namespace engine {
namespace internal {

using namespace std;

/*
 * JSON/RPC transaction message
 */
class TransactReq : public OpflexMessage {
public:
    TransactReq(const string& oper,
             const string& tab,
             const set<tuple<string, string, string>> cond);

//    virtual ~TransactReq() {};
//    virtual uint64_t getReqXid() { return xid; }

//    TransactReq(const TransactReq* req);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    virtual void serializePayload(MessageWriter& writer);

    virtual TransactReq* clone(){
        return new TransactReq(*this);
    }

    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        writer.StartArray();
        writer.String("Open_vSwitch");
        if (!conditions.empty()) {
            writer.StartObject();
            writer.String("where");
            writer.StartArray();
            for (auto elem : conditions) {
                writer.StartArray();
                string lhs = get<0>(elem);
                writer.String(lhs.c_str());
                string condition = get<1>(elem);
                writer.String(condition.c_str());
                string rhs = get<2>(elem);
                if (lhs == "_uuid") {
                    writer.StartArray();
                    writer.String("uuid");
                    writer.String(rhs.c_str());
                    writer.EndArray();
                } else {
                    writer.String(rhs.c_str());
                }
                writer.EndArray();
            }
            writer.EndArray();
        }

        writer.String("table");
        writer.String(table.c_str());
        writer.String("op");
        writer.String(operation.c_str());
        if (!columns.empty()) {
            writer.String("columns");
            writer.StartArray();
            for (set<string>::iterator it = columns.begin();
                    it != columns.end(); ++it) {
                string tmp = *it;
                writer.String(tmp.c_str());
            }
            writer.EndArray();
        }
        if (!rows.empty()) {
            writer.String("row");
            writer.StartObject();
            for (auto itr : rows) {
                string col = itr.first;
                writer.String(col.c_str());
                string type = "";
                if (table == "Bridge" && col == "ports") {
                    type = "uuid";
                }
                if (get<0>(itr.second) == "set") {
                    writer.StartArray();
                    writer.String("set");
                    writer.StartArray();
                    for (auto val : get<1>(itr.second)) {
                        if (type == "uuid") {
                            writer.StartArray();
                            writer.String("uuid");
                            writer.String(val.c_str());
                            writer.EndArray();
                        } else {
                            writer.String(val.c_str());
                        }
                    }
                    writer.EndArray();
                    writer.EndArray();
                } else {
                    writer.String(get<1>(itr.second).front().c_str());
                }
            }
            writer.EndObject();
        }
        writer.EndObject();
        writer.EndArray();


        return true;
    }
protected:
//    uint64_t xid;
public:
    set<tuple<string, string, string>> conditions;
    string operation;
    string table;
    set<string> columns;
    map<string, tuple<string, list<string>>> rows;
    std::function<void()> ResponseCb();

};

class TransactResp {
public:
    uint64_t reqId;
    const rapidjson::Value* payload;
    std::condition_variable cv;
    std::mutex respMutex;
};

class Transaction {
public:
    virtual void handleTransaction(uint64_t reqId,
            const rapidjson::Value& payload) = 0;
    TransactReq* req;
    TransactResp* resp;
};

void sendTransaction(TransactReq& req, yajr::Peer *p, uint64_t reqId);

}
}
}
#endif
