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
#include <thread>

#include <yajr/rpc/methods.hpp>

#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/ProcessorMessage.h"
#include "opflex/engine/internal/OvsdbConnection.h"
#include "rpc/JsonRpc.h"

#include "opflex/logging/internal/logging.hpp"
#include <yajr/yajr.hpp>
#include <yajr/rpc/rpc.hpp>

#include <rapidjson/document.h>

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

void TransactReq::serializePayload(MessageWriter& writer) {
    LOG(DEBUG) << "serializePayload message writer";
    (*this)(writer);
}

JsonReq::JsonReq(list<transData> tl) : OpflexMessage("transact", REQUEST)
{
    for (auto elem : tl) {
        shared_ptr<TransactReq> pTr = make_shared<TransactReq>(elem);
        transList.push_back(pTr);
    }
}

void JsonReq::serializePayload(yajr::rpc::SendHandler& writer) {
    LOG(DEBUG) << "serializePayload send handler";
    (*this)(writer);
}

void JsonReq::serializePayload(MessageWriter& writer) {
    LOG(DEBUG) << "serializePayload message writer";
    (*this)(writer);
}

void JsonReq::addTransaction(shared_ptr<TransactReq> req) {
    transList.push_back(req);
}

void sendTransaction(TransactReq& req, yajr::Peer *p, uint64_t reqId) {
    VLOG(5) << "send transaction";
        yajr::rpc::MethodName method(req.getMethod().c_str());
        PayloadWrapper wrapper(&req);
        yajr::rpc::OutboundRequest outm(wrapper, &method, reqId, p);
        outm.send();
}
void sendTransaction(JsonReq& req, std::shared_ptr<RpcConnection> p, uint64_t reqId) {
    VLOG(5) << "send transaction";
    shared_ptr<OvsdbConnection> pOv = dynamic_pointer_cast<OvsdbConnection>(p);
        yajr::rpc::MethodName method(req.getMethod().c_str());
        PayloadWrapper wrapper(&req);
        yajr::rpc::OutboundRequest outm(wrapper, &method, reqId, pOv->getPeer());
        outm.send();
}

void sendTransaction(const list<transData>& tl,shared_ptr<RpcConnection> p,
        const uint64_t& reqId) {
    shared_ptr<JsonReq> req = make_shared<JsonReq>(tl);
    sendTransaction(*req, p, reqId);
}

/**
 * walks the Value hierarchy according to the indices passed in the list
 * to retrieve the Value.
 * @param[in] val the Value tree to be walked
 * @param[in] idx list of indices to walk the tree.
 * @return a tuple: bool is true if Value found, false otherwise
 *         the Value or NULL.
 */
Value getValue(const Value& val, const list<string>& idx) {
    stringstream ss;
    for (auto str : idx) {
        ss << " " << str << ", ";
    }
    LOG(DEBUG) << ss.rdbuf();
    Document d;
    Document::AllocatorType& alloc = d.GetAllocator();
    Value tmpVal(Type::kNullType);
    //Value tmpVal;
    if (val == NULL || !val.IsArray()) {
        return tmpVal;
    }
    // if string is a number, treat it as index array.
    // otherwise its an object name.
    int index;
    tmpVal.CopyFrom(val, alloc);
    for (list<string>::const_iterator itr=idx.begin(); itr != idx.end();
            itr++) {
        LOG(DEBUG) << "index " << *itr;
        bool isArr = false;
        try {
            index = stoi(*itr);
            isArr = true;
            LOG(DEBUG) << "Is array";
        } catch (const invalid_argument& e) {
            // must be object name.
            LOG(DEBUG) << "Is not array";
        }

        if (isArr) {
            //int arrSize = tmpVal.GetArray().Size();
            int arrSize = tmpVal.Size();
            LOG(DEBUG) << "arr size " << arrSize;
            if ((arrSize - 1) < index) {
                LOG(DEBUG) << "arr size is less than index";
                // array size is less than the index we are looking for
                tmpVal.SetNull();
                return tmpVal;
            }
            tmpVal = tmpVal[index];
        } else if (tmpVal.IsObject()) {
            if (tmpVal.HasMember((*itr).c_str())) {
                Value::ConstMemberIterator itrMem =
                        tmpVal.FindMember((*itr).c_str());
                if (itrMem != tmpVal.MemberEnd()) {
                    LOG(DEBUG) << "obj name << " << itrMem->name.GetString();
                    tmpVal.CopyFrom(itrMem->value, alloc);
                } else {
                    // member not found
                    tmpVal.RemoveAllMembers();
                    return tmpVal;
                }
            } else {
                tmpVal.RemoveAllMembers();
                return tmpVal;
            }
        } else {
            LOG(DEBUG) << "Value is not array or object";
            // some primitive type, should not hit this before
            // list iteration is over.
            tmpVal.SetNull();
            return tmpVal;
        }
    }
    return tmpVal;
}

