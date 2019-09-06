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
    /*
    TransactReq(const string& oper,
             const string& tab,
             const set<tuple<string, string, string>> cond);

    TransactReq(const string& oper,
             const string& tab);
*/
//    virtual ~TransactReq() {};
//    virtual uint64_t getReqXid() { return xid; }

//    TransactReq(const TransactReq* req);

    virtual void serializePayload(yajr::rpc::SendHandler& writer);

    virtual void serializePayload(MessageWriter& writer);

    virtual TransactReq* clone(){
        return new TransactReq(*this);
    }

    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer);
    /*
    {
        for (auto pair : kvPairs) {
            writer.String(pair.first.c_str());
            writer.String(pair.second.c_str());
        }
        if (operation != "insert") {
            writer.String("where");
            writer.StartArray();
            if (!conditions.empty()) {
                for (auto elem : conditions) {
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
        writer.String(table.c_str());
        writer.String("op");
        writer.String(operation.c_str());
        if (!columns.empty()) {
            writer.String("columns");
            writer.StartArray();
            for (set<string>::iterator it = columns.begin();
                    it != columns.end(); ++it) {
                string tmp = *it;
                writer.String(tmp.c_str());
            }
            writer.EndArray();
        }
        if (!rows.empty()) {
            writer.String("row");
            writer.StartObject();
            for (row_map::iterator itr = rows.begin();
                    itr != rows.end(); ++itr) {
                string col = itr->first;
                writer.String(col.c_str());
                /*
                if (get<0>(get<0>(itr.second)) == COLLECTION::SET) {
                    writer.StartArray();
                    writer.String("set");
                    writer.StartArray();
                    for (auto val : get<1>(itr.second)) {
                        if (get<1>(get<0>(itr.second)) == TYPE::UUID) {
                            writer.StartArray();
                            writer.String("uuid");
                            writer.String(val.c_str());
                            writer.EndArray();
                        } else {
                            writer.String(val.c_str());
                        }
                    }
                    writer.EndArray();
                    writer.EndArray();
                    */
    /*
                shared_ptr<BaseData> bPtr = itr->second;
                if (bPtr->type == BaseData::Dtype::MAP) {
                    shared_ptr<MapData> mPtr =
                            dynamic_pointer_cast<MapData>(itr->second);
                    if (!mPtr->label.empty()) {
                        writer.StartArray();
                        writer.String(mPtr->label.c_str());
                    }
                    writer.StartArray();
                    for (auto val : mPtr->rmap)
                    {
                            writer.StartArray();
                            writer.String(val.first.c_str());
                            writer.String(val.second.c_str());
                            writer.EndArray();
                    }
                    writer.EndArray();
                    if (!mPtr->label.empty()) {
                        writer.EndArray();
                    }
                } else if (bPtr->type == BaseData::Dtype::STRING) {
                    shared_ptr<StringData> sPtr =
                            dynamic_pointer_cast<StringData>(itr->second);
                    writer.String(sPtr->data.c_str());
                }
*/
                /*
                (get<1>(get<0>(itr->second)) == TYPE::NAMED_UUID ||
                        get<1>(get<0>(itr->second)) == TYPE::UUID) {
                    string rval = dynamic_cast<string>((get<1>(itr->second)));
                    writer.StartArray();
                    string lval = (get<1>(get<0>(itr->second)) == TYPE::UUID ?
                            "uuid" : "named-uuid");
                    writer.String(lval.c_str());
                    writer.String(rval.c_str());
                    writer.EndArray();
                } else {
                    writer.String(dynamic_cast<string>(get<1>(itr->second)).c_str());
                }
            }
            */
    /*
            writer.EndObject();
        }
    }
    }

    /*
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
//        writer.StartArray();
//        writer.String("Open_vSwitch");
//        writer.StartObject();
        if (operation != "insert") {
            writer.String("where");
            writer.StartArray();
            if (!conditions.empty()) {
                for (auto elem : conditions) {
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
        writer.String(table.c_str());
        writer.String("op");
        writer.String(operation.c_str());
        if (!columns.empty()) {
            writer.String("columns");
            writer.StartArray();
            for (set<string>::iterator it = columns.begin();
                    it != columns.end(); ++it) {
                string tmp = *it;
                writer.String(tmp.c_str());
            }
            writer.EndArray();
        }
        if (!rows.empty()) {
            writer.String("row");
            writer.StartObject();
            for (auto itr : rows) {
                string col = itr.first;
                writer.String(col.c_str());
                if (get<0>(get<0>(itr.second)) == COLLECTION::SET) {
                    writer.StartArray();
                    writer.String("set");
                    writer.StartArray();
                    for (auto val : get<1>(itr.second)) {
                        if (get<1>(get<0>(itr.second)) == TYPE::UUID) {
                            writer.StartArray();
                            writer.String("uuid");
                            writer.String(val.c_str());
                            writer.EndArray();
                        } else {
                            writer.String(val.c_str());
                        }
                    }
                    writer.EndArray();
                    writer.EndArray();
                } else {
                    writer.String(get<1>(itr.second).front().c_str());
                }
            }
            writer.EndObject();
        }
        writer.EndObject();
        writer.EndArray();


        return true;
    }
    */

//void addKvPair(string lval, string rval);

private:
    //map<string, string> kvPairs;

public:

/*
    class RowData {
    public:
        enum class Type { STRING, MAP };
        Type type;
        RowData(map<string, string> m) : rdata(m), type(Type::MAP) {}
        RowData(string s) : rdata(s), type(Type::STRING) {}
        ~RowData() {
            if (type == Type::STRING) {
                rdata.str.~basic_string();
            } else if (type == Type::MAP) {
                rdata.rmap.~map();
            }
        }
        string getString() { return rdata.str; }
        map<string,string>& getMap() { return rdata.rmap;}
    private:
        union Rdata {
            string str;
            map<string,string> rmap;
            Rdata(map<string, string> m) {
                new (&rmap) map<string, string>;
                rmap.insert(m.begin(), m.end());
            }
            Rdata(string s) {
                new (&str) string(s);
            }
            ~Rdata() {}
        } rdata;
    };
    */
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
/*
    typedef tuple<COLLECTION, TYPE> data_type;
    typedef map<string, string> map_data;
    //union data { map_data data; string str; ~data() {}};
    union data { map_data m_data; string str; ~data() {}; data() {}};
    typedef tuple<data_type, data> row_data;
    //typedef map<string, row_data> row_map;
    typedef map<string, shared_ptr<BaseData>> row_map;

    set<tuple<string, string, string>> conditions;
    string operation;
    string table;
    set<string> columns;
    row_map rows;
*/
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
