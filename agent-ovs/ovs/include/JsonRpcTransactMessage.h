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
enum class OvsdbOperation {SELECT, INSERT, UPDATE};

/**
 * OVSDB tables
 */
enum class OvsdbTable {PORT, INTERFACE, BRIDGE, IPFIX, NETFLOW, MIRROR};

/**
 * Data template for JSON/RPC data representation
 */
template<typename T>
class DValue {
public:
    /**
     * constructor for data type class
     * @param val_ type T for data type value to be stored
     */
    DValue(T val_) : val(val_), type(Dtype::STRING) {}
    /**
     * data of type T
     */
    T val;
    /**
     * data type enum
     */
    Dtype type;
};

/**
 * specialization of data template for string
 */
template<> class DValue<string> {
public:
    /**
     * default constructor
     */
    DValue() : type(Dtype::STRING) {}
    /**
     * constructor
     * @param val_ string data value
     */
    DValue(const string& val_) : val(val_), type(Dtype::STRING) {}
    /**
     * string data
     */
    string val;
    /**
     * data type
     */
    Dtype type;
};

/**
 * specialization of data template for int
 */
template<> class DValue<int> {
public:
    /**
     * default constructor
     */
    DValue() : val(0), type(Dtype::INTEGER) {}
    /**
     * constructor
     * @param val_ int value of data
     */
    DValue(int val_) : val(val_), type(Dtype::INTEGER) {}
    /**
     * int data
     */
    int val;
    /**
     * data type enum
     */
    Dtype type;
};

/**
 * specialization of data template for bool
 */
template<> class DValue<bool> {
public:
    /**
     * default constructor
     */
    DValue() : val(false), type(Dtype::BOOL) {}
    /**
     * constructor for bool data type
     * @param val_ bool data type
     */
    DValue(bool val_) : val(val_), type(Dtype::BOOL) {}
    /**
     * bool data type
     */
    bool val;
    /**
     * data type enum
     */
    Dtype type;
};

/**
 * abstract class for data types.
 */
class BaseData {
public:
    /**
     * get the type of data
     * @return enum Dtype
     */
    virtual Dtype getType() = 0;
    /**
     * virtual destructor
     */
    virtual ~BaseData() {}
};
 /**
  * Class to represent JSON/RPC tuple data.
  */
template<typename T>
class TupleData : public BaseData {
public:
    /**
     * constructor
     * @param key the key string
     * @param val data type T
     */
    TupleData(string key, T val) {
        data = make_tuple<string, DValue<T>>(std::move(key), DValue<T>(val));
    }

    /**
     * Destructor
     */
    virtual ~TupleData() {}

    /**
     * get the data type
     * @return enum Dtype
     */
    virtual Dtype getType() {
        return get<1>(data).type;
    }

    /**
     * data stored as tuple of key value pair
     */
    tuple<string, DValue<T>> data;
};

/**
 * class for representing JSON/RPC tuple data set
 */
class TupleDataSet  {
public:
    /**
     * constructor that takes a tuple
     */
    TupleDataSet(set<shared_ptr<BaseData>>& m, string l = "") :
        label(l) {
        tset.insert(m.begin(), m.end());
    }

    virtual ~TupleDataSet() {}

    /**
     * label for collection type, viz. map, set
     */
    string label;
    /**
     * tuple data
     */
    set<shared_ptr<BaseData>> tset;
};

/**
 * struct representing row data for JSON/RPC requests
 */
typedef map<string, shared_ptr<TupleDataSet>> row_map;

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
    set<tuple<string, string, string>> conditions;

    /**
     * set of columns in table
     */
    set<string> columns;
    /**
     * map of row data
     */
    row_map rows;
    /**
     * key value pairs
     */
    set<shared_ptr<BaseData>> kvPairs;

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
