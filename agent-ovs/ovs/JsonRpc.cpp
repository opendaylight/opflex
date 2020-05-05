/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file JsonRpc.cpp
 * @brief Implementation of interface for various JSON/RPC messages used by the
 * engine
 */
/*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <set>
#include <map>
#include <tuple>
#include <regex>
#include <string>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include "JsonRpc.h"
#include <opflexagent/logging.h>
#include "OvsdbConnection.h"

using namespace std::chrono;

namespace opflexagent {

using namespace opflex::jsonrpc;
using namespace rapidjson;

void JsonRpc::handleTransaction(uint64_t reqId, const Document& payload) {
    pResp.reset(new Response(reqId, payload));
    // TODO - generically check payload of response for errors and log
    responseReceived = true;
    conn->ready.notify_all();
}

bool JsonRpc::createNetFlow(const string& brUuid, const string& target, const int& timeout, bool addidtointerface ) {
    vector<TupleData> tuples;
    tuples.emplace_back("", target);
    TupleDataSet tdSet(tuples);
    JsonRpcTransactMessage msg1(OvsdbOperation::INSERT, OvsdbTable::NETFLOW);
    msg1.rowData["targets"] = tdSet;

    tuples.clear();
    tuples.emplace_back("", timeout);
    tdSet = TupleDataSet(tuples);
    msg1.rowData["active_timeout"] = tdSet;

    tuples.clear();
    tuples.emplace_back("", addidtointerface);
    tdSet = TupleDataSet(tuples);
    msg1.rowData["add_id_to_interface"] = tdSet;

    const string uuid_name = "netflow1";
    msg1.kvPairs.emplace_back("uuid-name", uuid_name);

    JsonRpcTransactMessage msg2(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("_uuid", "==", brUuid);
    msg2.conditions = condSet;

    tuples.clear();
    tuples.emplace_back("named-uuid", uuid_name);
    tdSet = TupleDataSet(tuples);
    msg2.rowData.emplace("netflow", tdSet);

    const list<JsonRpcTransactMessage> requests = {msg1, msg2};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::createIpfix(const string& brUuid, const string& target, const int& sampling) {
    vector<TupleData> tuples;
    tuples.emplace_back("", target);
    TupleDataSet tdSet(tuples);
    JsonRpcTransactMessage msg1(OvsdbOperation::INSERT, OvsdbTable::IPFIX);
    msg1.rowData.emplace("targets", tdSet);
    if (sampling != 0) {
        tuples.clear();
        tuples.emplace_back("", sampling);
        tdSet = TupleDataSet(tuples);
        msg1.rowData.emplace("sampling", tdSet);
    }
    const string uuid_name = "ipfix1";
    msg1.kvPairs.emplace_back("uuid-name", uuid_name);

    JsonRpcTransactMessage msg2(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("_uuid", "==", brUuid);
    msg2.conditions = condSet;

    tuples.clear();
    tuples.emplace_back("named-uuid", uuid_name);
    tdSet = TupleDataSet(tuples);
    msg2.rowData.emplace("ipfix", tdSet);

    const list<JsonRpcTransactMessage> requests = {msg1, msg2};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::deleteNetFlow(const string& brName) {
    JsonRpcTransactMessage msg1(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("name", "==", brName);
    msg1.conditions = condSet;

    vector<TupleData> tuples;
    TupleDataSet tdSet(tuples, "set");
    msg1.rowData.emplace("netflow", tdSet);

    list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::deleteIpfix(const string& brName) {
    JsonRpcTransactMessage msg1(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("name", "==", brName);
    msg1.conditions = condSet;

    vector<TupleData> tuples;
    TupleDataSet tdSet(tuples, "set");
    msg1.rowData.emplace("ipfix", tdSet);

    const list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::handleGetPortUuidResp(uint64_t reqId,
        const Document& payload, string& uuid) {
    if (payload.IsArray()) {
        list<string> ids = { "0","rows","0","_uuid","1" };
        Value val;
        opflexagent::getValue(payload, ids, val);
        if (!val.IsNull()) {
            uuid = val.GetString();
            return true;
        }
    } else {
        LOG(WARNING) << "payload is not an array";
    }
    return false;
}

bool JsonRpc::updateBridgePorts(
    const string& brUuid, set<string>& brPorts, const string& port, bool action) {
    if (action) {
        // add port to list
        brPorts.emplace(port);
    } else {
        // remove port from list
        brPorts.erase(port);
    }
    JsonRpcTransactMessage msg1(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("_uuid", "==", brUuid);
    msg1.conditions = condSet;
    vector<TupleData> tuples;
    tuples.reserve(brPorts.size());
    for (auto& elem : brPorts) {
        tuples.emplace_back("uuid", elem);
    }
    TupleDataSet tdSet(tuples, "set");
    msg1.rowData.emplace("ports", tdSet);

    const list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::handleGetBridgePortList(uint64_t reqId, const Document& payload, BrPortResult& result) {
    set<string> brPortSet;
    list<string> ids = {"0","rows","0","ports","0"};
    Value val;
    opflexagent::getValue(payload, ids, val);
    if (!val.IsNull() && val.IsString()) {
        const string valStr(val.GetString());
        ids = {"0", "rows", "0", "ports", "1"};
        val.SetObject();
        opflexagent::getValue(payload, ids, val);
        if (valStr == "uuid") {
            brPortSet.emplace(val.GetString());
        }
    } else {
        LOG(WARNING) << "Error getting port uuid";
        return false;
    }

    if (val.IsArray()) {
        for (Value::ConstValueIterator itr1 = val.Begin();
             itr1 != val.End(); itr1++) {
            brPortSet.emplace((*itr1)[1].GetString());
        }
    }
    ids = {"0", "rows", "0", "_uuid", "1"};
    Value val3;
    opflexagent::getValue(payload, ids, val3);
    string brPortUuid = val3.GetString();
    tuple<string, set<string>> brPorts = make_tuple(brPortUuid, brPortSet);

    result.brUuid = brPortUuid;
    result.portUuids.insert(brPortSet.begin(), brPortSet.end());
    return true;
}

void JsonRpc::getUuidsFromVal(set<string>& uuidSet, const Document& payload, const string& index) {
    LOG(INFO) << "getUuidsFromVal with index " << index;
    list<string> ids = {"0","rows","0",index,"0"};
    Value val;
    opflexagent::getValue(payload, ids, val);
    if (!val.IsNull() && val.IsString()) {
        const string valStr(val.GetString());
        ids = {"0", "rows", "0", index, "1"};
        val.SetObject();
        opflexagent::getValue(payload, ids, val);
        if (valStr == "uuid") {
            uuidSet.emplace(valStr);
        }
    } else {
        LOG(WARNING) << "Error getting port uuid";
        return;
    }

    if (val.IsArray()) {
        for (Value::ConstValueIterator itr1 = val.Begin(); itr1 != val.End(); itr1++) {
            uuidSet.emplace((*itr1)[1].GetString());
        }
    }
}

bool JsonRpc::getBridgePortList(const string& bridge, BrPortResult& res) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("name", "==", bridge);
    msg1.conditions = condSet;
    msg1.columns.emplace("ports");
    msg1.columns.emplace("_uuid");
    const list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }

    return handleGetBridgePortList(pResp->reqId, pResp->payload, res);
}

bool JsonRpc::getOvsdbMirrorConfig(mirror& mir) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::MIRROR);
    const list<JsonRpcTransactMessage> requests1 = {msg1};
    if (!sendRequestAndAwaitResponse(requests1)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    if (!handleMirrorConfig(pResp->reqId, pResp->payload, mir)) {
        return false;
    }
    // collect all port UUIDs in a set and query
    // OVSDB for names.
    set<string> uuids;
    uuids.insert(mir.src_ports.begin(), mir.src_ports.end());
    uuids.insert(mir.dst_ports.begin(), mir.dst_ports.end());
    uuids.insert(mir.out_port);

    JsonRpcTransactMessage msg2(OvsdbOperation::SELECT, OvsdbTable::PORT);
    msg2.columns.emplace("name");
    msg2.columns.emplace("_uuid");
    const list<JsonRpcTransactMessage> requests2 = {msg2};
    if (!sendRequestAndAwaitResponse(requests2)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    unordered_map<string, string> portMap;
    if (!getPortList(pResp->reqId, pResp->payload, portMap)) {
        LOG(DEBUG) << "Unable to get port list";
        return false;
    }
    //replace port UUIDs with names in the mirror struct
    substituteSet(mir.src_ports, portMap);
    substituteSet(mir.dst_ports, portMap);
    auto itr = portMap.find(mir.out_port);
    if (itr != portMap.end()) {
        LOG(DEBUG) << "out_port name " << itr->second;
        mir.out_port = itr->second;
    }
    return true;
}

bool JsonRpc::getCurrentErspanParams(const string& portName, ErspanParams& params) {
    // for ERSPAN port get IP address
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::INTERFACE);
    tuple<string, string, string> cond1("name", "==", portName);
    set<tuple<string, string, string>> condSet;
    condSet.emplace(cond1);
    msg1.conditions = condSet;
    msg1.columns.emplace("options");

    list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    if (!getErspanOptions(pResp->reqId, pResp->payload, params)) {
        LOG(DEBUG) << "failed to get ERSPAN options";
        return false;
    }
    return true;
}

void JsonRpc::substituteSet(set<string>& s, const unordered_map<string, string>& portMap) {
    set<string> names;
    for (const auto& elem : s) {
        LOG(DEBUG) << "uuid " << elem;
        auto itr  = portMap.find(elem);
        if (itr != portMap.end()) {
            LOG(DEBUG) << "name " << itr->second;
            names.insert(itr->second);
        }
    }
    s.clear();
    s.insert(names.begin(), names.end());
}

bool JsonRpc::handleMirrorConfig(uint64_t reqId, const Document& payload, mirror& mir) {
    set<string> uuids;
    getUuidsFromVal(uuids, payload, "_uuid");
    if (uuids.empty()) {
        LOG(DEBUG) << "no mirror found";
        return false;
    }

    mir.uuid = *(uuids.begin());
    getUuidsFromVal(uuids, payload, "select_src_port");

    mir.src_ports.insert(uuids.begin(), uuids.end());
    uuids.clear();
    getUuidsFromVal(uuids, payload, "select_dst_port");

    mir.dst_ports.insert(uuids.begin(), uuids.end());
    uuids.clear();
    getUuidsFromVal(uuids, payload, "output_port");

    if (!uuids.empty()) {
        mir.out_port = *(uuids.begin());
    }
    return true;
}

bool JsonRpc::getPortList(const uint64_t reqId, const Document& payload, unordered_map<string, string>& portMap) {
    if (payload.IsArray()) {
        list<string> ids = { "0","rows"};
        Value arr;
        opflexagent::getValue(payload, ids, arr);
        if (arr.IsNull() || !arr.IsArray()) {
            LOG(DEBUG) << "expected array";
            return false;
        }
        for (Value::ConstValueIterator itr1 = arr.Begin();
             itr1 != arr.End(); itr1++) {
            if (itr1->HasMember("name") && itr1->HasMember("_uuid")) {
                LOG(DEBUG) << (*itr1)["name"].GetString() << ":"
                           << (*itr1)["_uuid"][1].GetString();
                portMap.emplace((*itr1)["_uuid"][1].GetString(),
                             (*itr1)["name"].GetString());
            } else {
                LOG(WARNING) << "Result missing name/uuid";
                return false;
            }
        }
        return true;
    } else {
        LOG(DEBUG) << "payload is not an array";
        return false;
    }
}

bool JsonRpc::getErspanOptions(const uint64_t reqId, const Document& payload, ErspanParams& params) {
    if (!payload.IsArray()) {
        LOG(DEBUG) << "payload is not an array";
        return false;
    }
    list<string> ids = {"0","rows","0","options","1"};
    Value arr;
    opflexagent::getValue(payload, ids, arr);
    if (arr.IsNull() || !arr.IsArray()) {
        LOG(DEBUG) << "expected array";
        return false;
    }
    for (Value::ConstValueIterator itr1 = arr.Begin(); itr1 != arr.End(); itr1++) {
        string val((*itr1)[1].GetString());
        string index((*itr1)[0].GetString());
        if (index == "erspan_ver") {
            params.setVersion(stoi(val));
        } else if (index == "remote_ip") {
            params.setRemoteIp(val);
        }
    }
    return (params.getVersion() != 0);
}

void JsonRpc::getPortUuid(const string& name, string& uuid) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::PORT);
    tuple<string, string, string> cond1("name", "==", name);
    set<tuple<string, string, string>> condSet;
    condSet.emplace(cond1);
    msg1.conditions = condSet;

    const list<JsonRpcTransactMessage> requests{msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(WARNING) << "Error sending message";
        return;
    }

    if (!handleGetPortUuidResp(pResp->reqId, pResp->payload,uuid)) {
        LOG(WARNING) << "Unable to extract UUID from response";
    }
}

void JsonRpc::getPortUuids(map<string, string>& ports) {
    for (auto& port : ports) {
        string uuid;
        getPortUuid(port.first, uuid);
        if (!uuid.empty()) {
            port.second = uuid;
        }
    }
}

void JsonRpc::getBridgeUuid(const string& name, string& uuid) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("name", "==", name);
    msg1.conditions = condSet;
    msg1.columns.emplace("_uuid");

    const list<JsonRpcTransactMessage> requests{msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
    }
    handleGetBridgeUuidResp(pResp->reqId, pResp->payload, uuid);
}

void JsonRpc::handleGetBridgeUuidResp(uint64_t reqId, const Document& payload, string& uuid) {
    list<string> ids = {"0","rows","0","_uuid","1"};
    Value val;
    opflexagent::getValue(payload, ids, val);
    if (!val.IsNull() && val.IsString()) {
        uuid = val.GetString();
    } else {
        LOG(WARNING) << "Unable to extract UUID from response for request " << reqId;
    }
}

bool JsonRpc::createMirror(const string& brUuid, const string& name, const set<string>& srcPorts,
                           const set<string>& dstPorts) {
    map<string, string> portUuidMap;
    set<string> ports;
    ports.insert(srcPorts.begin(), srcPorts.end());
    ports.insert(dstPorts.begin(), dstPorts.end());
    ports.insert(ERSPAN_PORT_PREFIX + name);
    for (const auto& port : ports) {
        portUuidMap.emplace(port, "");
    }
    getPortUuids(portUuidMap);

    JsonRpcTransactMessage msg1(OvsdbOperation::INSERT, OvsdbTable::MIRROR);

    vector<TupleData> tuples;
    // src ports
    set<tuple<string,string>> rdata;
    populatePortUuids(srcPorts, portUuidMap, rdata);
    tuples.reserve(rdata.size());
    for (auto pair : rdata) {
        LOG(INFO) << "entry " << get<0>(pair).c_str() << " - " << get<1>(pair).c_str();
        const string val = get<1>(pair);
        tuples.emplace_back(get<0>(pair).c_str(), val);
    }
    LOG(INFO) << "mirror src_port size " << tuples.size();
    TupleDataSet tdSet(tuples, "set");
    msg1.rowData.emplace("select_src_port", tdSet);

    // dst ports
    rdata.clear();
    tuples.clear();
    populatePortUuids(dstPorts, portUuidMap, rdata);
    for (auto pair : rdata) {
        LOG(INFO) << "entry " << get<0>(pair).c_str() << " - " << get<1>(pair).c_str();
        const string val = get<1>(pair);
        tuples.emplace_back(get<0>(pair).c_str(), val);
    }
    LOG(INFO) << "mirror dst_port size " << tuples.size();
    tdSet = TupleDataSet(tuples, "set");
    msg1.rowData.emplace("select_dst_port", tdSet);

    // output ports
    string outputPortUuid = portUuidMap[ERSPAN_PORT_PREFIX + name];
    LOG(WARNING) << "output port uuid " << outputPortUuid;

    tuples.clear();
    tuples.emplace_back("uuid", outputPortUuid);
    tdSet = TupleDataSet(tuples);
    msg1.rowData.emplace("output_port", tdSet);

    // name
    tuples.clear();
    tuples.emplace_back("", name);
    tdSet = TupleDataSet(tuples);
    msg1.rowData.emplace("name", tdSet);

    const string uuid_name = "mirror1";
    msg1.kvPairs.emplace_back("uuid-name", uuid_name);

    // msg2
    tuples.clear();
    JsonRpcTransactMessage msg2(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("_uuid", "==", brUuid);
    msg2.conditions = condSet;
    tuples.emplace_back("named-uuid", uuid_name);
    tdSet = TupleDataSet(tuples);
    msg2.rowData.emplace("mirrors", tdSet);

    const list<JsonRpcTransactMessage> requests = {msg1, msg2};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return handleCreateMirrorResp(pResp->reqId, pResp->payload);
}

bool JsonRpc::addErspanPort(const string& bridge, ErspanParams& params) {
    JsonRpcTransactMessage msg1(OvsdbOperation::INSERT, OvsdbTable::PORT);
    vector<TupleData> tuples;
    tuples.emplace_back("", params.getPortName());
    TupleDataSet tdSet(tuples);
    msg1.rowData.emplace("name", tdSet);

    // uuid-name
    const string uuid_name = "port1";
    msg1.kvPairs.emplace_back("uuid-name", uuid_name);

    // interfaces
    tuples.clear();
    const string named_uuid = "interface1";
    tuples.emplace_back("named-uuid", named_uuid);
    tdSet = TupleDataSet(tuples);
    msg1.rowData.emplace("interfaces", tdSet);

    // uuid-name
    JsonRpcTransactMessage msg2(OvsdbOperation::INSERT, OvsdbTable::INTERFACE);
    msg2.kvPairs.emplace_back("uuid-name", named_uuid);

    // row entries
    // name
    tuples.clear();
    tuples.emplace_back("", params.getPortName());
    tdSet = TupleDataSet(tuples);
    msg2.rowData.emplace("name", tdSet);

    tuples.clear();
    const string typeString("erspan");
    TupleData typeData("", typeString);
    tuples.push_back(typeData);
    tdSet = TupleDataSet(tuples);
    msg2.rowData.emplace("type", tdSet);

    // options depend upon version
    tuples.clear();
    tuples.emplace_back("erspan_ver", std::to_string(params.getVersion()));
    tuples.emplace_back("remote_ip", params.getRemoteIp());
    tdSet = TupleDataSet(tuples, "map");
    msg2.rowData.emplace("options", tdSet);

    // get bridge port list and add erspan port to it.
    BrPortResult res;
    if (!getBridgePortList(bridge, res)) {
        return false;
    }

    JsonRpcTransactMessage msg3(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    tuples.clear();
    for (const auto& elem : res.portUuids) {
        tuples.emplace_back("uuid", elem);
    }
    tuples.emplace_back("named-uuid", uuid_name);
    tdSet = TupleDataSet(tuples, "set");
    msg3.rowData.emplace("ports", tdSet);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("_uuid", "==", res.brUuid);
    msg3.conditions = condSet;

    const list<JsonRpcTransactMessage> requests = {msg1, msg2, msg3};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

inline void JsonRpc::populatePortUuids(const set<string>& ports, const map<string, string>& uuidMap, set<tuple<string,string>>& entries) {
    for (const auto& port : ports) {
        auto itmap = uuidMap.find(port);
        if (itmap != uuidMap.end()) {
            if (!itmap->second.empty())
                entries.emplace("uuid",itmap->second);
        }
    }
}

bool JsonRpc::handleCreateMirrorResp(uint64_t reqId, const Document& payload) {
    list<string> ids = {"0","uuid","1"};
    Value val;
    opflexagent::getValue(payload, ids, val);
    if (!val.IsNull() && val.IsString()) {
        LOG(DEBUG) << "mirror uuid " << val.GetString();
    } else {
        return false;
    }
    return true;
}

bool JsonRpc::deleteMirror(const string& brName) {
    JsonRpcTransactMessage msg(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, string, string>> condSet;
    condSet.emplace("name", "==", brName);
    msg.conditions = condSet;

    vector<TupleData> tuples;
    TupleDataSet tdSet(tuples, "set");
    tdSet.label = "set";
    msg.rowData.emplace("mirrors", tdSet);

    list<JsonRpcTransactMessage> requests = {msg};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

void JsonRpc::connect() {
    conn->connect();
}

bool JsonRpc::isConnected() {
    unique_lock<mutex> lock(OvsdbConnection::ovsdbMtx);
    if (!conn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
        [=]{return conn->isConnected();})) {
        LOG(DEBUG) << "lock timed out, no connection";
        return false;
    }
    return true;
}

/**
 * walks the Value hierarchy according to the indices passed in the list
 * to retrieve the Value.
 * @param[in] val the Value tree to be walked
 * @param[in] idx list of indices to walk the tree.
 * @return a Value object.
 */
