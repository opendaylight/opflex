/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of opflex messages for engine
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

#include <set>
#include <tuple>
#include <memory>

#include <yajr/rpc/methods.hpp>

#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/ProcessorMessage.h"
#include "opflex/engine/internal/OvsdbConnection.h"
#include "opflex/rpc/JsonRpc.h"

#include "opflex/logging/internal/logging.hpp"
#include <yajr/yajr.hpp>
#include <yajr/rpc/rpc.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

using  rapidjson::Writer;
using rapidjson::Value;
using rapidjson::Type;
using rapidjson::Document;

namespace opflex {
namespace engine {
namespace internal {

TransactReq::TransactReq(const transData& td) : OpflexMessage("transact", REQUEST),
        tData(td) {}

void TransactReq::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

JsonReq::JsonReq(const list<transData>& tl, uint64_t reqId)
    : OpflexMessage("transact", REQUEST), reqId(reqId)
{
    for (auto& elem : tl) {
        shared_ptr<TransactReq> pTr = make_shared<TransactReq>(elem);
        transList.push_back(pTr);
    }
}

void JsonReq::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

void OvsdbConnection::send_req_cb(uv_async_t* handle) {
    req_cb_data* reqCbd = (req_cb_data*)handle->data;
    JsonReq* req = reqCbd->req;
    yajr::rpc::MethodName method(req->getMethod().c_str());
    PayloadWrapper wrapper(req);
    yajr::rpc::OutboundRequest outr =
            yajr::rpc::OutboundRequest(wrapper, &method, req->reqId, reqCbd->peer);
    outr.send();
    delete(req);
    delete(reqCbd);
}

void OvsdbConnection::sendTransaction(const list<transData>& tl,
        const uint64_t& reqId) {
    req_cb_data* reqCbd = new req_cb_data();
    reqCbd->req = new JsonReq(tl, reqId);
    reqCbd->peer = getPeer();
    send_req_async.data = (void*)reqCbd;
    uv_async_send(&send_req_async);
}

/**
 * walks the Value hierarchy according to the indices passed in the list
 * to retrieve the Value.
 * @param[in] val the Value tree to be walked
 * @param[in] idx list of indices to walk the tree.
 * @return a Value object.
 */
void getValue(const Document& val, const list<string>& idx, Value& result) {
    stringstream ss;
    for (auto& str : idx) {
        ss << " " << str << ", ";
    }
    LOG(DEBUG4) << ss.rdbuf();
    Document::AllocatorType& alloc = ((Document&)val).GetAllocator();
    Value tmpVal(Type::kNullType);
    if (val == NULL || !val.IsArray()) {
        result = tmpVal;
        return;
    }
    // if string is a number, treat it as index array.
    // otherwise its an object name.
    int index;
    tmpVal.CopyFrom(val, alloc);
    for (list<string>::const_iterator itr=idx.begin(); itr != idx.end();
            itr++) {
        LOG(DEBUG4) << "index " << *itr;
        bool isArr = false;
        try {
            index = stoi(*itr);
            isArr = true;
            LOG(DEBUG4) << "Is array";
        } catch (const invalid_argument& e) {
            // must be object name.
            LOG(DEBUG4) << "Is not array";
        }

        if (isArr) {
            int arrSize = tmpVal.Size();
            LOG(DEBUG4) << "arr size " << arrSize;
            if ((arrSize - 1) < index) {
                LOG(DEBUG4) << "arr size is less than index";
                // array size is less than the index we are looking for
                tmpVal.SetNull();
                result = tmpVal;
                return;
            }
            tmpVal = tmpVal[index];
        } else if (tmpVal.IsObject()) {
            if (tmpVal.HasMember((*itr).c_str())) {
                Value::ConstMemberIterator itrMem =
                        tmpVal.FindMember((*itr).c_str());
                if (itrMem != tmpVal.MemberEnd()) {
                    LOG(DEBUG4) << "obj name << " << itrMem->name.GetString();
                    tmpVal.CopyFrom(itrMem->value, alloc);
                } else {
                    // member not found
                    tmpVal.RemoveAllMembers();
                    result = tmpVal;
                    return;
                }
            } else {
                tmpVal.RemoveAllMembers();
                result = tmpVal;
                return;
            }
        } else {
            LOG(DEBUG) << "Value is not array or object";
            // some primitive type, should not hit this before
            // list iteration is over.
            tmpVal.SetNull();
            result = tmpVal;
            return;
        }
    }
    result = tmpVal;
}

template<typename T>
void TransactReq::writePair(rapidjson::Writer<T>& writer, shared_ptr<BaseData> bPtr,
        bool kvPair) {
    if (bPtr->getType() == Dtype::INTEGER) {
        shared_ptr<TupleData<int>> tPtr =
                dynamic_pointer_cast<TupleData<int>>(bPtr);
        if (kvPair) {
            writer.String(get<0>(tPtr->data).c_str());
            writer.Int(get<1>(tPtr->data).val);
        } else {
            string str = get<0>(tPtr->data);
            if (!str.empty()) {
                writer.StartArray();
                writer.String(get<0>(tPtr->data).c_str());
            }
            writer.Int(get<1>(tPtr->data).val);
            if (!str.empty()) {
                writer.EndArray();
            }
        }
    } else if (bPtr->getType() == Dtype::STRING) {
        shared_ptr<TupleData<string>> tPtr =
                dynamic_pointer_cast<TupleData<string>>(bPtr);
        if (kvPair) {
            writer.String(get<0>(tPtr->data).c_str());
            writer.String(get<1>(tPtr->data).val.c_str());
        } else {
            string str = get<0>(tPtr->data);
            if (!str.empty()) {
                writer.StartArray();
                writer.String(get<0>(tPtr->data).c_str());
            }
            writer.String(get<1>(tPtr->data).val.c_str());
            if (!str.empty()) {
                writer.EndArray();
            }
        }
    } else if (bPtr->getType() == Dtype::BOOL) {
        shared_ptr<TupleData<bool>> tPtr =
                dynamic_pointer_cast<TupleData<bool>>(bPtr);
        if (kvPair) {
            writer.String(get<0>(tPtr->data).c_str());
            writer.Bool(get<1>(tPtr->data).val);
        } else {
            string str = get<0>(tPtr->data);
            if (!str.empty()) {
                writer.StartArray();
                writer.String(get<0>(tPtr->data).c_str());
            }
            writer.Bool(get<1>(tPtr->data).val);
            if (!str.empty()) {
                writer.EndArray();
            }
        }
    }
}

template <typename T>
bool TransactReq::operator()(rapidjson::Writer<T> & writer) {
    for (auto& pair : tData.kvPairs) {
        writePair<T>(writer, pair, true);
    }

    if (tData.operation != "insert") {
        writer.String("where");
        writer.StartArray();
        if (!tData.conditions.empty()) {
            for (auto elem : tData.conditions) {
                writer.StartArray();
                string lhs = get<0>(elem);
                writer.String(lhs.c_str());
                string condition = get<1>(elem);
                writer.String(condition.c_str());
                string rhs = get<2>(elem);
                if (lhs == "_uuid" ||
                        lhs == "mirrors") {
                    writer.StartArray();
                    writer.String("uuid");
                    writer.String(rhs.c_str());
                    writer.EndArray();
                } else {
                    writer.String(rhs.c_str());
                }
                writer.EndArray();
            }
        }
        writer.EndArray();
    }
    writer.String("table");
    writer.String(tData.table.c_str());
    writer.String("op");
    writer.String(tData.operation.c_str());
    if (!tData.columns.empty()) {
        writer.String("columns");
        writer.StartArray();
        for (auto& tmp : tData.columns) {
            writer.String(tmp.c_str());
        }
        writer.EndArray();
    }

    if (!tData.rows.empty()) {
        writer.String("row");
        writer.StartObject();
        for (row_map::iterator itr = tData.rows.begin();
                itr != tData.rows.end(); ++itr) {
            string col = itr->first;
            LOG(DEBUG) << "row label " << col;
            writer.String(col.c_str());
            shared_ptr<TupleDataSet> tdsPtr = itr->second;
            if (!tdsPtr->label.empty()) {
                writer.StartArray();
                writer.String(tdsPtr->label.c_str());
                writer.StartArray();
                LOG(DEBUG) << "label " << tdsPtr->label;

                for (auto& val : tdsPtr->tset) {
                    writePair<T>(writer, val, false);
                }
                writer.EndArray();
                writer.EndArray();
            } else {
                writePair(writer, *(tdsPtr->tset.begin()), false);
            }
        }
        writer.EndObject();
    }
    return true;
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
    VLOG(5) << ocp;
    // TODO - don't hardcode socket...this whole thing needs to moved out of libopflex
    std::string swPath("/usr/local/var/run/openvswitch/db.sock");
    ocp->peer = yajr::Peer::create(swPath, on_state_change,
                                   ocp, loop_selector, false);
    assert(ocp->peer);
}

void OvsdbConnection::stop() {
    uv_close((uv_handle_t*)&connect_async, NULL);
    uv_close((uv_handle_t*)&send_req_async, NULL);
    peer->destroy();
    yajr::finiLoop(client_loop);
    threadManager.stopTask("OvsdbConnection");
}

