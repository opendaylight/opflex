
/*
 * Copyright (c) 2014-2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>

#include "JsonRpcRenderer.h"
#include <opflexagent/logging.h>
#include <opflexagent/NetFlowManager.h>
#include <boost/optional.hpp>

#include <boost/range/adaptors.hpp>
#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
namespace opflexagent {
    using boost::optional;
    using namespace boost::adaptors;

    JsonRpcRenderer::JsonRpcRenderer(Agent& agent_) : agent(agent_),taskQueue(agent.getAgentIOService()) {

    }
    bool JsonRpcRenderer::connect() {
        // connect to OVSDB, destination is specified in agent config file.
        // If not the default is applied
        // If connection fails, a timer is started to retry and
        // back off at periodic intervals.
        if (timerStarted) {
            LOG(DEBUG) << "Cancelling timer";
            connection_timer->cancel();
            timerStarted = false;
        }
        jRpc = unique_ptr<JsonRpc>(new JsonRpc());
        jRpc->start();
        jRpc->connect(agent.getOvsdbIpAddress(), agent.getOvsdbPort());
        if (!jRpc->isConnected()) {
            return false;
        }
        return true;
    }
  void JsonRpcRenderer::cleanup() {
        jRpc->stop();
        jRpc.release();
    }
}
