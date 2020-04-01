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
void writePair(rapidjson::Writer<T>& writer, const TupleData& bPtr, bool kvPair) {
    const string& key = bPtr.getKey();
    if (bPtr.getType() == Dtype::INTEGER) {
        if (kvPair) {
            writer.String(key.c_str());
            writer.Int(bPtr.getIntValue());
        } else {
            if (!key.empty()) {
                writer.StartArray();
                writer.String(key.c_str());
            }
            writer.Int(bPtr.getIntValue());
            if (!key.empty()) {
                writer.EndArray();
            }
        }
    } else if (bPtr.getType() == Dtype::STRING) {
        if (kvPair) {
            writer.String(key.c_str());
            writer.String(bPtr.getStringValue().c_str());
        } else {
            if (!key.empty()) {
                writer.StartArray();
                writer.String(key.c_str());
            }
            writer.String(bPtr.getStringValue().c_str());
            if (!key.empty()) {
                writer.EndArray();
            }
        }
    } else if (bPtr.getType() == Dtype::BOOL) {
        if (kvPair) {
            writer.String(key.c_str());
            writer.Bool(bPtr.getBoolValue());
        } else {
            if (!key.empty()) {
                writer.StartArray();
                writer.String(key.c_str());
            }
            writer.Bool(bPtr.getBoolValue());
            if (!key.empty()) {
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
            const TupleDataSet& tdsPtr = row.second;
            if (!tdsPtr.label.empty()) {
                writer.StartArray();
                writer.String(tdsPtr.label.c_str());
                writer.StartArray();
                LOG(DEBUG) << "label " << tdsPtr.label;

                for (auto& val : tdsPtr.tuples) {
                    writePair<T>(writer, val, false);
                }
                writer.EndArray();
                writer.EndArray();
            } else {
                writePair(writer, *(tdsPtr.tuples.begin()), false);
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