 void OvsdbConnection::on_state_change(yajr::Peer * p, void * data,
                     yajr::StateChange::To stateChange,
                     int error) {
        OvsdbConnection* conn = (OvsdbConnection*)data;
        LOG(DEBUG) << "conn ptr " << hex << conn;
        switch (stateChange) {
        case yajr::StateChange::CONNECT:
            conn->setConnected(true);
            LOG(INFO) << "New client connection";
            conn->ready.notify_all();

            break;
        case yajr::StateChange::DISCONNECT:
            conn->setConnected(false);
            LOG(INFO) << "Disconnected";
            //p->stopKeepAlive();
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
            //p->stopKeepAlive();
            break;
        }
}

uv_loop_t* OvsdbConnection::loop_selector(void* data) {
    OvsdbConnection* jRpc = (OvsdbConnection*)data;
    return jRpc->client_loop;
}

void RpcConnection::handleTransaction(uint64_t reqId,
            const rapidjson::Document& payload) {
    pTrans->handleTransaction(reqId, payload);
}

void OvsdbConnection::connect() {
    connect_async.data = this;
    uv_async_send(&connect_async);
}

std::shared_ptr<RpcConnection> createConnection(Transaction& trans) {
    return make_shared<OvsdbConnection>(&trans);
}

void MockRpcConnection::sendTransaction(const list<transData>& tl,
        const uint64_t& reqId) {
    ResponseDict& rDict = ResponseDict::Instance();
    auto itr = rDict.dict.find(reqId);
    if (itr != rDict.dict.end()) {
        handleTransaction(1, rDict.d[itr->second]);
    } else {
        LOG(DEBUG) << "No response found";
    }
}

void ResponseDict::init() {
    uint64_t j=1000;
    for (unsigned int i=0 ; i < no_of_msgs; i++, j++) {
        d[i].GetAllocator().Clear();
        d[i].Parse(response[i].c_str());
        dict.emplace(j, i);
    }
}

    ResponseDict& ResponseDict::Instance() {
        static ResponseDict inst;
        if (!inst.isInitialized) {
            inst.init();
            inst.isInitialized = true;
        }
        return inst;
    }
}
}
}

