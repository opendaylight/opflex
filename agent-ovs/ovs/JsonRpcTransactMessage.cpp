/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of JSON-RPC transact messages
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "JsonRpcTransactMessage.h"
#include <opflexagent/logging.h>

namespace opflexagent {

JsonRpcTransactMessage::JsonRpcTransactMessage(const TransData& td) : JsonRpcMessage("transact", REQUEST),
        tData(td) {}

void JsonRpcTransactMessage::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

template<typename T>
void writePair(rapidjson::Writer<T>& writer, const shared_ptr<BaseData>& bPtr,
        bool kvPair) {
    if (bPtr->getType() == Dtype::INTEGER) {
        shared_ptr<TupleData<int>> tPtr =
                dynamic_pointer_cast<TupleData<int>>(bPtr);
        if (kvPair) {
            writer.String(get<0>(tPtr->data).c_str());
            writer.Int(get<1>(tPtr->data).val);
        } else {
            string str = get<0>(tPtr->data);
            if (!str.empty()) {
                writer.StartArray();
                writer.String(get<0>(tPtr->data).c_str());
            }
            writer.Int(get<1>(tPtr->data).val);
            if (!str.empty()) {
                writer.EndArray();
            }
        }
    } else if (bPtr->getType() == Dtype::STRING) {
        shared_ptr<TupleData<string>> tPtr =
                dynamic_pointer_cast<TupleData<string>>(bPtr);
        if (kvPair) {
            writer.String(get<0>(tPtr->data).c_str());
            writer.String(get<1>(tPtr->data).val.c_str());
        } else {
            string str = get<0>(tPtr->data);
            if (!str.empty()) {
                writer.StartArray();
                writer.String(get<0>(tPtr->data).c_str());
            }
            writer.String(get<1>(tPtr->data).val.c_str());
            if (!str.empty()) {
                writer.EndArray();
            }
        }
    } else if (bPtr->getType() == Dtype::BOOL) {
        shared_ptr<TupleData<bool>> tPtr =
                dynamic_pointer_cast<TupleData<bool>>(bPtr);
        if (kvPair) {
            writer.String(get<0>(tPtr->data).c_str());
            writer.Bool(get<1>(tPtr->data).val);
        } else {
            string str = get<0>(tPtr->data);
            if (!str.empty()) {
                writer.StartArray();
                writer.String(get<0>(tPtr->data).c_str());
            }
            writer.Bool(get<1>(tPtr->data).val);
            if (!str.empty()) {
                writer.EndArray();
            }
        }
    }
}

template <typename T>
bool JsonRpcTransactMessage::operator()(rapidjson::Writer<T> & writer) {
    for (auto& pair : tData.kvPairs) {
        writePair<T>(writer, pair, true);
    }

    if (tData.getOperation() != "insert") {
        writer.String("where");
        writer.StartArray();
        if (!tData.conditions.empty()) {
            for (auto elem : tData.conditions) {
                writer.StartArray();
                string lhs = get<0>(elem);
                writer.String(lhs.c_str());
                string condition = get<1>(elem);
                writer.String(condition.c_str());
                string rhs = get<2>(elem);
                if (lhs == "_uuid" ||
                        lhs == "mirrors") {
                    writer.StartArray();
                    writer.String("uuid");
                    writer.String(rhs.c_str());
                    writer.EndArray();
                } else {
                    writer.String(rhs.c_str());
                }
                writer.EndArray();
            }
        }
        writer.EndArray();
    }
    writer.String("table");
    writer.String(tData.getTable().c_str());
    writer.String("op");
    writer.String(tData.getOperation().c_str());
    if (!tData.columns.empty()) {
        writer.String("columns");
        writer.StartArray();
        for (auto& tmp : tData.columns) {
            writer.String(tmp.c_str());
        }
        writer.EndArray();
    }

    if (!tData.rows.empty()) {
        writer.String("row");
        writer.StartObject();
        for (auto& row : tData.rows) {
            string col = row.first;
            LOG(DEBUG) << "row label " << col;
            writer.String(col.c_str());
            shared_ptr<TupleDataSet> tdsPtr = row.second;
            if (!tdsPtr->label.empty()) {
                writer.StartArray();
                writer.String(tdsPtr->label.c_str());
                writer.StartArray();
                LOG(DEBUG) << "label " << tdsPtr->label;

                for (auto& val : tdsPtr->tset) {
                    writePair<T>(writer, val, false);
                }
                writer.EndArray();
                writer.EndArray();
            } else {
                writePair(writer, *(tdsPtr->tset.begin()), false);
            }
        }
        writer.EndObject();
    }
    return true;
}

JsonReq::JsonReq(const list<TransData>& tl, uint64_t reqId)
    : JsonRpcMessage("transact", REQUEST), reqId(reqId)
{
    for (auto& elem : tl) {
        shared_ptr<JsonRpcTransactMessage> pTr = make_shared<JsonRpcTransactMessage>(elem);
        transList.push_back(pTr);
    }
}

void JsonReq::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

}