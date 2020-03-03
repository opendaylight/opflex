/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OvsdbConnection.h
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
#ifndef OPFLEX_ENGINE_INTERNAL_OVSDBCONNECTION_H
#define OPFLEX_ENGINE_INTERNAL_OVSDBCONNECTION_H

#include "opflex/rpc/JsonRpc.h"
#include "opflex/engine/internal/OpflexMessage.h"

namespace opflex {
namespace engine {
namespace internal {

class TransactReq : public OpflexMessage {
public:
    TransactReq(const transData& td);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    virtual TransactReq* clone(){
        return new TransactReq(*this);
    }

    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer);

    template <typename T>
    void writePair(rapidjson::Writer<T>& writer, shared_ptr<BaseData> bPtr, bool kvPair);

    transData tData;

};

/*
 * JSON/RPC transaction message
 */
class JsonReq : public OpflexMessage {
public:
    JsonReq(const list<transData>& tl, uint64_t reqId);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    virtual JsonReq* clone(){
        return new JsonReq(*this);
    };

    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        writer.StartArray();
        writer.String("Open_vSwitch");
        for (shared_ptr<TransactReq> tr : transList) {
            writer.StartObject();
            (*tr)(writer);
            writer.EndObject();
        }
        writer.EndArray();
        return true;
    }

    list<shared_ptr<TransactReq>> transList;
    uint64_t reqId;
};

class OvsdbConnection : public RpcConnection {
    public:
    OvsdbConnection(Transaction* pTrans_) : RpcConnection(pTrans_) {}

    /**
     * get pointer to peer object
     * @return pointer to peer instance
     */
    yajr::Peer* getPeer() { return peer;};

    /**
     * initialize the module
     */
    void start();

    /**
     * stop the module
     */
    void stop();

    /**
     * call back to handle connection state changes
     * @param[in] p pointer to Peer object
     * @param[in] data void pointer to passed in context
     * @param[in] stateChange call back to handle connection state changes
     * @param[in] error error reported by call back caller
     */
    static void on_state_change(yajr::Peer * p, void * data,
            yajr::StateChange::To stateChange,
            int error);

    /**
     * get loop selector attribute
     * @param[in] data void pointer to context
     * @return a pointer to uv_loop_t
     */
    static uv_loop_t* loop_selector(void* data);

    /**
     * create a tcp connection to peer
     */
    void connect();

    /**
     * callback for invoking connect
     * @param[in] handle pointer to uv_async_t
     */
    static void connect_cb(uv_async_t* handle);

    /**
     * callback for sending requests
     * @param[in] handle pointer to uv_async_t
     */
    static void send_req_cb(uv_async_t* handle);

    /**
     * send transaction request
     * @param[in] tl list of transData objects
     * @param[in] reqId request ID
     */
    virtual void sendTransaction(const list<transData>& tl, const uint64_t& reqId);


    yajr::Peer* peer;

private:

    typedef struct req_cb_data_ {
        JsonReq* req;
        yajr::Peer* peer;
    } req_cb_data;

    uv_loop_t* client_loop;
    util::ThreadManager threadManager;
    uv_async_t connect_async;
    uv_async_t send_req_async;
    string hostname;
    int port;

};
}
}
}
#endif
