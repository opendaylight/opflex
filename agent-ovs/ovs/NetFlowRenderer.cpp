
/*
 * Copyright (c) 2014-2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>

#include "NetFlowRenderer.h"
#include <opflexagent/logging.h>
#include <opflexagent/NetFlowManager.h>
#include <boost/optional.hpp>

#include <boost/range/adaptors.hpp>
#include <boost/format.hpp>


namespace opflexagent {
    using boost::optional;
    using namespace boost::adaptors;

    NetFlowRenderer::NetFlowRenderer(Agent& agent_) : JsonRpcRenderer(agent_) {

    }

    void NetFlowRenderer::start() {
        LOG(DEBUG) << "starting NetFlowRenderer renderer";
        agent.getNetFlowManager().registerListener(this);
    }

    void NetFlowRenderer::stop() {
        LOG(DEBUG) << "stopping NetFlowRenderer renderer";
        agent.getNetFlowManager().unregisterListener(this);
    }

    void NetFlowRenderer::netflowUpdated(const opflex::modb::URI& netFlowURI) {
        LOG(DEBUG) << "NetFlowRenderer netflowUpdated";
        handleNetFlowUpdate(netFlowURI);
    }

    void NetFlowRenderer::netflowDeleted() {
        unique_lock<mutex> lock(handlerMutex);
        connect();
        deleteNetFlow();
        cleanup();
    }

    void NetFlowRenderer::handleNetFlowUpdate(const opflex::modb::URI& netFlowURI) {
        LOG(DEBUG) << "NetFlow handle update, thread " << std::this_thread::get_id();
        unique_lock<mutex> lock(handlerMutex);
        NetFlowManager &spMgr = agent.getNetFlowManager();
        optional<shared_ptr<ExporterConfigState>> expSt =
            spMgr.getExporterConfigState(netFlowURI);
        // Is the exporter config state pointer set
        if (!expSt) {
            return;
        }
        if (!connect())
        {
            LOG(DEBUG) << "failed to connect, retry in " << CONNECTION_RETRY << " seconds";
            // connection failed, start a timer to try again

            connection_timer.reset(new deadline_timer(agent.getAgentIOService(),
                                                      milliseconds(CONNECTION_RETRY * 1000)));
            connection_timer->async_wait(boost::bind(&NetFlowRenderer::updateConnectCb, this,
                                                     boost::asio::placeholders::error, netFlowURI));
            timerStarted = true;
            LOG(DEBUG) << "conn timer " << connection_timer << ", timerStarted: " << timerStarted;
            cleanup();
            return;
        }

        LOG(DEBUG) << "creating netflow";
        std::string target = expSt.get()->getDstAddress() + ":";
        std::string port = std::to_string(expSt.get()->getDestinationPort());
        target += port;
        uint32_t timeout = expSt.get()->getActiveFlowTimeOut();
        LOG(DEBUG) << "netflow target " << target.c_str();
        LOG(DEBUG) << "netflow timeout " << timeout;
        createNetFlow(target, timeout);
        cleanup();
    }

    bool NetFlowRenderer::deleteNetFlow() {
        LOG(DEBUG) << "deleting netflow";
        if (!jRpc->deleteNetFlow(agent.getOvsdbBridge()))
        {
            LOG(DEBUG) << "Unable to delete netflow";
            cleanup();
            return false;
        }
        return true;
    }

    bool NetFlowRenderer::createNetFlow(const string &targets, int timeout) {
        LOG(DEBUG) << "createNetFlow:";
        string brUuid = jRpc->getBridgeUuid(agent.getOvsdbBridge());
        LOG(DEBUG) << "bridge uuid " << brUuid;
        jRpc->createNetFlow(brUuid, targets, timeout);
        return true;
    }

    void NetFlowRenderer::updateConnectCb(const boost::system::error_code& ec,
            const opflex::modb::URI& spanURI) {
        LOG(DEBUG) << "timer update cb";
        if (ec) {
            string cat = string(ec.category().name());
            LOG(DEBUG) << "timer error " << cat << ":" << ec.value();
            if (!(cat.compare("system") == 0 &&
                ec.value() == 125)) {
                connection_timer->cancel();
                timerStarted = false;
            }
            return;
        }
        netflowUpdated(spanURI);
    }

    void NetFlowRenderer::delConnectCb(const boost::system::error_code& ec) {
        if (ec) {
            connection_timer.reset();
            return;
        }
        LOG(DEBUG) << "timer span del cb";
        netflowDeleted();
    }


}
