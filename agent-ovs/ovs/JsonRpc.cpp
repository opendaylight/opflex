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
#include <memory>
#include <regex>
#include <string>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <uv.h>

#include "JsonRpc.h"


#include <opflexagent/logging.h>
#include "opflex/ofcore/OFConstants.h"
#include "rpc/JsonRpc.h"

#include <random>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional/optional.hpp>


using namespace opflex::ofcore;
using namespace std::chrono;

namespace opflexagent {

using rapidjson::Writer;

using namespace rapidjson;
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;

    void JsonRpc::handleTransaction(uint64_t reqId,
            const rapidjson::Value& payload) {
        pResp.reset(new response(reqId, payload));
        responseReceived = true;
        pConn->ready.notify_all();
    }

     bool JsonRpc::createNetFlow(const string& brUuid, const string& target, const int& timeout, bool addidtointerface ) {
        transData td1;
        td1.operation = "insert";
        td1.table = "NetFlow";

        shared_ptr<TupleData<string>> tPtr =
            make_shared<TupleData<string>>("", target);
        set<shared_ptr<BaseData>> pSet;
        pSet.emplace(tPtr);
        shared_ptr<TupleDataSet> pTdSet = make_shared<TupleDataSet>(TupleDataSet(pSet));
        td1.rows.emplace("targets", pTdSet);

        shared_ptr<TupleData<int>> iTimeout = make_shared<TupleData<int>>("", timeout);
        pSet.clear();
        pSet.emplace(iTimeout);
        pTdSet.reset(new TupleDataSet(pSet));
        td1.rows.emplace("active_timeout", pTdSet);

        shared_ptr<TupleData<bool>> bAddIdToInterface = make_shared<TupleData<bool>>("", addidtointerface);
        pSet.clear();
        pSet.emplace(bAddIdToInterface);
        pTdSet.reset(new TupleDataSet(pSet));
        td1.rows.emplace("add_id_to_interface", pTdSet);

        string uuid_name = generateTempUuid();
        td1.kvPairs.emplace(make_shared<TupleData<string>>("uuid-name", uuid_name));
        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td1);

