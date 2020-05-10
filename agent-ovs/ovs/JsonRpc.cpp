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
#include <regex>
#include <rapidjson/stringbuffer.h>

#include "JsonRpc.h"

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
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("_uuid", OvsdbFunction::EQ, brUuid);
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

    tuples.clear();
    static const string enabled("true");
    tuples.emplace_back("enable-tunnel-sampling", enabled);
    tdSet = TupleDataSet(tuples, "map");
    msg1.rowData.emplace("other_config", tdSet);
    const string uuid_name = "ipfix1";
    msg1.kvPairs.emplace_back("uuid-name", uuid_name);

    JsonRpcTransactMessage msg2(OvsdbOperation::UPDATE, OvsdbTable::BRIDGE);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("_uuid", OvsdbFunction::EQ, brUuid);
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
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, brName);
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
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, brName);
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

bool JsonRpc::updateBridgePorts(const string& brName, const string& portUuid, bool addToList) {
    JsonRpcTransactMessage msg1(OvsdbOperation::MUTATE, OvsdbTable::BRIDGE);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, brName);
    msg1.conditions = condSet;

    vector<TupleData> tuples;
    tuples.emplace_back("uuid", portUuid);
    TupleDataSet tdSet = TupleDataSet(tuples);
    msg1.mutateRowData.emplace("ports", std::make_pair(addToList ? OvsdbOperation::INSERT : OvsdbOperation::DELETE, tdSet));

    const list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::getOvsdbMirrorConfig(const string& sessionName, mirror& mir) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::MIRROR);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, sessionName);
    msg1.conditions = condSet;
    const list<JsonRpcTransactMessage> requests1 = {msg1};
    if (!sendRequestAndAwaitResponse(requests1)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    if (!handleMirrorConfig(pResp->payload, mir)) {
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
    if (!getPortList(pResp->payload, portMap)) {
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
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, portName);
    msg1.conditions = condSet;
    msg1.columns.emplace("options");

    list<JsonRpcTransactMessage> requests = {msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    if (!getErspanOptions(pResp->payload, params)) {
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

bool JsonRpc::handleMirrorConfig(const Document& payload, mirror& mir) {
    getUuidByNameFromResp(payload, "_uuid", mir.uuid);
    if (mir.uuid.empty()) {
        LOG(DEBUG) << "no mirror found";
        return false;
    }
    getUuidsByNameFromResp(payload, "select_src_port", mir.src_ports);
    getUuidsByNameFromResp(payload, "select_dst_port", mir.dst_ports);
    getUuidByNameFromResp(payload, "output_port", mir.out_port);
    return true;
}

bool JsonRpc::getPortList(const Document& payload, unordered_map<string, string>& portMap) {
    LOG(WARNING) << "Gettting port list";
    if (payload.IsArray() && payload.Size() > 0 && payload[0].IsObject() && payload[0].HasMember("rows")) {
        const Value& rowVal = payload[0]["rows"];
        if (rowVal.IsArray()) {
            for (Value::ConstValueIterator itr = rowVal.Begin(); itr != rowVal.End(); itr++) {
                if (itr->HasMember("name") && itr->HasMember("_uuid")) {
                    LOG(DEBUG) << (*itr)["name"].GetString() << ":"
                               << (*itr)["_uuid"][1].GetString();
                    portMap.emplace((*itr)["_uuid"][1].GetString(),
                                    (*itr)["name"].GetString());
                } else {
                    LOG(WARNING) << "Result missing name/uuid";
                    return false;
                }
            }
        }
    } else {
        LOG(DEBUG) << "payload is not an array";
        return false;
    }
    return true;
}

bool JsonRpc::getErspanOptions(const Document& payload, ErspanParams& params) {
    if (!payload.IsArray()) {
        LOG(DEBUG) << "payload is not an array";
        return false;
    }
    if (payload.Size() > 0 && payload[0].IsObject() && payload[0].HasMember("rows")) {
        const Value& rowVal = payload[0]["rows"];
        if (rowVal.IsArray() && rowVal.Size() > 0 && rowVal[0].IsObject()) {
            if (rowVal[0].HasMember("options")) {
                const Value& optionsVal = rowVal[0]["options"];
                if (optionsVal.Size() > 1 && optionsVal[0].IsString() && optionsVal[1].IsArray()) {
                    string entry = optionsVal[0].GetString();
                    if (entry == "map") {
                        // then array of arrays
                        for (Value::ConstValueIterator itr = optionsVal[1].Begin(); itr != optionsVal[1].End(); itr++) {
                            if (itr->IsArray()) {
                                for (Value::ConstValueIterator innerItr = itr->Begin(); innerItr != itr->End(); innerItr++) {
                                    string key(innerItr[0].GetString());
                                    if (key == "erspan_ver") {
                                        params.setVersion(stoi(innerItr[1].GetString()));
                                    } else if (key == "remote_ip") {
                                        params.setRemoteIp(innerItr[1].GetString());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return (params.getVersion() != 0);
}

void JsonRpc::getPortUuid(const string& name, string& uuid) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::PORT);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, name);
    msg1.conditions = condSet;
    msg1.columns.emplace("_uuid");

    const list<JsonRpcTransactMessage> requests{msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(WARNING) << "Error sending message";
        return;
    }

    getUuidByNameFromResp(pResp->payload, "_uuid", uuid);
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

void JsonRpc::getMirrorUuid(const string& name, string& uuid) {
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::MIRROR);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, name);
    msg1.conditions = condSet;
    msg1.columns.emplace("_uuid");

    const list<JsonRpcTransactMessage> requests{msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
    }
    getUuidByNameFromResp(pResp->payload, "_uuid", uuid);
}

void JsonRpc::getBridgeUuid(const string& name, string& uuid) {
    static const string uuidColumn("_uuid");
    JsonRpcTransactMessage msg1(OvsdbOperation::SELECT, OvsdbTable::BRIDGE);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, name);
    msg1.conditions = condSet;
    msg1.columns.emplace(uuidColumn);

    const list<JsonRpcTransactMessage> requests{msg1};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
    }
    getUuidByNameFromResp(pResp->payload, uuidColumn, uuid);
}

void JsonRpc::getUuidByNameFromResp(const Document& payload, const string& uuidName, string& uuid) {
    if (payload.IsArray() && payload.Size() > 0 && payload[0].IsObject() && payload[0].HasMember("rows")) {
        const Value& rowVal = payload[0]["rows"];
        if (rowVal.IsArray() && rowVal.Size() > 0 && rowVal[0].IsObject()) {
            if (rowVal[0].HasMember(uuidName.c_str())) {
                const Value& uuidVal = rowVal[0][uuidName.c_str()];
                if (uuidVal.Size() > 1) {
                    uuid = uuidVal[1].GetString();
                }
            }
        }
    }
}

void JsonRpc::getUuidsByNameFromResp(const Document& payload, const string& uuidsName, set<string>& uuids) {
    if (payload.IsArray() && payload.Size() > 0 && payload[0].IsObject() && payload[0].HasMember("rows")) {
        const Value& rowVal = payload[0]["rows"];
        if (rowVal.IsArray() && rowVal.Size() > 0 && rowVal[0].IsObject()) {
            if (rowVal[0].HasMember(uuidsName.c_str())) {
                LOG(DEBUG) << "Adding uuids for " << uuidsName;
                const Value& uuidVal = rowVal[0][uuidsName.c_str()];
                if (uuidVal.Size() > 1 && uuidVal[0].IsString() && uuidVal[1].IsArray()) {
                    string entry = uuidVal[0].GetString();
                    if (entry == "set") {
                        // then array of arrays
                        for (Value::ConstValueIterator itr = uuidVal[1].Begin(); itr != uuidVal[1].End(); itr++) {
                            if (itr->IsArray()) {
                                for (Value::ConstValueIterator uuidItr = itr->Begin(); uuidItr != itr->End(); uuidItr++) {
                                    if (uuidItr->IsString()) {
                                        string val = uuidItr->GetString();
                                        if (val != "uuid") {
                                            LOG(DEBUG) << "Adding uuid " << uuidItr->GetString();
                                            uuids.emplace(uuidItr->GetString());
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else if (uuidVal.Size() > 1 && uuidVal[0].IsString() && uuidVal[1].IsString()) {
                    LOG(WARNING) << "uuid " << uuidVal[1].GetString();
                    uuids.emplace(uuidVal[1].GetString());
                }
            }
        }
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
    JsonRpcTransactMessage msg2(OvsdbOperation::MUTATE, OvsdbTable::BRIDGE);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("_uuid", OvsdbFunction::EQ, brUuid);
    msg2.conditions = condSet;
    tuples.emplace_back("named-uuid", uuid_name);
    tdSet = TupleDataSet(tuples);
    msg2.mutateRowData.emplace("mirrors", std::make_pair(OvsdbOperation::INSERT, tdSet));

    const list<JsonRpcTransactMessage> requests = {msg1, msg2};
    if (!sendRequestAndAwaitResponse(requests)) {
        LOG(DEBUG) << "Error sending message";
        return false;
    }
    return true;
}

bool JsonRpc::addErspanPort(const string& bridgeName, ErspanParams& params) {
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

    tuples.clear();
    tuples.emplace_back("erspan_ver", std::to_string(params.getVersion()));
    tuples.emplace_back("remote_ip", params.getRemoteIp());
    tdSet = TupleDataSet(tuples, "map");
    msg2.rowData.emplace("options", tdSet);

    JsonRpcTransactMessage msg3(OvsdbOperation::MUTATE, OvsdbTable::BRIDGE);
    tuples.clear();
    tuples.emplace_back("named-uuid", uuid_name);
    tdSet = TupleDataSet(tuples);
    msg3.mutateRowData.emplace("ports", std::make_pair(OvsdbOperation::INSERT, tdSet));
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, bridgeName);
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

bool JsonRpc::deleteMirror(const string& brName, const string& sessionName) {
    string sessionUuid;
    getMirrorUuid(sessionName, sessionUuid);
    if (sessionUuid.empty()) {
        LOG(DEBUG) << "Unable to find session " << sessionName;
        return false;
    }

    JsonRpcTransactMessage msg(OvsdbOperation::MUTATE, OvsdbTable::BRIDGE);
    set<tuple<string, OvsdbFunction, string>> condSet;
    condSet.emplace("name", OvsdbFunction::EQ, brName);
    msg.conditions = condSet;

    vector<TupleData> tuples;
    tuples.emplace_back("uuid", sessionUuid);
    TupleDataSet tdSet = TupleDataSet(tuples);
    msg.mutateRowData.emplace("mirrors", std::make_pair(OvsdbOperation::DELETE, tdSet));

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

} // namespace opflexagemt
