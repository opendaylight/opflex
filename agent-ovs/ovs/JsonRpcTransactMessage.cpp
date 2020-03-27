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

namespace opflexagent {

JsonRpcTransactMessage::JsonRpcTransactMessage(OvsdbOperation operation_, OvsdbTable table_) : JsonRpcMessage("transact", REQUEST),
    operation(operation_), table(table_) {}

JsonRpcTransactMessage::JsonRpcTransactMessage(const JsonRpcTransactMessage& copy) : JsonRpcMessage("transact", REQUEST),
    conditions(copy.conditions), columns(copy.columns), rows(copy.rows), kvPairs(copy.kvPairs),
    operation(copy.getOperation()), table(copy.getTable()) {}

void JsonRpcTransactMessage::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

static const char* OvsdbOperationStrings[] = {"select", "insert", "update"};

static const char* toString(OvsdbOperation operation) {
    return OvsdbOperationStrings[static_cast<uint32_t>(operation)];
}

static const char* OvsdbTableStrings[] = {"Port", "Interface", "Bridge", "IPFIX", "NetFlow", "Mirror"};

static const char* toString(OvsdbTable table) {
    return OvsdbTableStrings[static_cast<uint32_t>(table)];
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
    for (auto& pair : kvPairs) {
        writePair<T>(writer, pair, true);
    }

    if (getOperation() != OvsdbOperation::INSERT) {
        writer.String("where");
        writer.StartArray();
        if (!conditions.empty()) {
            for (auto elem : conditions) {
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
    writer.String(toString(getTable()));
    writer.String("op");
    writer.String(toString(getOperation()));
    if (!columns.empty()) {
        writer.String("columns");
        writer.StartArray();
        for (auto& tmp : columns) {
            writer.String(tmp.c_str());
        }
        writer.EndArray();
    }

    if (!rows.empty()) {
        writer.String("row");
        writer.StartObject();
        for (auto& row : rows) {
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

TransactReq::TransactReq(const list<JsonRpcTransactMessage>& msgs, uint64_t reqId)
    : JsonRpcMessage("transact", REQUEST), reqId(reqId) {
    transList = msgs;
}

void TransactReq::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

}