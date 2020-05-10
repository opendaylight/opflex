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
#include <rapidjson/document.h>
#include "OvsdbConnection.h"
#include <opflexagent/SpanSessionState.h>
#include <opflexagent/logging.h>

#include <boost/optional.hpp>

namespace opflexagent {

using namespace std;
using namespace opflex;
using namespace opflex::jsonrpc;
using namespace std::chrono;

/*
 * name of ERPSAN port
 */
static const string ERSPAN_PORT_PREFIX("erspan");

/**
 * class to handle JSON/RPC transactions without opflex.
 */
class JsonRpc : public Transaction {
public:


    /**
     * struct for managing mirror data
     */
    typedef struct mirror_ {
        /**
         * UUID of the mirror
         */
        string uuid;
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
        string out_port;
    } mirror;

    /**
     * Constructor
     *
     * @param conn_ OVSDB connection
     */
    JsonRpc(OvsdbConnection* conn_) : conn(conn_) {}
    /**
     * destructor
     */
    virtual ~JsonRpc() {}
    /**
     * call back for transaction response
     * @param[in] reqId request ID of the request for this response.
     * @param[in] payload Document reference of the response body.
     */
    void handleTransaction(uint64_t reqId, const Document& payload);

    /**
     * update the port list for the bridge
     * @param[in] brUuid bridge UUID
     * @param[in] portUuid port UUID to be added or removed.
     * @param[in] addToList true: add port, false: remove port.
     * @return bool true if update succeeded, false otherwise.
     */
    bool updateBridgePorts(const string& brUuid, const string& portUuid, bool addToList);

    /**
     * get the UUID of the port
     * @param[in] name name of the port
     * @param[out] uuid of the port or empty string.
     */
    void getPortUuid(const string& name, string& uuid);

    /**
     * create a tcp connection to peer
     */
    virtual void connect();

    /**
     * create mirror
     * @param[in] uuid uuid of the bridge to add the mirror to.
     * @param[in] name name of mirror
     * @param[in] srcPorts source ports
     * @param[in] dstPorts dest ports
     * @return bool true if created successfully, false otherwise.
     */
    bool createMirror(const string& uuid, const string& name, const set<string>& srcPorts, const set<string>& dstPorts);

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
     * @param[in] sessionName Session to delete
     * @return true if success, false otherwise.
     */
    bool deleteMirror(const string& brName, const string& sessionName);

    /**
     * get uuid of bridge from OVSDB
     * @param[in] name name of bridge
     * @param[out] uuid of the bridge or empty
     */
    void getBridgeUuid(const string& name, string& uuid);

    /**
     * get uuid of the named mirror from OVSDB
     * @param[in] name name of mirror
     * @param[out] uuid of the mirror or empty
     */
    void getMirrorUuid(const string& name, string& uuid);

    /**
     * read port uuids from the map and insert into list
     * @param[in] ports set of port names
     * @param[in] uuidMap map of port names to uuids
     * @param[out] entries list of port uuids
     */
    static void populatePortUuids(const set<string>& ports, const map<string, string>& uuidMap, set<tuple<string, string>>& entries);

    /**
     * add an erspan port to the bridge
     * @param[in] bridgeName name of bridge to add the port to
     * @param[in] params ERSPAN params
     * @return true if success, false otherwise
     */
    bool addErspanPort(const string& bridgeName, ErspanParams& params);

    /**
     * createNetFlow
     * @param[in] brUuid uuid of the bridge to add the netflow to.
     * @param[in] target target of netflow
     * @param[in] timeout timeout of netflow
     * @param[in] addidtointerface add id to interface of netflow
     * @return bool true if created successfully, false otherwise.
    */
    bool createNetFlow(const string& brUuid, const string& target, const int& timeout, bool addidtointerface = false);

     /**
     * deletes netflow on OVSDB bridge.
     * @param[in] brName name of bridge that the netflow is associated with
     * @return true if success, false otherwise.
    */
    bool deleteNetFlow(const string& brName);

    /**
     * createNetFlow
     * @param[in] brUuid uuid of the bridge to add the ipfix to.
     * @param[in] target target of ipfix
     * @param[in] sampling sampling of ipfix
     * @return bool true if created successfully, false otherwise.
    */
    bool createIpfix(const string& brUuid, const string& target, const int& sampling);

     /**
     * deletes ipfix on OVSDB bridge.
     * @param[in] brName name of bridge that the ipfix is associated with
     * @return true if success, false otherwise.
    */
    bool deleteIpfix(const string& brName);

    /**
     * retrieve uuid from response
     * @param[in] payload body of the response
     * @param[in] uuidName name of the UUID field
     * @param[out] uuid uuid or empty
     */
    static void getUuidByNameFromResp(const rapidjson::Document& payload, const string& uuidName, string& uuid);

    /**
     * retrieve list of uuids from response
     * @param[in] payload body of the response
     * @param[in] uuidsName name of the field that's a list of uuids
     * @param[out] uuids list of uuids
     */
    static void getUuidsByNameFromResp(const rapidjson::Document& payload, const string& uuidsName, set<string>& uuids);

    /**
     * process mirror config
     * @param[in] payload body of the response
     * @param[out] mir mirror info
     * @return true id success, false otherwise
     */
    static bool handleMirrorConfig(const rapidjson::Document& payload, mirror& mir);

    /**
     * get the mirror config from OVSDB.
     * @param[in] sessionName session name
     * @param[out] mir struct to hold mirror data
     * @return bool true if retrieval succeeded, false otherwise.
     */
    bool getOvsdbMirrorConfig(const string& sessionName, mirror& mir);

    /**
     * get ERSPAN interface parameters from OVSDB
     * @param[in] portName ERSPAN port name
     * @param[out] params erspan interface struct
     * @return true if success, false otherwise
     */
    bool getCurrentErspanParams(const string& portName, ErspanParams& params);

    /**
     * check if connection has been established
     */
    bool isConnected();

    /**
     * get rpc connection pointer
     * @return pointer to OVSDB connection
     */
    OvsdbConnection* getConnection() { return conn; }

private:

    /**
     * get the list of port names and UUIDs from the Value struct
     * @param[in] payload a Value struct
     * @param[out] portMap unordered map of port UUID as key and name as value
     * @return bool false if there is a problem getting the value, true otherwise
     */
    static bool getPortList(const Document& payload, unordered_map<string, string>& portMap);

    /**
     * get ERSPAN interface options from Value struct
     * @param[in] payload response Value struct
     * @param[out] pararms ERSPAN session params
     * interface struct
     */
    static bool getErspanOptions(const Document& payload, ErspanParams& params);

    template <typename T>
    inline bool sendRequestAndAwaitResponse(const list<T> &tl) {
        unique_lock<mutex> lock(OvsdbConnection::ovsdbMtx);
        if (!conn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                [=]{return conn->isConnected();})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        }
        responseReceived = false;
        conn->sendTransaction(tl, this);

        if (!conn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                                   [=]{return responseReceived;})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        } else {
            return true;
        }
    }

    static void substituteSet(set<string>& s, const unordered_map<string, string>& portMap);

    class Response {
    public:
        uint64_t reqId;
        rapidjson::Document payload;

        Response(uint64_t reqId, const rapidjson::Document& payload_) :
            reqId(reqId) {
            payload.CopyFrom(payload_, payload.GetAllocator());
        }
    };

    bool responseReceived = false;
    const int WAIT_TIMEOUT = 10;
    OvsdbConnection* conn;
    shared_ptr<Response> pResp;
};

}
#endif // OPFLEX_JSONRPC_H