        tuple<string, string, string> cond1("_uuid", "==", brUuid);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);

        transData td2;
        pSet.clear();
        td2.conditions = condSet;
        td2.operation = "update";
        td2.table = "Bridge";

        tPtr.reset(new TupleData<string>("named-uuid", uuid_name));
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(pSet));
        td2.rows.emplace("netflow", pTdSet);

        tl.push_back(td2);

        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return false;
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return false;
        }
        string uuid;
        if (handleCreateNetFlowResp(pResp->reqId, pResp->payload, uuid)) {
            return true;
        } else {
            return false;
        }
    }
  bool JsonRpc::handleCreateNetFlowResp(uint64_t reqId,
            const rapidjson::Value& payload, string& uuid) {
        list<string> ids = {"0","uuid","1"};
        Value val = opflex::engine::internal::getValue(payload, ids);
        if (!val.IsNull() && val.IsString()) {
            LOG(DEBUG) << "netflow uuid " << val.GetString();
            uuid = val.GetString();
        } else {
            return false;
        }
        return true;
    }

    bool JsonRpc::deleteNetFlow(const string& brName) {

        tuple<string, string, string> cond1("name", "==", brName);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);

        transData td;
        td.conditions = condSet;
        td.operation = "update";
        td.table = "Bridge";

        set<shared_ptr<BaseData>> pSet;
        shared_ptr<TupleDataSet> pTdSet = make_shared<TupleDataSet>(TupleDataSet(pSet));
        pTdSet->label = "set";
        td.rows.emplace("netflow", pTdSet);

        uint64_t reqId = getNextId();

        list<transData> tl = {td};

        if (!sendRequest(tl, reqId))
        {
            LOG(DEBUG) << "Error sending message";
            return false;
            }
            if (!checkForResponse()) {
                LOG(DEBUG) << "Error getting response";
                return false;
            }
            return true;
    }
    bool JsonRpc::handleGetPortUuidResp(uint64_t reqId,
            const rapidjson::Value& payload, string& uuid) {
        if (payload.IsArray()) {
            try {
                list<string> ids = { "0","rows","0","_uuid","1" };
                 const Value& obj3 =
                         opflex::engine::internal::getValue(payload, ids);
                 if (obj3.IsNull()) {
                     LOG(DEBUG) << "got null";
                     return false;
                 }
                uuid = obj3.GetString();
                return true;
            } catch(const std::exception &e) {
                LOG(DEBUG) << "caught exception " << e.what();
                return false;
            }
        } else {
            LOG(DEBUG) << "payload is not an array";
            return false;
        }
    }

    bool JsonRpc::handleGetPortParam(uint64_t reqId,
            const rapidjson::Value& payload, string& col, string& param) {
        if (payload.IsArray()) {
            try {
                list<string> ids;
                if (col.compare("_uuid")) {
                    ids = { "0","rows","0","_uuid","1" };
                } else if (col.compare("name")) {
                    ids = { "0","rows","0","name"};
                } else {
                    return false;
                }
                 const Value& obj3 =
                         opflex::engine::internal::getValue(payload, ids);
                 if (obj3.IsNull()) {
                     LOG(DEBUG) << "got null";
                     return false;
                 }
                param = obj3.GetString();
                return true;
            } catch(const std::exception &e) {
                LOG(DEBUG) << "caught exception " << e.what();
                return false;
            }
        } else {
            LOG(DEBUG) << "payload is not an array";
            return false;
        }
    }

    void JsonRpc::printMirMap(const map<string, mirror>& mirMap) {
        stringstream ss;
        ss << endl;
        for (pair<string, mirror> elem : mirMap) {
            ss << "mirror name: " << elem.first << endl;
            ss << "   uuid: " << elem.second.uuid << endl;
            ss << "   bridge uuid: " << elem.second.brUuid << endl;
            ss << "   src ports" << endl;
            for (string uuid : elem.second.src_ports) {
                ss << "      " << uuid << endl;
            }
            ss << "...dst ports" << endl;
            for (string uuid : elem.second.dst_ports) {
                ss << "      " << uuid << endl;
            }
            ss << "   output port" << endl;
            for (string uuid : elem.second.dst_ports) {
                ss << "      " << uuid << endl;
            }

            LOG(DEBUG) << ss.rdbuf();
        }
    }

    void JsonRpc::printMap(const map<string, string>& m) {
        stringstream ss;
        ss << endl;
        for (auto elem : m) {
            ss << elem.first << ":" << elem.second << endl;
        }
        LOG(DEBUG) << ss.rdbuf();
    }

    void JsonRpc::printSet(const set<string>& s) {
        stringstream ss;
        ss << endl;
        for (auto elem : s) {
            ss << elem << endl;
        }
        LOG(DEBUG) << ss.rdbuf();
    }

    void JsonRpc::handleGetBridgeMirrorUuidResp(uint64_t reqId,
            const rapidjson::Value& payload) {
        list<string> ids = {"0","rows","0","_uuid","1"};
        string brUuid;
        Value val = opflex::engine::internal::getValue(payload, ids);
        if (!val.IsNull() && val.IsString()) {
            LOG(DEBUG) << "bridge uuid " << val.GetString();
            brUuid = val.GetString();
        } else {
            responseReceived = true;
            return;
        }
        // OVS supports only one mirror, expect only one.
        ids = {"0","rows","0","mirrors","1"};
        val = opflex::engine::internal::getValue(payload, ids);
        string mirUuid;
        if (!val.IsNull() && val.IsString()) {
            LOG(DEBUG) << "mirror uuid " << val.GetString();
            mirUuid = val.GetString();
        } else {
            LOG(WARNING) << "did not find mirror ID";
            responseReceived = true;
            return;
        }
        for (auto& elem : mirMap) {
            if ((elem.second).uuid == mirUuid) {
                LOG(DEBUG) << "found mirror, adding bridge uuid";
                (elem.second).brUuid = brUuid;
                break;
            }
        }
        printMirMap(mirMap);
        pConn->ready.notify_all();
        responseReceived = true;
    }

    bool JsonRpc::updateBridgePorts(tuple<string,set<string>> ports,
            string port, bool action) {
        string brPortUuid = get<0>(ports);
        set<string> brPorts = get<1>(ports);
        for (set<string>::iterator itr = brPorts.begin();
                itr != brPorts.end(); ++itr) {
        }
        if (action) {
            // add port to list
            brPorts.emplace(port);
        } else {
            // remove port from list
            brPorts.erase(port);
        }
        transData td;
        td.operation = "update";
        td.table = "Bridge";
        tuple<string, string, string> cond1("_uuid", "==", brPortUuid);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        td.conditions = condSet;
        set<shared_ptr<BaseData>> pSet;
        for (auto elem : brPorts) {
            shared_ptr<TupleData<string>> tPtr =
                    make_shared<TupleData<string>>("uuid", elem);
            pSet.emplace(tPtr);
        }
        shared_ptr<TupleDataSet> pTdSet = make_shared<TupleDataSet>(TupleDataSet(pSet));
        pTdSet->label = "set";
        td.rows.emplace("ports", pTdSet);

        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td);
        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return false;
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return false;
        }
        return true;
    }

    bool JsonRpc::handleGetBridgePortList(uint64_t reqId,
            const rapidjson::Value& payload,
            shared_ptr<BrPortResult> brPtr) {
        tuple<string, set<string>> brPorts;
        string brPortUuid;
        set<string> brPortSet;
        list<string> ids = {"0","rows","0","ports","0"};
        Value val =
                opflex::engine::internal::getValue(payload, ids);
        if (!val.IsNull() && val.IsString()) {
            string valStr(val.GetString());
            ids = {"0", "rows", "0", "ports", "1"};
            val = opflex::engine::internal::getValue(payload, ids);
            if (valStr.compare("uuid") == 0) {
                brPortSet.emplace(val.GetString());
            }
        } else {
            error = "Error getting port uuid";
            LOG(DEBUG) << error;
            return false;
        }

        if (val.IsArray()) {
            for (Value::ConstValueIterator itr1 = val.Begin();
                    itr1 != val.End(); itr1++) {
                brPortSet.emplace((*itr1)[1].GetString());
            }
        }
        ids = {"0", "rows", "0", "_uuid", "1"};
        val = opflex::engine::internal::getValue(payload, ids);
        brPortUuid = val.GetString();
        brPorts = make_tuple(brPortUuid, brPortSet);

        brPtr->brUuid = brPortUuid;
        brPtr->portUuids.insert(brPortSet.begin(), brPortSet.end());
        return true;
    }

    void JsonRpc::getUuidsFromVal(set<string>& uuidSet, const Value& payload,
            const string& index) {
        list<string> ids = {"0","rows","0",index,"0"};
        Value val =
                opflex::engine::internal::getValue(payload, ids);
        if (!val.IsNull() && val.IsString()) {
            string valStr(val.GetString());
            ids = {"0", "rows", "0", index, "1"};
            val = opflex::engine::internal::getValue(payload, ids);
            if (valStr.compare("uuid") == 0) {
                uuidSet.emplace(val.GetString());
            }
        } else {
            error = "Error getting port uuid";
            LOG(DEBUG) << error;
            return;
        }

        if (val.IsArray()) {
            for (Value::ConstValueIterator itr1 = val.Begin();
                    itr1 != val.End(); itr1++) {
                uuidSet.emplace((*itr1)[1].GetString());
            }
        }
    }

    bool JsonRpc::getBridgePortList(string bridge,
            BrPortResult& res) {
        tuple<string, string, string> cond1("name", "==", bridge);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        transData td;
        td.conditions = condSet;
        td.operation = "select";
        td.table = "Bridge";
        td.columns.emplace("ports");
        td.columns.emplace("_uuid");
        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td);

        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return false;
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return false;
        }

        shared_ptr<BrPortResult> brPtr =
                make_shared<BrPortResult>();
        if (handleGetBridgePortList(pResp->reqId, pResp->payload, brPtr)) {
            res = *brPtr;
            return true;
        }
        return false;
    }

    bool JsonRpc::getOvsdbMirrorConfig(mirror& mir) {
        transData td;
        td.operation = "select";
        td.table = "Mirror";
        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td);

        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return false;
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return false;
        }
        handleMirrorConfig(pResp->reqId, pResp->payload, mir);
        // collect all port UUIDs in a set and query
        // OVSDB for names.
        set<string> uuids;
        uuids.insert(mir.src_ports.begin(), mir.src_ports.end());
        uuids.insert(mir.dst_ports.begin(), mir.dst_ports.end());
        uuids.insert(mir.out_ports.begin(), mir.out_ports.end());

        td.operation = "select";
        td.table = "Port";
        td.columns.emplace("name");
        td.columns.emplace("_uuid");
        reqId = getNextId();
        tl.clear();
        tl.push_back(td);

        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return false;
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
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
        substituteSet(mir.out_ports, portMap);
        return true;
    }

        bool JsonRpc::getErspanIfcParams(erspan_ifc& ifc) {
            // for ERSPAN port get IP address
            transData td;
            td.operation = "select";
            td.table = "Interface";
            tuple<string, string, string> cond1("name", "==", ERSPAN_PORT_NAME);
            set<tuple<string, string, string>> condSet;
            condSet.emplace(cond1);
            td.conditions = condSet;
            td.columns.emplace("options");
            uint64_t reqId = getNextId();
            list<transData> tl;
            tl.push_back(td);

            if (!sendRequest(tl, reqId)) {
                LOG(DEBUG) << "Error sending message";
                return false;
            }
            if (!checkForResponse()) {
                LOG(DEBUG) << "Error getting response";
                return false;
            }

            unordered_map<string, string> optMap;
            if (!getErspanOptions(pResp->reqId, pResp->payload, ifc)) {
                LOG(DEBUG) << "failed to get ERSPAN options";
                return false;
            }

            return true;
    }

    void JsonRpc::substituteSet(set<string>& s, const unordered_map<string, string>& portMap) {
        set<string> names;
        for (auto elem = s.begin(); elem != s.end(); elem++) {
            LOG(DEBUG) << "uuid " << *elem;
            auto itr  = portMap.find(*elem);
            if (itr != portMap.end()) {
                LOG(DEBUG) << "name " << itr->second;
                names.insert(itr->second);
            }
        }
        s.clear();
        s.insert(names.begin(), names.end());
    }

    bool JsonRpc::handleMirrorConfig(uint64_t reqId,
            const rapidjson::Value& payload, mirror& mir) {
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

        mir.out_ports.insert(uuids.begin(), uuids.end());
        return true;
    }

    bool JsonRpc::getPortList(const uint64_t reqId, const Value& payload,
            unordered_map<string, string>& portMap) {
        if (payload.IsArray()) {
            try {
                list<string> ids = { "0","rows"};
                 const Value& arr =
                         opflex::engine::internal::getValue(payload, ids);
                 if (arr.IsNull() || !arr.IsArray()) {
                     LOG(DEBUG) << "expected array";
                     return false;
                 }
                 for (Value::ConstValueIterator itr1 = arr.Begin();
                         itr1 != arr.End(); itr1++) {
                     LOG(DEBUG) << (*itr1)["name"].GetString() << ":"
                     << (*itr1)["_uuid"][1].GetString();
                     portMap.emplace((*itr1)["_uuid"][1].GetString(),
                             (*itr1)["name"].GetString());
                 }
            } catch(const std::exception &e) {
                LOG(DEBUG) << "caught exception " << e.what();
                return false;
            }
        } else {
            LOG(DEBUG) << "payload is not an array";
            return false;
        }
        return true;
    }

    bool JsonRpc::getErspanOptions(const uint64_t reqId, const Value& payload,
            erspan_ifc& ifc) {
        if (!payload.IsArray()) {
            LOG(DEBUG) << "payload is not an array";
            return false;
        }
        try {
            list<string> ids = {"0","rows","0","options","1"};
            const Value& arr =
                    opflex::engine::internal::getValue(payload, ids);
            if (arr.IsNull() || !arr.IsArray()) {
                LOG(DEBUG) << "expected array";
                return false;
            }
            for (Value::ConstValueIterator itr1 = arr.Begin();
                    itr1 != arr.End(); itr1++) {
                LOG(DEBUG) << (*itr1)[0].GetString() << ":"
                << (*itr1)[1].GetString();
                string val((*itr1)[1].GetString());
                string index((*itr1)[0].GetString());
                if (index.compare("erspan_idx") == 0 ||
                    index.compare("erspan_ver") == 0 ||
                    index.compare("key")) {
                        ifc.erspan_ver = stoi(val);
                } else if (index.compare("remote_ip") == 0) {
                    ifc.remote_ip = val;
                }
            }
       } catch(const std::exception &e) {
           LOG(DEBUG) << "caught exception " << e.what();
           return false;
       }
       return true;
    }

    string JsonRpc::getPortUuid(const string& name) {
        transData td;
        tuple<string, string, string> cond1("name", "==", name);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        td.conditions = condSet;

        td.operation = "select";
        td.table = "Port";

        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td);
        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return "";
        }

        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return "";
        }

        string uuid;
        if (handleGetPortUuidResp(pResp->reqId, pResp->payload,
                uuid)) {
            return uuid;
        } else {
            return "";
        }
    }

    string JsonRpc::getPortParam(const string& col, const string& match) {
        transData td;
        tuple<string, string, string> cond1(col, "==", match);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        td.conditions = condSet;

        td.operation = "select";
        td.table = "Port";

        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td);
        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return "";
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return "";
        }
        string uuid;
        if (handleGetPortUuidResp(pResp->reqId, pResp->payload,
                uuid)) {
            return uuid;
        } else {
            return "";
        }
    }

    void JsonRpc::getPortUuids(map<string, string>& ports) {
        for (map<string, string>::iterator it = ports.begin();
                it != ports.end(); ++it) {
            string uuid = getPortUuid(it->first);
            if (!uuid.empty()) {
               it->second = uuid;
            }
        }
    }

    string JsonRpc::getBridgeUuid(string name) {
        tuple<string, string, string> cond1("name", "==", name);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        transData td;
        td.conditions = condSet;

        td.operation = "select";
        td.table = "Bridge";
        td.columns.emplace("_uuid");
        uint64_t reqId = getNextId();

        list<transData> tl;
        tl.push_back(td);
        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return "";
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return "";
        }
        string uuid;
        if (handleGetBridgeUuidResp(pResp->reqId, pResp->payload, uuid)) {
            return uuid;
        } else {
            return "";
        }
    }

    bool JsonRpc::handleGetBridgeUuidResp(uint64_t reqId,
                    const rapidjson::Value& payload, string& uuid) {
        list<string> ids = {"0","rows","0","_uuid","1"};
        Value val = opflex::engine::internal::getValue(payload, ids);
        if (!val.IsNull() && val.IsString()) {
            uuid = val.GetString();
        } else {
            return false;
        }
        return true;
    }

    bool JsonRpc::createMirror(string brUuid, string name) {
        map<string, string> portUuidMap;

        auto itr = mirMap.find(name);
        if (!(itr != mirMap.end())) {
            LOG(DEBUG) << "mirror " << name << " not found" <<
                    ", unable to create mirror";
            return false;
        }
        mirror& mir = itr->second;

        set<string> ports;
        ports.insert(mir.src_ports.begin(), mir.src_ports.end());
        ports.insert(mir.dst_ports.begin(), mir.dst_ports.end());
        ports.insert(ERSPAN_PORT_NAME);
        for (set<string>::iterator it = ports.begin();
                it != ports.end(); ++it) {
            portUuidMap.emplace(*it, "");
        }
        getPortUuids(portUuidMap);

        transData td1;

        td1.operation = "insert";
        td1.table = "Mirror";

        set<shared_ptr<BaseData>> pSet;
        // src ports
        set<tuple<string,string>> rdata;
        populatePortUuids(mir.src_ports, portUuidMap, rdata);
        for (auto pair : rdata) {
            shared_ptr<TupleData<string>> tPtr =
                    make_shared<TupleData<string>>(get<0>(pair).c_str(), get<1>(pair).c_str());
            pSet.emplace(tPtr);
        }
        shared_ptr<TupleDataSet> pTdSet = make_shared<TupleDataSet>(TupleDataSet(pSet));
        pTdSet->label = "set";
        td1.rows.emplace("select_src_port", pTdSet);

        // dst ports
        rdata.clear();
        pSet.clear();
        populatePortUuids(mir.dst_ports, portUuidMap, rdata);
        for (auto pair : rdata) {
            shared_ptr<TupleData<string>> tPtr =
                    make_shared<TupleData<string>>(get<0>(pair).c_str(), get<1>(pair).c_str());
            pSet.emplace(tPtr);
        }
        pTdSet.reset(new TupleDataSet(pSet));
        pTdSet->label = "set";
        td1.rows.emplace("select_dst_port", pTdSet);

        // output ports
        pSet.clear();
        rdata.clear();
        ports.clear();
        ports.insert(ERSPAN_PORT_NAME);
        populatePortUuids(ports, portUuidMap, rdata);
        for (auto pair : rdata) {
            shared_ptr<TupleData<string>> tPtr =
                    make_shared<TupleData<string>>(get<0>(pair).c_str(), get<1>(pair).c_str());
            pSet.emplace(tPtr);
        }
        pTdSet.reset(new TupleDataSet(pSet));
        pTdSet->label = "set";
        td1.rows.emplace("output_port", pTdSet);

        // name
        pSet.clear();
        shared_ptr<TupleData<string>> tPtr =
                make_shared<TupleData<string>>("", name);
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(TupleDataSet(pSet)));
        td1.rows.emplace("name", pTdSet);

        string uuid_name = generateTempUuid();
        td1.kvPairs.emplace(make_shared<TupleData<string>>("uuid-name", uuid_name));
        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td1);

        // msg2
        tuple<string, string, string> cond1("_uuid", "==", brUuid);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);

        transData td2;
        pSet.clear();
        td2.conditions = condSet;
        td2.operation = "update";
        td2.table = "Bridge";
        tPtr.reset(new TupleData<string>("named-uuid", uuid_name));
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(pSet));
        td2.rows.emplace("mirrors", pTdSet);
        tl.push_back(td2);

        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return false;
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return false;
        }

        string uuid;
        if (handleCreateMirrorResp(pResp->reqId, pResp->payload, uuid)) {
            return true;
        } else {
            return false;
        }
    }

    inline string JsonRpc::generateTempUuid() {
        random_device rng;
        mt19937 urng(rng());
        string uuid_name = to_string(basic_random_generator<mt19937>(urng)());
        uuid_name.insert(0,"row");
        std::regex hyph ("-");
        string underscore("_");
        return std::regex_replace(uuid_name, hyph, underscore);
    }

    bool JsonRpc::addErspanPort(const string& bridge, const erspan_ifc& port) {

        transData td;
        td.operation = "insert";
        td.table = "Port";
        shared_ptr<TupleData<string>> tPtr =
                make_shared<TupleData<string>>("", port.name);
        set<shared_ptr<BaseData>> pSet;
        pSet.emplace(tPtr);
        shared_ptr<TupleDataSet> pTdSet = make_shared<TupleDataSet>(TupleDataSet(pSet));

        td.rows.emplace("name", pTdSet);

        // uuid-name
        string uuid_name = generateTempUuid();
        td.kvPairs.emplace(make_shared<TupleData<string>>("uuid-name", uuid_name));

        // interfaces
        string named_uuid = generateTempUuid();
        tPtr.reset(new TupleData<string>("named-uuid", named_uuid));
        pSet.clear();
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(TupleDataSet(pSet)));
        td.rows.emplace("interfaces", pTdSet);

        list<transData> tl;
        tl.push_back(td);

        // uuid-name
        transData td1;
        td1.operation = "insert";
        td1.table = "Interface";
        td1.kvPairs.emplace(make_shared<TupleData<string>>("uuid-name", named_uuid));

        // row entries
        // name
        tPtr.reset(new TupleData<string>("", port.name));
        pSet.clear();
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(pSet));
        td1.rows.emplace("name", pTdSet);

        // options
        pSet.clear();
        tPtr.reset(new TupleData<string>("erspan_idx", std::to_string(port.erspan_idx)));
        pSet.emplace(tPtr);
        tPtr.reset(new TupleData<string>("erspan_ver", std::to_string(port.erspan_ver)));
        pSet.emplace(tPtr);
        tPtr.reset(new TupleData<string>("key", std::to_string(port.key)));
        pSet.emplace(tPtr);
        tPtr.reset(new TupleData<string>("remote_ip", port.remote_ip));
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(pSet));
        pTdSet->label = "map";
        td1.rows.emplace("options", pTdSet);

        // type
        tPtr.reset(new TupleData<string>("", "erspan"));
        pSet.clear();
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(pSet));
        td1.rows.emplace("type", pTdSet);
        tl.push_back(td1);

        // get bridge port list and add erspan port to it.
        BrPortResult res;
        if (!getBridgePortList(bridge, res)) {
            return false;
        }
        tuple<string, string, string> cond1("_uuid", "==", res.brUuid);
        set<tuple<string, string, string>> condSet;
        condSet.emplace(cond1);

        transData td2;
        td2.operation = "update";
        td2.table = "Bridge";
        //rdata.clear();
        pSet.clear();
        for (auto elem : res.portUuids) {
            tPtr.reset(new TupleData<string>("uuid", elem));
            pSet.emplace(tPtr);
        }
        tPtr.reset(new TupleData<string>("named-uuid", uuid_name));
        pSet.emplace(tPtr);
        pTdSet.reset(new TupleDataSet(pSet, "set"));
        td2.rows.emplace("ports", pTdSet);
        tl.push_back(td2);

        uint64_t reqId = getNextId();

        if (!sendRequest(tl, reqId)) {
            LOG(DEBUG) << "Error sending message";
            return "";
        }
        if (!checkForResponse()) {
            LOG(DEBUG) << "Error getting response";
            return "";
        }

        return true;
    }

    inline void JsonRpc::populatePortUuids(set<string>& ports, map<string,
            string>& uuidMap, set<tuple<string,string>>& entries) {
        for (set<string>::iterator it = ports.begin();
                it != ports.end(); ++it) {
            map<string, string>::iterator itmap = uuidMap.find(*it);
            if (itmap != uuidMap.end()) {
                if (!itmap->second.empty())
                    entries.emplace("uuid",itmap->second);
            }
        }
    }

    bool JsonRpc::handleCreateMirrorResp(uint64_t reqId,
            const rapidjson::Value& payload, string& uuid) {
        list<string> ids = {"0","uuid","1"};
        Value val = opflex::engine::internal::getValue(payload, ids);
        if (!val.IsNull() && val.IsString()) {
            LOG(DEBUG) << "mirror uuid " << val.GetString();
            uuid = val.GetString();
        } else {
            return false;
        }
        return true;
    }

    void JsonRpc::handleAddMirrorToBridgeResp(uint64_t reqId,
            const rapidjson::Value& payload) {
    }

    void JsonRpc::handleAddErspanPortResp(uint64_t reqId,
            const rapidjson::Value& payload) {
        //ready.notify_all();
        responseReceived = true;
    }

    inline bool JsonRpc::checkForResponse() {
        unique_lock<mutex> lock(pConn->mtx);
        if (!pConn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                [=]{return responseReceived;})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        } else {
            return true;
        }
    }

    bool JsonRpc::deleteMirror(string brName) {
            list<string> mirList;
            tuple<string, string, string> cond1("name", "==", brName);
            set<tuple<string, string, string>> condSet;
            condSet.emplace(cond1);

            transData td;
            td.conditions = condSet;
            td.operation = "update";
            td.table = "Bridge";

            set<shared_ptr<BaseData>> pSet;
            shared_ptr<TupleDataSet> pTdSet = make_shared<TupleDataSet>(TupleDataSet(pSet));
            pTdSet->label = "set";
            td.rows.emplace("mirrors", pTdSet);

            uint64_t reqId = getNextId();

            list<transData> tl = {td};

            if (!sendRequest(tl, reqId)) {
                LOG(DEBUG) << "Error sending message";
                return false;
            }
            if (!checkForResponse()) {
                LOG(DEBUG) << "Error getting response";
                return false;
            }

            return true;
    }

    void JsonRpc::addMirrorData(string name, mirror mir) {
        mirMap.emplace(make_pair(name, mir));
    }

    void JsonRpc::start() {
        LOG(DEBUG) << "Starting .....";
        pConn = createConnection(*this);
        pConn->start();
    }

    void JsonRpc::stop() {
        pConn->stop();
        pConn.reset();
    }

    void JsonRpc::connect(string const& hostname, int port) {
        pConn->connect(hostname, port);
    }

    bool JsonRpc::isConnected() {
        unique_lock<mutex> lock(pConn->mtx);
        if (!pConn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                [=]{return pConn->isConnected();})) {
            LOG(DEBUG) << "lock timed out, no connection";
            return false;
        }
        return true;
    }

} // namespace opflex
