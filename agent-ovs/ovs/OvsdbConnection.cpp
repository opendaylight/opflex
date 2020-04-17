/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of ovsdb messages for engine
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

extern "C" {
#include <lib/dirs.h>
}

#include "ovs/include/OvsdbConnection.h"
#include <opflexagent/logging.h>

namespace opflexagent {


void OvsdbConnection::send_req_cb(uv_async_t* handle) {
    auto* reqCbd = (req_cb_data*)handle->data;
    TransactReq* req = reqCbd->req;
    yajr::rpc::MethodName method(req->getMethod().c_str());
    opflex::jsonrpc::PayloadWrapper wrapper(req);
    yajr::rpc::OutboundRequest outr =
        yajr::rpc::OutboundRequest(wrapper, &method, req->getReqId(), reqCbd->peer);
    outr.send();
    delete(req);
    delete(reqCbd);
}

void OvsdbConnection::sendTransaction(const list<JsonRpcTransactMessage>& requests, Transaction* trans) {
    uint64_t reqId = 0;
    {
        unique_lock<mutex> lock(transactionMutex);
        reqId = getNextId();
        transactions[reqId] = trans;
    }
    auto* reqCbd = new req_cb_data();
    reqCbd->req = new TransactReq(requests, reqId);
    reqCbd->peer = getPeer();
    send_req_async.data = (void*)reqCbd;
    uv_async_send(&send_req_async);
}

void OvsdbConnection::start() {
    LOG(DEBUG) << "Starting .....";
    unique_lock<mutex> lock(mtx);
    client_loop = threadManager.initTask("OvsdbConnection");
    yajr::initLoop(client_loop);
    uv_async_init(client_loop,&connect_async, connect_cb);
    uv_async_init(client_loop, &send_req_async, send_req_cb);

    threadManager.startTask("OvsdbConnection");
}

void OvsdbConnection::connect_cb(uv_async_t* handle) {
    OvsdbConnection* ocp = (OvsdbConnection*)handle->data;
    if (ocp->ovsdbUseLocalTcpPort) {
        ocp->peer = yajr::Peer::create("127.0.0.1",
                                       "6640",
                                       on_state_change,
                                       ocp, loop_selector, false);
    } else {
        std::string swPath;
        swPath.append(ovs_rundir()).append("/db.sock");
        ocp->peer = yajr::Peer::create(swPath, on_state_change,
                                       ocp, loop_selector, false);
    }
    assert(ocp->peer);
}

void OvsdbConnection::stop() {
    uv_close((uv_handle_t*)&connect_async, nullptr);
    uv_close((uv_handle_t*)&send_req_async, nullptr);
    if (peer) {
        peer->destroy();
    }
    yajr::finiLoop(client_loop);
    threadManager.stopTask("OvsdbConnection");
}

 void OvsdbConnection::on_state_change(yajr::Peer * p, void * data,
                     yajr::StateChange::To stateChange,
                     int error) {
    auto* conn = (OvsdbConnection*)data;
    switch (stateChange) {
        case yajr::StateChange::CONNECT:
            conn->setConnected(true);
            LOG(INFO) << "New client connection";
            conn->ready.notify_all();
            break;
        case yajr::StateChange::DISCONNECT:
            conn->setConnected(false);
            LOG(INFO) << "Disconnected";
            break;
        case yajr::StateChange::TRANSPORT_FAILURE:
            conn->setConnected(false);
            {
              LOG(ERROR)  << "SSL Connection error: ";
            }
            break;
        case yajr::StateChange::FAILURE:
            conn->setConnected(false);
            LOG(ERROR) << "Connection error: " << uv_strerror(error);
            break;
        case yajr::StateChange::DELETE:
            conn->setConnected(false);
            LOG(INFO) << "Connection closed";
            break;
    }
}

uv_loop_t* OvsdbConnection::loop_selector(void* data) {
    auto* jRpc = (OvsdbConnection*)data;
    return jRpc->client_loop;
}

void OvsdbConnection::connect() {
    connect_async.data = this;
    uv_async_send(&connect_async);
}

void OvsdbConnection::disconnect() {
    // TODO
}

void OvsdbConnection::handleTransaction(uint64_t reqId,  const rapidjson::Document& payload) {
    unique_lock<mutex> lock(transactionMutex);
    auto iter = transactions.find(reqId);
    if (iter != transactions.end()) {
        iter->second->handleTransaction(reqId, payload);
        transactions.erase(iter);
    } else {
        LOG(WARNING) << "Unable to find reqId " << reqId;
    }
}

}