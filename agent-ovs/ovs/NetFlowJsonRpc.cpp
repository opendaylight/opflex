/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file NetFlowJsonRpc.cpp
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

#include "NetFlowJsonRpc.h"

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

    void NetFlowJsonRpc::handleTransaction(uint64_t reqId,
            const rapidjson::Value& payload) {
        pResp.reset(new response(reqId, payload));
        responseReceived = true;
        pConn->ready.notify_all();
    }

    bool NetFlowJsonRpc::createNetFlow(const string& brUuid, const string& target, const int& timeout, bool addidtointerface ) {
        transData td1;
        td1.operation = "insert";
        td1.table = "NetFlow";
        // active timeout
        std::shared_ptr<IntData> sTimeout = make_shared<IntData>(timeout);
        td1.rows.emplace("active_timeout", sTimeout);
        std::shared_ptr<StringData> sTarget = make_shared<StringData>(target);
        td1.rows.emplace("targets", sTarget);
        std::shared_ptr<BoolData> bAddIdToInterface = make_shared<BoolData>(addidtointerface);
        td1.rows.emplace("add_id_to_interface", bAddIdToInterface);

        string uuid_name = generateTempUuid();
        td1.kvPairs.emplace("uuid-name", uuid_name);
        uint64_t reqId = getNextId();
        list<transData> tl;
        tl.push_back(td1);
        // msg2
        std::tuple<string, string, string> cond1("_uuid", "==", brUuid);
        set<std::tuple<string, string, string>> condSet;
        condSet.emplace(cond1);
        set<tuple<string, string>> rdata;
        std::shared_ptr<TupleData> tPtr = make_shared<TupleData>(rdata, "set");

        transData td2;
        td2.conditions = condSet;
        td2.operation = "update";
        td2.table = "Bridge";
        rdata.clear();
        rdata.emplace("named-uuid", uuid_name);

        tPtr.reset(new TupleData(rdata));
        td2.rows.emplace("netflow", tPtr);
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

    inline string NetFlowJsonRpc::generateTempUuid() {
        std::random_device rng;
        std::mt19937 urng(rng());
        string uuid_name = to_string(basic_random_generator<std::mt19937>(urng)());
        uuid_name.insert(0,"row");
        std::regex hyph ("-");
        string underscore("_");
        return std::regex_replace(uuid_name, hyph, underscore);
    }

 

    bool NetFlowJsonRpc::handleCreateNetFlowResp(uint64_t reqId,
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

 
    inline bool NetFlowJsonRpc::checkForResponse() {
        unique_lock<mutex> lock(pConn->mtx);
        if (!pConn->ready.wait_for(lock, milliseconds(WAIT_TIMEOUT*1000),
                [=]{return responseReceived;})) {
            LOG(DEBUG) << "lock timed out";
            return false;
        } else {
            return true;
        }
    }


    string NetFlowJsonRpc::getBridgeUuid(const string& name) {
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

    bool NetFlowJsonRpc::handleGetBridgeUuidResp(uint64_t reqId,
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

    bool NetFlowJsonRpc::deleteNetFlow(const string& brName) {
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
            td.rows.emplace("netflow", mPtr);
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

  

    void NetFlowJsonRpc::start() {
        LOG(DEBUG) << "Starting .....";
        pConn = createConnection(*this);
        pConn->start();
    }

    void NetFlowJsonRpc::stop() {
        pConn->stop();
        pConn.reset();
    }

    void NetFlowJsonRpc::connect(string const& hostname, int port) {
        pConn->connect(hostname, port);
    }

} // namespace opflex
