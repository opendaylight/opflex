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

#pragma once
#ifndef RPC_JSONRPC_H
#define RPC_JSONRPC_H

#include<set>
#include<map>
#include<list>

#include <condition_variable>
#include <mutex>

#include <rapidjson/document.h>
#include <boost/lexical_cast.hpp>

namespace opflex {
namespace engine {
namespace internal {

using namespace std;
using namespace rapidjson;
 /**
  * Abstract base class to represent JSON/RPC data.
  */
class BaseData {
public:
    /**
     * enum for row data type
     */
    enum class Dtype {STRING, TUPLE};
    /**
     * data type
     */
    Dtype type;
    /**
     * string representation of object
     */
    virtual string toString() = 0;
};

/**
 * class for representing JSON/RPC string data
 */
class StringData : public BaseData {
public:
    /**
     * constructor takes a string
     */
    StringData(const string& s) : data(s) { type = Dtype::STRING;}
    /**
     * data holder
     */
    string data;
    /**
     * string representation of object
     */
    string toString() { return data;}
};

/**
 * class for representing JSON/RPC tuple data
 */
class TupleData : public BaseData {
public:
    /**
     * constructor that takes a tuple
     */
    TupleData(set<tuple<string, string>>& m, string l = "") :
        label(l) {
        type = Dtype::TUPLE;
        tset.insert(m.begin(), m.end());
    }
    /**
     * label for collection type, viz. map, set
     */
    string label;
    /**
     * tuple data
     */
    set<tuple<string, string>> tset;
    /**
     * string representation of object
     */
    string toString() {
        stringstream ss;
        ss << "label " << label << endl;
        for (auto elem : tset) {
            ss << std::get<0>(elem) << " : " << get<1>(elem) << endl;
        }
        return ss.str();
    }
};

/**
 * struct representing row data for JSON/RPC requests
 */
typedef map<string, shared_ptr<BaseData>> row_map;

/**
 * struct used for setting up a JSON/RPC request.
 */
typedef struct transData_ {
    /**
     * set of tuple of data to be mapped to rows
     */
    set<tuple<string, string, string>> conditions;
    /**
     * operation type, E.g select, insert.
     */
    string operation;
    /**
     * table name
     */
    string table;
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
    map<string, string> kvPairs;
    /**
     * default constructor
     */
    transData_() {};
    /**
     * constructor with data
     */
    transData_(const transData_& td) : conditions(td.conditions),
            operation(td.operation), table(td.table),
            columns(td.columns), rows(td.rows), kvPairs(td.kvPairs) {};
} transData;

/**
 * JSON/RPC transaction base class
 */
class Transaction {
public:
    /**
     * pure virtual method for handling transactions
     */
    virtual void handleTransaction(uint64_t reqId,
            const rapidjson::Value& payload) = 0;
};

/**
 * class for managing RPC connection to a server.
 */
class RpcConnection {
    public:
    /**
     * constructor that takes a pointer to a Transaction object
     */
    RpcConnection(Transaction* pTrans_) : pTrans(pTrans_) {}

    /**
     * call back for transaction response
     * @param[in] reqId request ID of the request for this response.
     * @param[in] payload rapidjson::Value reference of the response body.
     */
    virtual void handleTransaction(uint64_t reqId,
                const rapidjson::Value& payload);

    /**
     * initialize the module
     */
    virtual void start() = 0;

    /**
     * stop the module
     */
    virtual void stop() = 0;

    /**
     * create a tcp connection to peer
     * @param[in] hostname host name of the peer.
     * @param[in] port port number to connect to.
     */
    virtual void connect(string const& hostname, int port) = 0;
    /**
     * get state of connection
     * @return true if connected, false otherwise
     */
    bool isConnected() { return connected;};

    /**
     * set connection state
     * @param[in] state state of connection
     */
    void setConnected(bool state) {
        connected = state;
    }

    /**
     * send transaction request
     * @param[in] tl list of transData objects
     * @param[in] reqId request ID
     */
    virtual void sendTransaction(const list<transData>& tl, const uint64_t& reqId) = 0;

    /**
     * condition variable used for synchronizing JSON/RPC
     * request and response
     */
    condition_variable ready;
    /**
     * mutex used for synchronizing JSON/RPC
     * request and response
     */
    mutex mtx;

    /**
     * boolean flag to indicate connection state.
     */
    bool connected = false;
    /**
     * pointer to a Transaction object instance
     */
    Transaction* pTrans;

};

/**
 * create an RPC connection to a server
 * @param[in] trans a reference to a Transaction object instance
 * @return shared pointer to an RpcConnection object
 */
shared_ptr<RpcConnection> createConnection(Transaction& trans);

/**
 * helper function to get Value of a given index
 * @param[in] val rapidjson Value object
 * @param[in] idx list of strings representing indices
 * @return Value object
 */
Value getValue(const Value& val, const list<string>& idx);
}
}
}
#endif
