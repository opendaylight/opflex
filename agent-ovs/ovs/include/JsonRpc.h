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
static const string ERSPAN_PORT_NAME("erspan");

/**
 * helper function to get Value of a given index
 * @param[in] val rapidjson Value object
 * @param[in] idx list of strings representing indices
 * @param[out] result Value object
 */
void getValue(const Document& val, const list<string>& idx, Value& result);

/**
 * class to handle JSON/RPC transactions without opflex.
 */
class JsonRpc : public Transaction {
public:
    /**
     * base struct for ERSPAN interface parameters
     */
    typedef struct erspan_ifc_ {
        /**
         * ERSPAN version
         */
        int erspan_ver;
        /**
         * name of ERSPAN port
         */
        string name;
        /**
         * ERSPAN key
         * maps to ERSPAN session ID/Span ID
         */
        int key;
        /**
         * destination IP address
         */
        string remote_ip;
    } erspan_ifc;

    /**
     * ERSPAN type II struct
     */
    typedef struct erspan_ifc_v1_ : erspan_ifc {
        /**
         * constructor
         * ERSPAN version 1 maps to ERSPAN type 2
         */
        erspan_ifc_v1_() {
            erspan_ver = 1;
            erspan_idx = 0;
        }
        /**
         * ERSPAN index
         * This field is a 20-bit index/port number associated with the ERSPAN traffic's
         * source port and direction (ingress/egress). This field is platform dependent.
         */
        int erspan_idx;
    } erspan_ifc_v1;

    /**
     * ERSPAN type III struct
     */
    typedef struct erspan_ifc_v2_ : erspan_ifc {
        /**
         * constructor
         * ERSPAN version 2 maps to ERSPAN type 3.
         */
        erspan_ifc_v2_() {
            erspan_ver = 2;
            erspan_hw_id = 0;
            erspan_dir = 0;
        }
        /**
         * ERSPAN hardware ID
         * A 6-bit unique identifier of an ERSPAN v2 engine within a system.
         */
        int erspan_hw_id;
        /**
         * ERSPAN direction
         * the mirrored traffic's direction: 0 for ingress traffic, 1 for egress traffic.
         */
        int erspan_dir;
    } erspan_ifc_v2;

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
     * @param[in] ports a tuple of bridge name a port UUIDs
     * @param[in] port port UUID to be added or removed.
     * @param[in] action true: add port, false: remove port.
     * @return bool true if update succeeded, false otherwise.
     */
    bool updateBridgePorts(tuple<string,set<string>> ports,
                const string& port, bool action);

    /**
     * sends request to get port list of the bridge
     * @param[in] bridge name of bridge
     * @param[out] result bridge and port UUIDs
     * @return bool true if successful, false otherwise
     */
    bool getBridgePortList(const string& bridge, BrPortResult& result);

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
     * @return true if success, false otherwise.
     */
    bool deleteMirror(const string& brName);

    /**
     * get uuid of bridge from OVSDB
     * @param[in] name name of bridge
     * @param[out] uuid of the bridge or empty
     */
    void getBridgeUuid(const string& name, string& uuid);

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
     * @param[in] port erspan_ifc struct
     * @return true if success, false otherwise
     */
    bool addErspanPort(const string& bridgeName, shared_ptr<erspan_ifc> port);

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
     * process port uuid request response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid uuid of the port
     * @return true id success, false otherwise
     */
    static bool handleGetPortUuidResp(uint64_t reqId, const rapidjson::Document& payload,
            string& uuid);

    /**
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] brPtr shared pointer to result struct
     * @return true id success, false otherwise
     */
    static bool handleGetBridgePortList(uint64_t reqId, const rapidjson::Document& payload,
            shared_ptr<BrPortResult>& brPtr);

    /**
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid of the bridge
     */
    static void handleGetBridgeUuidResp(uint64_t reqId, const rapidjson::Document& payload,
            string& uuid);

    /**
     * process mirror config
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] mir mirror info
     * @return true id success, false otherwise
     */
    static bool handleMirrorConfig(uint64_t reqId, const rapidjson::Document& payload,
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
     * @return true id success, false otherwise
     */
    static bool handleCreateMirrorResp(uint64_t reqId, const rapidjson::Document& payload);

    /**
     * get ERSPAN interface parameters from OVSDB
     * @param[out] pIfc empty shared pointer reference to erspan interface struct
     * @return true if success, false otherwise
     */
    bool getErspanIfcParams(shared_ptr<erspan_ifc>& pIfc);

    /**
     * check if connection has been established
     */
    bool isConnected();

    /**
     * get rpc connection pointer
     * @return pointer to OVSDB connection
     */
    OvsdbConnection* getConnection() { return conn; }

    /**
     * set the next request ID
     * @param id_ request id
     */
    void setNextId(uint64_t id_) { id = id_;}

private:
    uint64_t getNextId() { return ++id; }

    /**
     * get UUIDs from a Value struct. Can handle both a single uuid two tuple
     * or an set of uuid two tuples.
     * @param[in] payload Value struct with the info to be parsed
     * @param[in] index an index into the Value struct
     * @param[out] uuidSet set of UUIDs extracted from the Value struct
     */
    static void getUuidsFromVal(set<string>& uuidSet, const Document& payload, const string& index);

    /**
     * get the list of port names and UUIDs from the Value struct
     * @param[in] payload Value struct with the info to be parsed
     * @param[in] payload a Value struct
     * @param[out] portMap unordered map of port UUID as key and name as value
     * @return bool false if there is a problem getting the value, true otherwise
     */
    static bool getPortList(const uint64_t reqId, const Document& payload, unordered_map<string, string>& portMap);

    /**
     * get ERSPAN interface options from Value struct
     * @param[in] reqId request ID
     * @param[in] payload response Value struct
     * @param[out] pIfc empty shared pointer to ERSPAN
     * interface struct
     */
    static bool getErspanOptions(const uint64_t reqId, const Document& payload, shared_ptr<erspan_ifc>& pIfc);

    template <typename T>
    inline bool sendRequestAndAwaitResponse(const list<T>& tl, uint64_t reqId) {
        unique_lock<mutex> lock(conn->mtx);
        if (!conn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                [=]{return conn->isConnected();})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        }
        responseReceived = false;
        conn->sendTransaction(reqId, tl, this);

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
    uint64_t id = 0;
};

}
#endif // OPFLEX_JSONRPC_H
