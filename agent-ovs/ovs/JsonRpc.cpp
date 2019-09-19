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

using namespace boost;
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
        pConn->ready.notify_all();
        responseReceived = true;
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
                LOG(INFO) << obj3.GetString();
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

    void JsonRpc::printMirMap(map<string, mirror> mirMap) {
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
            ss << "   output ports" << endl;
            for (string uuid : elem.second.out_ports) {
                ss << "      " << uuid << endl;
            }
            LOG(INFO) << ss.rdbuf();
        }
    }

    void JsonRpc::printMap(map<string, string> m) {
        stringstream ss;
        ss << endl;
        for (auto elem : m) {
            ss << elem.first << ":" << elem.second << endl;
        }
        LOG(INFO) << ss.rdbuf();
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
        std::tuple<string, string, string> cond1("_uuid", "==", brPortUuid);
        set<std::tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        td.conditions = condSet;
        set<tuple<string, string>> rdata;
        for (auto elem : brPorts) {
            rdata.emplace("uuid", elem);
        }
        std::shared_ptr<TupleData> mPtr = make_shared<TupleData>(rdata, "set");
        uint64_t reqId = getNextId();
        td.rows.emplace("ports", mPtr);
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
            std::shared_ptr<BrPortResult> brPtr) {
        std::tuple<string, set<string>> brPorts;
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

    bool JsonRpc::getBridgePortList(string bridge,
            BrPortResult& res) {
        std::tuple<string, string, string> cond1("name", "==", bridge);
        set<std::tuple<string, string, string>> condSet;
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

        std::shared_ptr<BrPortResult> brPtr =
                make_shared<BrPortResult>();
        if (handleGetBridgePortList(pResp->reqId, pResp->payload, brPtr)) {
            res = *brPtr;
            return true;
        }
        return false;
    }
/*
    string JsonRpc::getBridgeUuidFromMap(string name) {
        map<string, mirror>::iterator itMap = mirMap.find(name);
        if( itMap == mirMap.end()) {
            LOG(WARNING) << "mirror not found";
            return "";
        } else {
            return itMap->second.brUuid;
        }
    }
*/
    string JsonRpc::getPortUuid(string name) {
        transData td;
        std::tuple<string, string, string> cond1("name", "==", name);
        set<std::tuple<string, string, string>> condSet;
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
        std::tuple<string, string, string> cond1("name", "==", name);
        set<std::tuple<string, string, string>> condSet;
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
        ports.insert(mir.out_ports.begin(), mir.out_ports.end());
        for (set<string>::iterator it = ports.begin();
                it != ports.end(); ++it) {
            portUuidMap.emplace(*it, "");
        }
        getPortUuids(portUuidMap);

        printMap(portUuidMap);

        transData td1;

        td1.operation = "insert";
        td1.table = "Mirror";
        // src ports
        set<tuple<string,string>> rdata;
        populatePortUuids(mir.src_ports, portUuidMap, rdata);
        std::shared_ptr<TupleData> tPtr = make_shared<TupleData>(rdata, "set");

        td1.rows.emplace("select_src_port", tPtr);

        // dst ports
        rdata.clear();
        populatePortUuids(mir.dst_ports, portUuidMap, rdata);
        tPtr.reset(new TupleData(rdata, "set"));
        td1.rows.emplace("select_dst_port", tPtr);

        // output ports
        rdata.clear();
        populatePortUuids(mir.out_ports, portUuidMap, rdata);
        tPtr.reset( new TupleData(rdata, "set"));
        td1.rows.emplace("output_port", tPtr);

        // name
        std::shared_ptr<StringData> sPtr = make_shared<StringData>(name);
        td1.rows.emplace("name", sPtr);

        string uuid_name = generateTempUuid();
        td1.kvPairs.emplace("uuid-name", uuid_name);
        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td1);

        // msg2
        std::tuple<string, string, string> cond1("_uuid", "==", brUuid);
        set<std::tuple<string, string, string>> condSet;
        condSet.emplace(cond1);

        transData td2;
        td2.conditions = condSet;
        td2.operation = "update";
        td2.table = "Bridge";
        rdata.clear();
        rdata.emplace("named-uuid", uuid_name);
        tPtr.reset(new TupleData(rdata));
        td2.rows.emplace("mirrors", tPtr);
        //req->addTransaction(msg2);
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
        std::random_device rng;
        std::mt19937 urng(rng());
        string uuid_name = to_string(basic_random_generator<std::mt19937>(urng)());
        uuid_name.insert(0,"row");
        std::regex hyph ("-");
        string underscore("_");
        return std::regex_replace(uuid_name, hyph, underscore);
    }

    bool JsonRpc::addErspanPort(const string bridge, const erspan_port port) {

        transData td;
        td.operation = "insert";
        td.table = "Port";
        std::shared_ptr<StringData> sPtr = make_shared<StringData>(port.name);

        td.rows.emplace("name", sPtr);

        // uuid-name
        string uuid_name = generateTempUuid();
        td.kvPairs.emplace("uuid-name", uuid_name);

        // interfaces
        string named_uuid = generateTempUuid();
        set<tuple<string, string>> rdata;
        rdata.emplace("named-uuid", named_uuid);
        std::shared_ptr<TupleData> mPtr = make_shared<TupleData>(rdata);
        td.rows.emplace("interfaces", mPtr);

        list<transData> tl;
        tl.push_back(td);

        // uuid-name
        transData td1;
        td1.operation = "insert";
        td1.table = "Interface";
        td1.kvPairs.emplace("uuid-name", named_uuid);

        // row entries
        // name
        td1.rows.emplace("name", sPtr);
        // options
        rdata.clear();
        rdata.emplace("erspan_idx", "1");
        rdata.emplace("erspan_ver", "1");
        rdata.emplace("key", "1");
        rdata.emplace("remote_ip", port.ip_address);
        mPtr.reset(new TupleData(rdata, "map"));
        td1.rows.emplace("options", mPtr);
        // type
        sPtr.reset();
        sPtr = make_shared<StringData>("erspan");
        td1.rows.emplace("type", sPtr);
        tl.push_back(td1);
        //req->addTransaction(msg2);

        // get bridge port list and add erspan port to it.
        BrPortResult res;
        if (!getBridgePortList(bridge, res)) {
            return false;
        }
        std::tuple<string, string, string> cond1("_uuid", "==", res.brUuid);
        set<std::tuple<string, string, string>> condSet;
        condSet.emplace(cond1);

        transData td2;
        td2.operation = "update";
        td2.table = "Bridge";
        rdata.clear();
        for (auto elem : res.portUuids) {
            rdata.emplace("uuid", elem);
        }
        rdata.emplace("named-uuid", uuid_name);
        mPtr.reset(new TupleData(rdata, "set"));
        td2.rows.emplace("ports", mPtr);

        uint64_t reqId = getNextId();

        tl.push_back(td2);
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
            std::tuple<string, string, string> cond1("name", "==", brName);
            set<std::tuple<string, string, string>> condSet;
            condSet.emplace(cond1);

            transData td;
            td.conditions = condSet;
            td.operation = "update";
            td.table = "Bridge";

            set<tuple<string, string>> rdata;
            std::shared_ptr<TupleData> mPtr = make_shared<TupleData>(rdata, "set");
            td.rows.emplace("mirrors", mPtr);
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

} // namespace opflex
