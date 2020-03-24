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
    DValue(T val_) : val(val_) {}
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
    DValue() {}
    /**
     * constructor
     * @param val_ string data value
     */
    DValue(const string& val_) : val(val_) { type = Dtype::STRING; }
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
    DValue() {}
    /**
     * constructor
     * @param val_ int value of data
     */
    DValue(int val_) : val(val_) { type = Dtype::INTEGER; }
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
    DValue() {}
    /**
     * constructor for bool data type
     * @param val_ bool data type
     */
    DValue(bool val_) : val(val_) { type = Dtype::BOOL; }
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
     * string representation of object
     */
    string toString() {
        stringstream ss;
        ss << std::get<0>(data) << " : " << get<1>(data).val << endl;
        return ss.str();
    }

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
    /**
     * label for collection type, viz. map, set
     */
    string label;
    /**
     * tuple data
     */
    set<shared_ptr<BaseData>> tset;
    /**
     * string representation of object
     */
    string toString() {
        stringstream ss;
        ss << "label " << label << endl;
        for (auto& elem : tset) {
            if (elem->getType() == Dtype::INTEGER) {
                shared_ptr<TupleData<int>> tPtr =
                        dynamic_pointer_cast<TupleData<int>>(elem);
                ss << get<0>(tPtr->data) << " : " << get<1>(tPtr->data).val << endl;
            } else if (elem->getType() == Dtype::STRING) {
                shared_ptr<TupleData<string>> tPtr =
                        dynamic_pointer_cast<TupleData<string>>(elem);
                ss << get<0>(tPtr->data) << " : " << get<1>(tPtr->data).val << endl;
            } else if (elem->getType() == Dtype::BOOL) {
                shared_ptr<TupleData<bool>> tPtr =
                        dynamic_pointer_cast<TupleData<bool>>(elem);
                ss << get<0>(tPtr->data) << " : " << get<1>(tPtr->data).val << endl;
            }
        }
        return ss.str();
    }
};

/**
 * struct representing row data for JSON/RPC requests
 */
typedef map<string, shared_ptr<TupleDataSet>> row_map;

/**
 * struct used for setting up a JSON/RPC request.
 */
class TransData {
public:
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

    /**
     * constructor with data
     */
    TransData(OvsdbOperation operation_, OvsdbTable table_) :
        operation(operation_), table(table_) {};

    /**
     * copy constructor
     */
    TransData(const TransData& td) :  conditions(td.conditions),
            columns(td.columns), rows(td.rows), kvPairs(td.kvPairs),
            operation(td.operation), table(td.table){};

    /**
     * operation type, E.g select, insert.
     */
    OvsdbOperation getOperation() {
        return operation;
    }

    /**
     * table name
     */
    OvsdbTable getTable() {
        return table;
    }

private:
    OvsdbOperation operation;
    OvsdbTable table;
};

/**
 * Transact message
 */
class JsonRpcTransactMessage : public opflex::jsonrpc::JsonRpcMessage {
public:
    /**
     * Constract a transaction request
     * @param td transaction data
     */
    JsonRpcTransactMessage(const TransData& td);

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

private:

    TransData tData;
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
    TransactReq(const list<TransData>& tl, uint64_t reqId);

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
    bool operator()(rapidjson::Writer<T> & writer) {
        writer.StartArray();
        writer.String("Open_vSwitch");
        for (shared_ptr<JsonRpcTransactMessage>& tr : transList) {
            writer.StartObject();
            (*tr)(writer);
            writer.EndObject();
        }
        writer.EndArray();
        return true;
    }

private:
    list<shared_ptr<JsonRpcTransactMessage>> transList;
    uint64_t reqId;
};

}

#endif //OPFLEX_JSONRPCTRANSACTMESSAGE_H
