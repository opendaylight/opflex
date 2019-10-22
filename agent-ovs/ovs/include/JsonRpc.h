/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file JsonRpc.h
 * @brief Interface definition file for JSON/RPC messages
 */
 /*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEX_JSONRPC_H
#define OPFLEX_JSONRPC_H


#include <tuple>
#include <map>
#include <unordered_map>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <thread>

#include "rpc/JsonRpc.h"
#include <opflexagent/logging.h>

#include <boost/optional.hpp>

namespace opflexagent {

using namespace std;
using namespace opflex;
using namespace opflex::engine::internal;
using namespace std::chrono;

/*
 * name of ERPSAN port
 */
static const string ERSPAN_PORT_NAME("erspan");

/**
 * class to handle JSON/RPC transactions without opflex.
 */
class JsonRpc : public Transaction {
public:
    /**
     * struct for ERSPAN interface parameters
     */
    typedef struct erspan_ifc_ {
        /**
         * name of ERSPAN port
         */
        string name;
        /**
         * ERSPAN port index
         */
        int erspan_idx;
        /**
         * ERSPAN version
         */
        int erspan_ver;
        /**
         * ERSPAN key
         */
        int key;
        /**
         * destination IP address
         */
        string remote_ip;
    } erspan_ifc;

    /**
     * struct for managing mirror data
     */
    typedef struct mirror_ {
        /**
         * UUID of the mirror
         */
        string uuid;
        /**
         * UUID of the bridge
         */
        string brUuid;
        /**
         * set of source port UUIDs
         */
        set<string> src_ports;
        /**
         * set of destination port UUIDs
         */
        set<string> dst_ports;
        /**
         * set of erspan ports
         */
        set<string> out_ports;
    } mirror;

    /**
     * results for bridge port list query
     * contains UUIDs for the bridge and the ports in the ports column
     * of the bridge table row.
     */
    struct BrPortResult {
        /**
         * bridge UUID
         */
        string brUuid;
        /**
         * set of port UUIDs
         */
        set<string> portUuids;
    };

    /**
     * call back for transaction response
     * @param[in] reqId request ID of the request for this response.
     * @param[in] payload rapidjson::Value reference of the response body.
     */
    void handleTransaction(uint64_t reqId,
                const rapidjson::Value& payload);

    /**
     * update the port list for the bridge
     * @param[in] ports a tuple of bridge name a port UUIDs
     * @param[in] port port UUID to be added or removed.
     * @param[in] action true: add port, false: remove port.
     * @return bool true if update succeeded, false otherwise.
     */
    bool updateBridgePorts(tuple<string,set<string>> ports,
                string port, bool action);

    /**
     * sends request to get port list of the bridge
     * @param[in] bridge name of bridge
     * @param[out] result bridge and port UUIDs
     * @return bool true if successful, false otherwise
     */
    bool getBridgePortList(string bridge, BrPortResult& result);

    /**
     * get the UUID of the port
     * @param[in] name name of the port
     * @return uuid of the port or empty string.
     */
    string getPortUuid(const string& name);

    /**
     * initialize the module
     */
    void start();

    /**
     * stop the module
     */
    void stop();

    /**
     * create a tcp connection to peer
     * @param[in] hostname host name of the peer.
     * @param[in] port port number to connect to.
     */
    void connect(string const& hostname, int port);

    /**
     * create mirror
     * @param[in] uuid uuid of the bridge to add the mirror to.
     * @param[in]  name name of mirror
     * @return bool true if created successfully, false otherwise.
     */
    bool createMirror(string uuid, string name);

    /**
     * get port uuids from OVSDB
     * @param[out] ports map of mirror ports. The uuids are populated in
     * map.
     *
     */
    void getPortUuids(map<string, string>& ports);

    /**
     * deletes mirror on OVSDB bridge.
     * @param[in] brName name of bridge that the mirror is associated with
     * @return true if success, false otherwise.
     */
    bool deleteMirror(string brName);

    /**
     * get uuid of bridge from OVSDB
     * @param[in] name name of bridge
     * @return uuid of the bridge or empty
     */
    string getBridgeUuid(string name);

    /**
     * read port uuids from the map and insert into list
     * @param[in] ports set of port names
     * @param[in] uuidMap map of port names to uuids
     * @param[out] entries list of port uuids
     */
    void populatePortUuids(set<string>& ports, map<string,
                string>& uuidMap, set<tuple<string, string>>& entries);

    /**
     * add an erspan port to the bridge
     * @param[in] bridgeName name of bridge to add the port to
     * @param[in] port erspan_ifc struct
     * @return true if success, false otherwise
     */
    bool addErspanPort(const string& bridgeName, const erspan_ifc& port);

    /**
     * add mirror data to in memory struct
     * @param[in] name name of mirror
     * @param[in] mir struct mirror
     */
    void addMirrorData(string name, mirror mir);

    /**
     * process port uuid request response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid uuid of the port
     * @return true id success, false otherwise
     */
    bool handleGetPortUuidResp(uint64_t reqId, const rapidjson::Value& payload,
            string& uuid);

    /**
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] brPtr shared pointer to result struct
     * @return true id success, false otherwise
     */
    bool handleGetBridgePortList(uint64_t reqId, const rapidjson::Value& payload,
            shared_ptr<BrPortResult> brPtr);

