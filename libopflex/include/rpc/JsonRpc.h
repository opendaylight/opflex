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
 * utility class to pretty print rapidjson::Value
 * @param val Value object reference
 */
void prettyPrintValue(const rapidjson::Value& val);

/**
 * enum for data types to be sent over JSON/RPC
 */
enum class  Dtype {STRING, INTEGER, BOOL};

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
    DValue(string val_) : val(val_) { type = Dtype::STRING; }
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
        for (auto elem : tset) {
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
    set<shared_ptr<BaseData>> kvPairs;
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
     * destructor
     */
    virtual ~RpcConnection() {}

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
 * class for a mockup of an RpcConnection object
 */
class MockRpcConnection : public RpcConnection {
public:
    /**
     * constructor that takes a Transaction object reference
     */
    MockRpcConnection(Transaction& pTrans_) : RpcConnection(&pTrans_) {}

    /**
     * establish mock connection
     * @param hostname name of host to connect to
     * @param port port to connect to
     */
    void connect(string const& hostname, int port) { connected = true;}

    /**
     * send transaction
     * @param tl list of Transaction objects
     * @param reqId request ID
     */
    void sendTransaction(const list<transData>& tl,
            const uint64_t& reqId);

    /**
     * destructor
     */
    virtual ~MockRpcConnection() {}
private:
    void start() {}
    void stop() {}
};

/**
 * class holding the request response lookup
 */
class ResponseDict {
public:
    /**
     * get the sole instance of this class
     */
    static ResponseDict& Instance();

    /**
     * initialize the object instance
     */
    void init();

protected:
    /**
     * constructor
     */
    ResponseDict() {}

    /**
     * destructor
     */
    ~ResponseDict() {}
public:
    /**
     * flag to indicate initialization state
     */
    bool isInitialized = false;

    /**
     * map of request response pairs
     */
    map<uint64_t, int> dict;
    /**
     * number of responses to send
     */
    static const unsigned int no_of_msgs = 13;
    /**
     * rapidjson Document object array
     */
    Document d[no_of_msgs];
private:

    //string request1 {"[\"Open_vSwitch\",{\"where\":[],\"table\":\"Mirror\",\"op\":\"select\"}]"};
    string response1 {"[{\"rows\":[{\"statistics\":[\"map\",[[\"tx_bytes\",0],[\"tx_packets\",0]]],\
            \"_version\":[\"uuid\",\"ec4c165c-335d-477f-a96b-c37c02d6131b\"],\"select_all\"\
             :false,\"name\":\"msandhu-sess1\",\"output_vlan\":[\"set\",[]],\"select_dst_port\":\
             [\"uuid\",\"0a7a4d65-e785-4674-a219-167391d10c3f\"],\"select_src_port\":[\"set\",\
             [[\"uuid\",\"0a7a4d65-e785-4674-a219-167391d10c3f\"],[\"uuid\",\
             \"373108c7-ce2d-4d46-a419-1654a5bf47ef\"]]],\"external_ids\":[\"map\",[]],\
             \"snaplen\":[\"set\",[]],\"_uuid\":[\"uuid\",\"3f64048e-0abd-4b96-8874-092a527ee80b\"]\
             ,\"output_port\":[\"uuid\",\"fff42dce-44cb-4b6a-8920-dfc32d88ec07\"],\"select_vlan\"\
              :[\"set\",[]]}]}]"};
    //string request2 {"[\"Open_vSwitch\",{\"where\":[],\"table\":\"Port\",\"op\":\"select\",\"columns\":[\"_uuid\",\"name\"]}]"};
    string response2 {"[{\"rows\":[{\"name\":\"br-int\",\"_uuid\":[\"uuid\",\
            \"ffaee0cd-bb7d-4698-9af1-99f57f9b7081\"]},{\"name\":\"erspan\",\"_uuid\":[\"uuid\",\
            \"fff42dce-44cb-4b6a-8920-dfc32d88ec07\"]},{\"name\":\"p1-tap\",\"_uuid\":[\"uuid\",\
            \"0a7a4d65-e785-4674-a219-167391d10c3f\"]},{\"name\":\"p2-tap\",\"_uuid\":[\"uuid\",\
            \"373108c7-ce2d-4d46-a419-1654a5bf47ef\"]}]}]"};
    /*
    string request3 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"erspan\"]],\"table\":\
            \"Port\",\"op\":\"select\"}]"};
            */
    string response3 {"[{\"rows\":[{\"protected\":false,\"statistics\":[\"map\",[]],\
            \"bond_downdelay\":0,\"name\":\"erspan\",\"mac\":[\"set\",[]],\"fake_bridge\":false,\
            \"trunks\":[\"set\",[]],\"_uuid\":[\"uuid\",\"fff42dce-44cb-4b6a-8920-dfc32d88ec07\"],\
            \"rstp_status\":[\"map\",[]],\"tag\":[\"set\",[]],\"_version\":[\"uuid\",\
            \"bbc91b12-a377-4f1e-b7d4-e6499172baac\"],\"cvlans\":[\"set\",[]],\"bond_updelay\":0,\
            \"bond_active_slave\":[\"set\",[]],\"status\":[\"map\",[]],\"external_ids\":[\"map\",[]],\
            \"other_config\":[\"map\",[]],\"qos\":[\"set\",[]],\"bond_mode\":[\"set\",[]],\
            \"rstp_statistics\":[\"map\",[]],\"vlan_mode\":[\"set\",[]],\"interfaces\":[\"uuid\",\
            \"d05435fa-e35c-4661-8402-f5cfe32ca1f3\"],\"bond_fake_iface\":false,\"lacp\":[\"set\",[]]}]}]"};

    /*string request4 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"br-int\"]],\"table\":\"Bridge\",\
            \"op\":\"select\",\"columns\":[\"_uuid\",\"ports\"]}]"};
            */
    string response4 {"[{\"rows\":[{\"ports\":[\"set\",[[\"uuid\",\
            \"0a7a4d65-e785-4674-a219-167391d10c3f\"],[\"uuid\",\
            \"373108c7-ce2d-4d46-a419-1654a5bf47ef\"],[\"uuid\",\
            \"ffaee0cd-bb7d-4698-9af1-99f57f9b7081\"],[\"uuid\",\
            \"fff42dce-44cb-4b6a-8920-dfc32d88ec07\"]]],\"_uuid\":[\"uuid\",\
            \"7cb323d7-0215-406d-ae1d-679b72e1f6aa\"]}]}]"};
    /*
    string request5 {"[\"Open_vSwitch\",{\"where\":[[\"_uuid\",\"==\",\
            [\"uuid\",\"7cb323d7-0215-406d-ae1d-679b72e1f6aa\"]]],\"table\":\"Bridge\",\"op\":\
            \"update\",\"row\":{\"ports\":[\"set\",[[\"uuid\",\
            \"0a7a4d65-e785-4674-a219-167391d10c3f\"],[\"uuid\",\
            \"373108c7-ce2d-4d46-a419-1654a5bf47ef\"],[\"uuid\",\
            \"ffaee0cd-bb7d-4698-9af1-99f57f9b7081\"]]]}}]"};
            */
    string response5 {"[{\"count\":1}]"};
    /*
    string request6 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"br-int\"]],\"table\":\
            \"Bridge\",\"op\":\"update\",\"row\":{\"mirrors\":[\"set\",[]]}}]"};
            */
    string response6 {"[{\"count\":1}]"};
    /*
    string request7 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"br-int\"]],\"table\
            \":\"Bridge\",\"op\":\"select\",\"columns\":[\"_uuid\",\"ports\"]}]"};
            */
    string response7 {"[{\"rows\":[{\"ports\":[\"set\",[[\"uuid\",\
            \"0a7a4d65-e785-4674-a219-167391d10c3f\"],[\"uuid\",\
            \"373108c7-ce2d-4d46-a419-1654a5bf47ef\"],[\"uuid\",\
            \"ffaee0cd-bb7d-4698-9af1-99f57f9b7081\"]]],\"_uuid\":[\"uuid\",\
            \"7cb323d7-0215-406d-ae1d-679b72e1f6aa\"]}]}]"};
    /*
    string request8 {"[\"Open_vSwitch\",{\"uuid-name\":\"rowfaaf29de_8043_4197_8636_4c3d72aeefef\",\
            \"table\":\"Port\",\"op\":\"insert\",\"row\":{\"interfaces\":[\"named-uuid\",\
            \"rowfe66a640_545c_4de1_97d9_a6ea9c0e4434\"],\"name\":\"erspan\"}},{\"uuid-name\":\
            \"rowfe66a640_545c_4de1_97d9_a6ea9c0e4434\",\"table\":\"Interface\",\"op\":\"insert\",\
            \"row\":{\"name\":\"erspan\",\"options\":[\"map\",[[\"erspan_idx\",\"1\"],[\"erspan_ver\",\
            \"1\"],[\"key\",\"1\"],[\"remote_ip\",\"10.30.120.240\"]]],\"type\":\"erspan\"}},\
            {\"where\":[],\"table\":\"Bridge\",\"op\":\"update\",\"row\":{\"ports\":[\"set\",\
            [[\"uuid\",\"0a7a4d65-e785-4674-a219-167391d10c3f\"],[\"uuid\",\
            \"373108c7-ce2d-4d46-a419-1654a5bf47ef\"],[\"uuid\",\"ffaee0cd-bb7d-4698-9af1-\
            99f57f9b7081\"],[\"named-uuid\",\"rowfaaf29de_8043_4197_8636_4c3d72aeefef\"]]]}}]"};
            */
    string response8 {"[{\"uuid\":[\"uuid\",\"67a63d27-9f82-48e6-9931-068bf7dd1b1d\"]},{\"uuid\":[\"uuid\",\
            \"56eadeda-cb76-4d09-b49a-b5abf7640cd4\"]},{\"count\":1}]"};
    /*
    string request9 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"br-int\"]],\"table\":\
            \"Bridge\",\"op\":\"select\",\"columns\":[\"_uuid\"]}]"};
            */
    string response9 {"[{\"rows\":[{\"_uuid\":[\"uuid\",\"7cb323d7-0215-406d-ae1d-679b72e1f6aa\"]}]}]"};
    /*
    string request10 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"erspan\"]],\"table\":\
            \"Port\",\"op\":\"select\"}]"};
            */
    string response10 {"[{\"rows\":[{\"protected\":false,\"statistics\":[\"map\",[]],\"bond_downdelay\":0,\
            \"name\":\"erspan\",\"mac\":[\"set\",[]],\"fake_bridge\":false,\"trunks\":[\"set\",[]]\
            ,\"_uuid\":[\"uuid\",\"67a63d27-9f82-48e6-9931-068bf7dd1b1d\"],\"rstp_status\":\
            [\"map\",[]],\"tag\":[\"set\",[]],\"_version\":[\"uuid\",\"e1279783-fdf4-487d-9bdb-\
            35d0fdc225e0\"],\"cvlans\":[\"set\",[]],\"bond_updelay\":0,\"bond_active_slave\":\
            [\"set\",[]],\"status\":[\"map\",[]],\"external_ids\":[\"map\",[]],\"other_config\":\
            [\"map\",[]],\"qos\":[\"set\",[]],\"bond_mode\":[\"set\",[]],\"rstp_statistics\":[\
            \"map\",[]],\"vlan_mode\":[\"set\",[]],\"interfaces\":[\"uuid\",\"56eadeda-cb76-4d09-\
            b49a-b5abf7640cd4\"],\"bond_fake_iface\":false,\"lacp\":[\"set\",[]]}]}]"};
    /*
    string request11 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"p1-tap\"]],\"table\"\
            :\"Port\",\"op\":\"select\"}]"};
            */
    string response11 {"[{\"rows\":[{\"protected\":false,\"statistics\":[\"map\",[]],\"bond_downdelay\":0,\
            \"name\":\"p1-tap\",\"mac\":[\"set\",[]],\"fake_bridge\":false,\"trunks\":[\"set\",\
            []],\"_uuid\":[\"uuid\",\"0a7a4d65-e785-4674-a219-167391d10c3f\"],\"rstp_status\":\
            [\"map\",[]],\"tag\":[\"set\",[]],\"_version\":[\"uuid\",\"f67c3499-82d0-46da-95c2-\
            cc73eff735b2\"],\"cvlans\":[\"set\",[]],\"bond_updelay\":0,\"bond_active_slave\":\
            [\"set\",[]],\"status\":[\"map\",[]],\"external_ids\":[\"map\",[]],\"other_config\"\
            :[\"map\",[]],\"qos\":[\"set\",[]],\"bond_mode\":[\"set\",[]],\"rstp_statistics\":\
            [\"map\",[]],\"vlan_mode\":[\"set\",[]],\"interfaces\":[\"uuid\",\"22eb4594-4ee5-\
            42f0-be04-3c6daf46918f\"],\"bond_fake_iface\":false,\"lacp\":[\"set\",[]]}]}]"};
    /*
    string request12 {"[\"Open_vSwitch\",{\"where\":[[\"name\",\"==\",\"p2-tap\"]],\"table\":\
            \"Port\",\"op\":\"select\"}]"};
            */
    string response12 {"[{\"rows\":[{\"protected\":false,\"statistics\":[\"map\",[]],\"bond_downdelay\":0,\
            \"name\":\"p2-tap\",\"mac\":[\"set\",[]],\"fake_bridge\":false,\"trunks\":[\"set\",\
            []],\"_uuid\":[\"uuid\",\"373108c7-ce2d-4d46-a419-1654a5bf47ef\"],\"rstp_status\":\
            [\"map\",[]],\"tag\":[\"set\",[]],\"_version\":[\"uuid\",\"43c91940-feaa-42ee-8646-\
            5d26f90bde17\"],\"cvlans\":[\"set\",[]],\"bond_updelay\":0,\"bond_active_slave\":\
            [\"set\",[]],\"status\":[\"map\",[]],\"external_ids\":[\"map\",[]],\"other_config\":\
            [\"map\",[]],\"qos\":[\"set\",[]],\"bond_mode\":[\"set\",[]],\"rstp_statistics\":\
            [\"map\",[]],\"vlan_mode\":[\"set\",[]],\"interfaces\":[\"uuid\",\"9d39228b-186f-\
            455e-8da1-c8dc82d92692\"],\"bond_fake_iface\":false,\"lacp\":[\"set\",[]]}]}]"};
    /*
    string request13 {"[\"Open_vSwitch\",{\"uuid-name\":\"row8c31e171_96ef_452c_8a4b_abbdf1ea498c\
            \",\"table\":\"Mirror\",\"op\":\"insert\",\"row\":{\"name\":\"msandhu-sess1\",\
            \"output_port\":[\"set\",[[\"uuid\",\"67a63d27-9f82-48e6-9931-068bf7dd1b1d\"]]],\
            \"select_dst_port\":[\"set\",[[\"uuid\",\"0a7a4d65-e785-4674-a219-167391d10c3f\"],\
            [\"uuid\",\"373108c7-ce2d-4d46-a419-1654a5bf47ef\"]]],\"select_src_port\":[\"set\",\
            [[\"uuid\",\"373108c7-ce2d-4d46-a419-1654a5bf47ef\"],[\"uuid\",\"0a7a4d65-e785-4674-\
            a219-167391d10c3f\"]]]}},{\"where\":[[\"_uuid\",\"==\",[\"uuid\",\"7cb323d7-0215-\
            406d-ae1d-679b72e1f6aa\"]]],\"table\":\"Bridge\",\"op\":\"update\",\"row\":\
            {\"mirrors\":[\"named-uuid\",\"row8c31e171_96ef_452c_8a4b_abbdf1ea498c\"]}}]"};
            */
    string response13 {"[{\"uuid\":[\"uuid\",\"ad0810fb-fa38-4dd0-b0b3-6a98985dd2bc\"]},{\"count\":1}]"};

    string response[no_of_msgs] = {response1, response2, response3, response4, response5,
            response6, response7, response8, response9, response10, response11, response12,
            response13 };

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