template <typename T>
bool TransactReq::operator()(rapidjson::Writer<T> & writer) {
    for (auto pair : tData.kvPairs) {
        writer.String(pair.first.c_str());
        writer.String(pair.second.c_str());
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
        for (set<string>::iterator it = tData.columns.begin();
                it != tData.columns.end(); ++it) {
            string tmp = *it;
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

            shared_ptr<BaseData> bPtr = itr->second;
            if (bPtr->type == BaseData::Dtype::TUPLE) {
                LOG(DEBUG) << "map";
                shared_ptr<TupleData> mPtr =
                        dynamic_pointer_cast<TupleData>(itr->second);
                if (!mPtr->label.empty()) {
                    writer.StartArray();
                    writer.String(mPtr->label.c_str());
                    writer.StartArray();
                    LOG(DEBUG) << "label " << mPtr->label;
                }
                for (auto val : mPtr->tset)
                {
                        writer.StartArray();
                        writer.String(get<0>(val).c_str());
                        writer.String(get<1>(val).c_str());
                        LOG(DEBUG) << "first " << get<0>(val)
                                   << ", second " << get<1>(val);
                        writer.EndArray();
                }
                if (!mPtr->label.empty()) {
                    writer.EndArray();
                    writer.EndArray();
                }
            } else if (bPtr->type == BaseData::Dtype::STRING) {
                shared_ptr<StringData> sPtr =
                        dynamic_pointer_cast<StringData>(itr->second);
                writer.String(sPtr->data.c_str());
                LOG(DEBUG) << "string " << sPtr->data;
            }

        }
        writer.EndObject();
    }
    return true;
}

void OvsdbConnection::idle_cb(uv_idle_t* handle) {
    OvsdbConnection* conn = (OvsdbConnection*)handle->data;
    conn->idle_count++;
    if (conn->idle_count % 10000 == 0) {
        //LOG(DEBUG) << "idle count " << conn->idle_count;
    }

}

void OvsdbConnection::start() {

    LOG(DEBUG) << "Starting .....";
    unique_lock<mutex> lock(mtx);
    client_loop = threadManager.initTask("OvsdbConnection");
    yajr::initLoop(client_loop);
    uv_idle_init(client_loop, &idler);
    idler.data = (void*)this;
    uv_idle_start(&idler, idle_cb);

    threadManager.startTask("OvsdbConnection");
}


void OvsdbConnection::stop() {
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
            //uv_idle_stop(&(conn->idler));
            LOG(INFO) << "New client connection";
            conn->ready.notify_all();

            break;
        case yajr::StateChange::DISCONNECT:
            uv_idle_stop(&(conn->idler));
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

void OvsdbConnection::handleTransaction(uint64_t reqId,
            const rapidjson::Value& payload) {
    VLOG(5) << "thread " << this_thread::get_id();
    pTrans->handleTransaction(reqId, payload);
}

void OvsdbConnection::connect(string const& hostname, int port) {

    peer = yajr::Peer::create(hostname,
                               boost::lexical_cast<string>(port),
                               on_state_change,
                               this, loop_selector, false);

}

std::shared_ptr<RpcConnection> createConnection(Transaction& trans) {
    return make_shared<OvsdbConnection>(&trans);
}

}
}
}