    /**
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid of the bridge
     * @return true id success, false otherwise
     */
    bool handleGetBridgeUuidResp(uint64_t reqId, const rapidjson::Value& payload,
            string& uuid);

    /**
     * process mirror config
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] mir mirror info
     * @return true id success, false otherwise
     */
    bool handleMirrorConfig(uint64_t reqId, const rapidjson::Value& payload,
            mirror& mir);

    /**
     * get the mirror config from OVSDB.
     * @param[out] mir struct to hold mirror data
     * @return bool true if retrieval succeeded, false otherwise.
     */
    bool getOvsdbMirrorConfig(mirror& mir);

    /**
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid of the mirror
     * @return true id success, false otherwise
     */
    bool handleCreateMirrorResp(uint64_t reqId, const rapidjson::Value& payload,
            string& uuid);

    /**
     * get ERSPAN interface parameters from OVSDB
     * @param[in] ifc erspan interface struct
     * @return true if success, false otherwise
     */
    bool getErspanIfcParams(erspan_ifc& ifc);

/*! macro to declare handlers */
#define DECLARE_HANDLER(F) \
        /** \
         * F \
         * @param[in] reqId request ID \
         * @param[in] payload body of the response \
         */ \
        void F(uint64_t reqId, const rapidjson::Value& payload);
    /*! declaration for  handleGetBridgeMirrorUuidResp */
    DECLARE_HANDLER(handleGetBridgeMirrorUuidResp);
    /*! declaration for handleAddMirrorToBridgeResp */
    DECLARE_HANDLER(handleAddMirrorToBridgeResp);
    /*! declaration for handleAddErspanPortResp */
    DECLARE_HANDLER(handleAddErspanPortResp);

private:
    uint64_t getNextId() { return ++id; }

    /**
     * get UUIDs from a Value struct. Can handle both a single uuid two tuple
     * or an set of uuid two tuples.
     * @param[in] payload Value struct with the info to be parsed
     * @param[in] index an index into the Value struct
     * @param[out] uuidSet set of UUIDs extracted from the Value struct
     */
    void getUuidsFromVal(set<string>& uuidSet, const Value& payload,
                const string& index);

    /**
     * get the list of port names and UUIDs from the Value struct
     * @param[in] payload Value struct with the info to be parsed
     * @param[in] payload a Value struct
     * @param[out] portMap unordered map of port UUID as key and name as value
     * @return bool false if there is a problem getting the value, true otherwise
     */
    bool getPortList(const uint64_t reqId, const Value& payload,
                unordered_map<string, string>& portMap);

    /**
     * generate a UUID for use by OVSDB when creating new artifacts.
     * This UUID conforms to OVSDB format for temp UUIDs.
     * @return string temp UUID
     */
    string generateTempUuid();

    /**
     * get the port parameter from Value struct
     * @param[in] payload Value struct with the info to be parsed
     * @param[in] payload a Value struct
     * @param[in] col column name in the table
     * @param[out] param the paramter to be returned
     * @return bool false if there is a problem getting the value, true otherwise.
     */
    bool handleGetPortParam(uint64_t reqId,
                const rapidjson::Value& payload, string& col, string& param);

    /**
     * get a value of the column in a specific row of the port table
     * @param[in] col the column name in the port table
     * @param[in] match the mtach that we are looking for
     * @return bool flase if problem getting the value, true otherwise.
     */
    string getPortParam(const string& col, const string& match);

    /**
     * get ERSPAN interface options from Value struct
     * @param[in] reqId request ID
     * @param[in] payload response Value struct
     * @param[out] ifc ERSPAN interface struct
     */
    bool getErspanOptions(const uint64_t reqId, const Value& payload,
            erspan_ifc& ifc);

    template <typename T>
    inline bool sendRequest(list<T>& tl, uint64_t reqId) {
        unique_lock<mutex> lock(pConn->mtx);
        if (!pConn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                [=]{return pConn->isConnected();})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        }
        responseReceived = false;
        pConn->sendTransaction(tl, reqId);
        return true;
    }

    void substituteSet(set<string>& s, const unordered_map<string, string>& portMap);

    /**
     * checks for response arrival
     * @return true if response received, false if timed out.
     */
    bool checkForResponse();

    //std::function<void(uint64_t, const rapidjson::Value&)> nextMethod;
    /**
     * print mirror map values
     */
    void printMirMap(const map<string, mirror>& mirMap);

    /**
     * print map<string, string> key value pairs.
     */
    void printMap(const map<string, string>& m);

    /**
     * print set<string> members.
     */
    void printSet(const set<string>& s);

    typedef struct response_ {
        uint64_t reqId;
        const rapidjson::Value& payload;
        response_(uint64_t reqId, const rapidjson::Value& payload) :
            reqId(reqId), payload(payload) {}
    } response;


//    bool connected = false;
    bool responseReceived = false;
    map<string, string> results;
    map<string, mirror> mirMap;
    const int WAIT_TIMEOUT = 10;
    string error;
    shared_ptr<RpcConnection> pConn;
    shared_ptr<response> pResp;
    uint64_t id = 0;
};
}
#endif // OPFLEX_JSONRPC_H
