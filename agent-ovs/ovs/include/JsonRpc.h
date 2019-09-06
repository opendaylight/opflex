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
#include <condition_variable>
#include <mutex>

#include "rpc/JsonRpc.h"
#include <opflexagent/logging.h>

#include <boost/optional.hpp>

namespace opflexagent {

using namespace std;
using namespace opflex;
using namespace opflex::engine::internal;
//using f_type = std::function<void(uint64_t, const rapidjson::Value&)>;

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
     * struct for managing erspan data
     */
    typedef struct erspan_port_ {
        /**
         * name of erspan port
         */
        string name;
        /**
         * IP Address of the destination
         */
        string ip_address;
    } erspan_port;

    /**
     * result for mirror.
     * has a subset of columns from the mirror table in OVSDB
     */
    struct MirrorResult {
        /**
         * struct for holding mirror information
         */
        mirror mir;
    };

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
    string getPortUuid(string name);

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
     * @param[in] port name of port to be added
     * @return true if success, false otherwise
     */
    bool addErspanPort(const string bridgeName, const erspan_port port);

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
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid of the mirror
     * @return true id success, false otherwise
     */
    bool handleCreateMirrorResp(uint64_t reqId, const rapidjson::Value& payload,
            string& uuid);

    /**
     * Macro to declare handler
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid of the mirror
     * @return true id success, false otherwise
     */
#define DECLARE_HANDLER(F) \
        void F(uint64_t reqId, const rapidjson::Value& payload);

    DECLARE_HANDLER(handleGetBridgeMirrorUuidResp);
    DECLARE_HANDLER(handleAddMirrorToBridgeResp);
    DECLARE_HANDLER(handleAddErspanPortResp);

    /**
     * TCP port for ovsdb rpc connection
     */
    const uint32_t OVSDB_RPC_PORT = 6640;


private:
    uint64_t getNextId() { return ++id; }

    /**
     * generate a UUID for use by OVSDB when creating new artifacts.
     * This UUID conforms to OVSDB format for temp UUIDs.
     * @return string temp UUID
     */
    string generateTempUuid();

    /**
     * send a JSON RPC request to the peer
     * @param[in] msg the request message body
     * @param[in] reqId request ID of the message
     * @return true if message sent, false otherwise.

    bool sendRequest(TransactReq&, uint64_t);
*/
    template <typename T>
    inline bool sendRequest(list<T>& tl, uint64_t reqId) {
        unique_lock<mutex> lock(pConn->mtx);
        if (!pConn->ready.wait_for(lock, WAIT_TIMEOUT*1000ms,
                [=]{return pConn->isConnected();})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        }
        responseReceived = false;
        opflex::engine::internal::sendTransaction(tl, pConn, reqId);
        return true;
    }
    /**
     * checks for response arrival
     * @return true if response received, false if timed out.
     */
    bool checkForResponse();

    //std::function<void(uint64_t, const rapidjson::Value&)> nextMethod;
    /**
     * print mirror map values
     */
    void printMirMap(map<string, mirror> mirMap);

    /**
     * print map<string, string> key value pairs.
     */
    void printMap(map<string, string> m);

    enum Action {
        NOP = 0,
        DEL_PORT,
        DEL_MIRROR,
        ADD_PORT,
        ADD_MIRROR,
    };

    typedef struct response_ {
        uint64_t reqId;
        const rapidjson::Value& payload;
        response_(uint64_t reqId, const rapidjson::Value& payload) :
            reqId(reqId), payload(payload) {}
    } response;


//    bool connected = false;
    bool responseReceived = false;
    Action action = NOP;
    map<string, string> results;
    map<string, mirror> mirMap;
    const int WAIT_TIMEOUT = 3;
    string error;
    shared_ptr<RpcConnection> pConn;
    shared_ptr<response> pResp;
    uint64_t id = 0;
};
}
#endif // OPFLEX_JSONRPC_H
