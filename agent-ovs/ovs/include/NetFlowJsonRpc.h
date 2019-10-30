/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file NetFlowJsonRpc.h
 * @brief Interface definition file for JSON/RPC messages
 */
 /*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEX_NetFlowJsonRpc_H
#define OPFLEX_NetFlowJsonRpc_H


#include <tuple>
#include <map>
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

/**
 * class to handle JSON/RPC transactions without opflex.
 */
class NetFlowJsonRpc : public Transaction {
public:
   
    /**
     * call back for transaction response
     * @param[in] reqId request ID of the request for this response.
     * @param[in] payload rapidjson::Value reference of the response body.
     */
    void handleTransaction(uint64_t reqId,
                const rapidjson::Value& payload);

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
     * @param[in] uuid uuid of the bridge to add the netflow to.
     * @param[in]  target target of netflow
     * @return bool true if created successfully, false otherwise.
    */
    bool createNetFlow(const string& brUuid, const string& target, const int& timeout, bool addidtointerface = false);

    /**
     * deletes mirror on OVSDB bridge.
     * @param[in] brName name of bridge that the netflow is associated with
     * @return true if success, false otherwise.
    */
    bool deleteNetFlow(const string& brName);

    /**
     * get uuid of bridge from OVSDB
     * @param[in] name name of bridge
     * @return uuid of the bridge or empty
    */
    string getBridgeUuid(const string& name);
    /**
     * process bridge port list response
     * @param[in] reqId request ID
     * @param[in] payload body of the response
     * @param[out] uuid of the mirror
     * @return true id success, false otherwise
    */
    bool handleCreateNetFlowResp(uint64_t reqId, const rapidjson::Value& payload,
            string& uuid);
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
     * @param[in] tl list of transData objects
     * @param[in] reqId request ID of the message
     * @return true if message sent, false otherwise.

    bool sendRequest(TransactReq&, uint64_t);
    */
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
    /**
     * checks for response arrival
     * @return true if response received, false if timed out.
     */
    bool checkForResponse();

    typedef struct response_ {
        uint64_t reqId;
        const rapidjson::Value& payload;
        response_(uint64_t reqId, const rapidjson::Value& payload) :
            reqId(reqId), payload(payload) {}
    } response;


    bool connected = false;
    bool responseReceived = false;
    map<string, string> results;
    const int WAIT_TIMEOUT = 10;
    string error;
    shared_ptr<RpcConnection> pConn;
    shared_ptr<response> pResp;
    uint64_t id = 0;
};
}
#endif // OPFLEX_NetFlowJsonRpc_H
