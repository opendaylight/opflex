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
#ifndef OPFLEX_ENGINE_INTERNAL_OVSDBCONNECTION_H
#define OPFLEX_ENGINE_INTERNAL_OVSDBCONNECTION_H

#include "rpc/JsonRpc.h"
#include "opflex/engine/internal/OpflexMessage.h"

namespace opflex {
namespace engine {
namespace internal {

class TransactReq : public OpflexMessage {
public:
    TransactReq(const transData& td);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    virtual void serializePayload(MessageWriter& writer);

    virtual TransactReq* clone(){
        return new TransactReq(*this);
    }

    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer);

public:

    enum class COLLECTION {
        NONE = 0,
        SET,
        MAP,
    };

    enum class TYPE {
        NONE = 0,
        NAMED_UUID,
        UUID,
    };

    transData tData;
    //std::function<void()> ResponseCb();

};

/*
 * JSON/RPC transaction message
 */
class JsonReq : public OpflexMessage {
public:
    JsonReq(list<transData> tl);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    virtual void serializePayload(MessageWriter& writer);

    virtual JsonReq* clone(){
        return new JsonReq(*this);
    };

    void addTransaction(shared_ptr<TransactReq> req);

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
};

class OvsdbConnection : public RpcConnection {
    public:
    OvsdbConnection(Transaction* pTrans_) : RpcConnection(pTrans_) {}

    /**
     * call back for transaction response
     * @param[in] reqId request ID of the request for this response.
     * @param[in] payload rapidjson::Value reference of the response body.
     */
    void handleTransaction(uint64_t reqId,
                const rapidjson::Value& payload);

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
     * call back for idle task. Is used to keep the loop active.
     * @param[in] handle reference to uv_idle_t struct
     */
    static void idle_cb(uv_idle_t* handle);

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
     * @param[in] hostname host name of the peer.
     * @param[in] port port number to connect to.
     * @param[in] pTrans pointer to Transaction object
     */
    void connect(string const& hostname, int port);

    yajr::Peer* peer;

private:

    uv_idle_t idler;
    uv_loop_t* client_loop;
    util::ThreadManager threadManager;

};
}
}
}
#endif