void getValue(const Document& val, const list<string>& idx, Value& result) {
    Document::AllocatorType& alloc = ((Document&)val).GetAllocator();
    Value tmpVal(Type::kNullType);
    if (val == NULL || !val.IsArray()) {
        result = tmpVal;
        return;
    }
    // if string is a number, treat it as index array.
    // otherwise its an object name.
    tmpVal.CopyFrom(val, alloc);
    for (const auto& itr : idx) {
        LOG(DEBUG) << "index " << itr;
        bool isArr = false;
        int index = 0;
        try {
            index = stoi(itr);
            isArr = true;
        } catch (const invalid_argument& e) {
            // must be object name.
        }

        if (isArr) {
            int arrSize = tmpVal.Size();
            LOG(DEBUG) << "arr size " << arrSize;
            if ((arrSize - 1) < index) {
                LOG(DEBUG) << "arr size is less than index";
                // array size is less than the index we are looking for
                tmpVal.SetNull();
                result = tmpVal;
                return;
            }
            tmpVal = tmpVal[index];
        } else if (tmpVal.IsObject()) {
            if (tmpVal.HasMember(itr.c_str())) {
                Value::ConstMemberIterator itrMem =
                        tmpVal.FindMember(itr.c_str());
                if (itrMem != tmpVal.MemberEnd()) {
                    LOG(DEBUG) << "obj name << " << itrMem->name.GetString();
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

} // namespace opflexagemt
