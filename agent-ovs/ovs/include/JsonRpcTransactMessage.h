/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file JsonRpcTransactMessage.h
 * @brief Interface definition for JSON-RPC transact messages used by the
 * engine
 */
/*
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_JSONRPCTRANSACTMESSAGE_H
#define OPFLEX_JSONRPCTRANSACTMESSAGE_H

#include <rapidjson/document.h>
#include <opflex/rpc/JsonRpcMessage.h>
#include <opflexagent/logging.h>
#include <unordered_map>

namespace opflexagent {

using namespace std;
using namespace rapidjson;

/**
 * enum for data types to be sent over JSON/RPC
 */
enum class Dtype {STRING, INTEGER, BOOL};

/**
 * OVSDB operations
 */
enum class OvsdbOperation {SELECT, INSERT, UPDATE, MUTATE, DELETE};

/**
 * OVSDB tables
 */
enum class OvsdbTable {PORT, INTERFACE, BRIDGE, IPFIX, NETFLOW, MIRROR};

/**
 * OVSDB functions
 */
enum class OvsdbFunction {EQ};

/**
 * Class to represent JSON/RPC tuple data.
 */
class TupleData {
public:
    /**
     * constructor
     * @param key_ the key string
     * @param val value
     */
    TupleData(const string& key_, const string& val) : key(key_), type(Dtype::STRING), sVal(val), iVal(-1), bVal(false) {}

    /**
     * constructor
     * @param key_ the key string
     * @param val value
     */
    TupleData(const string& key_, bool val) : key(key_), type(Dtype::BOOL), iVal(-1), bVal(val) {}
    /**
     * constructor
     * @param key_ the key string
     * @param val value
     */
    TupleData(const string& key_, int val) : key(key_), type(Dtype::INTEGER), iVal(val), bVal(false) {}

    /**
     * Copy constructor
     *
     * @param copy Object to copy from
     */
    TupleData(const TupleData& copy) : key(copy.key), type(copy.type), sVal(copy.sVal), iVal(copy.iVal), bVal(copy.bVal) {}

    /**
     * Destructor
     */
    virtual ~TupleData() {}

    /** Get key */
    const string& getKey() const {
        return key;
    }

    /**
     * get the data type
     * @return enum Dtype
     */
     Dtype getType() const {
        return type;
    }

    /**
     * Get the value when set to string type
     */
    const string& getStringValue() const {
         return sVal;
     }

    /**
     * Get the value when set to bool type
     */
     bool getBoolValue() const {
         return bVal;
     }

    /**
     * Get the value when set to int type
     */
     int getIntValue() const {
         return iVal;
     }

private:
    string key;
    Dtype type;
    string sVal;
    int iVal;
    bool bVal;
};

/**
 * class for representing JSON/RPC tuple data set
 */
class TupleDataSet {
public:
    /**
     * Default constructor
     */
    TupleDataSet() {}
    /**
     * Copy constructor
     */
    TupleDataSet(const TupleDataSet& s) : label(s.label), tuples(s.tuples) {}

    /**
     * constructor that takes a tuple
     */
    TupleDataSet(const vector<TupleData>& m, string l = "") : label(l), tuples(m) {}

    virtual ~TupleDataSet() {}

    /**
     * label for collection type, viz. map, set
     */
    string label;
    /**
     * tuple data
     */
    vector<TupleData> tuples;
};

/**
 * Transact message
 */
class JsonRpcTransactMessage : public opflex::jsonrpc::JsonRpcMessage {
public:
    /**
     * Construct a transact request
     */
    JsonRpcTransactMessage(OvsdbOperation operation_, OvsdbTable table_);

    /**
     * Copy constructor
     */
     JsonRpcTransactMessage(const JsonRpcTransactMessage& copy);

     /**
      * Destructor
      */
     virtual ~JsonRpcTransactMessage() {};

    /**
     * Serialize payload
     * @param writer writer
     */
    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    /**
     * Clone the transact message
     */
    virtual JsonRpcTransactMessage* clone(){
        return new JsonRpcTransactMessage(*this);
    }

    /**
     * Operator to serialize a payload to a writer
     * @param writer the writer to serialize to
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer);


    /**
     * operation type, E.g select, insert.
     */
    OvsdbOperation getOperation() const {
        return operation;
    }

    /**
     * table name
     */
    OvsdbTable getTable() const {
        return table;
    }

    /**
     * set of tuple of data to be mapped to rows
     */
    set<tuple<string, OvsdbFunction, string>> conditions;

    /**
     * set of columns in table
     */
    set<string> columns;
    /**
     * map of row data
     */
    unordered_map<string, TupleDataSet> rowData;
    /**
     * mutate row data
     */
    unordered_map<string, std::pair<OvsdbOperation, TupleDataSet>> mutateRowData;
    /**
     * key value pairs
     */
    vector<TupleData> kvPairs;

private:
    OvsdbOperation operation;
    OvsdbTable table;
};

/**
 * JSON/RPC transaction message
 */
class TransactReq : public opflex::jsonrpc::JsonRpcMessage {
public:
    /**
     * Construct a TransactReq instance
     * @param tl transaction data
     * @param reqId request ID
     */
    TransactReq(const list<JsonRpcTransactMessage>& tl, uint64_t reqId);

    /**
     * Destructor
     */
    virtual ~TransactReq() {};

    /**
     * Serialize payload
     * @param writer writer
     */
    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    /**
     * Clone a request
     * @return clone
     */
    virtual TransactReq* clone(){
        return new TransactReq(*this);
    }

    /**
     * Get request ID
     * @return request ID
     */
    uint64_t getReqId() {
        return reqId;
    }

    /**
     * Operator to serialize OVSDB transaction
     * @tparam T Type
     * @param writer writer
     * @return
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) const {
        writer.StartArray();
        writer.String("Open_vSwitch");
        for (auto tr : transList) {
            writer.StartObject();
            tr.serializePayload(writer);
            writer.EndObject();
        }
        writer.EndArray();
        return true;
    }

private:
    list<JsonRpcTransactMessage> transList;
    uint64_t reqId;
};

}

#endif //OPFLEX_JSONRPCTRANSACTMESSAGE_H
