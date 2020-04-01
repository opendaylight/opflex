
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

    void NetFlowRenderer::start(const std::string& swName, OvsdbConnection* conn) {
        LOG(DEBUG) << "starting NetFlowRenderer renderer";
        JsonRpcRenderer::start(swName, conn);
        agent.getNetFlowManager().registerListener(this);
    }

    void NetFlowRenderer::stop() {
        LOG(DEBUG) << "stopping NetFlowRenderer renderer";
        agent.getNetFlowManager().unregisterListener(this);
    }

    void NetFlowRenderer::exporterUpdated(const opflex::modb::URI& netFlowURI) {
        LOG(DEBUG) << "NetFlowRenderer exporter updated";
        handleNetFlowUpdate(netFlowURI);
    }

    void NetFlowRenderer::exporterDeleted(const shared_ptr<ExporterConfigState>& expSt) {
        LOG(DEBUG) << "deleting exporter";
        unique_lock<mutex> lock(handlerMutex);
        if (!expSt) {
            return;
        }
        if (!connect()) {
            LOG(DEBUG) << "failed to connect, retry in " << CONNECTION_RETRY << " seconds";
            // connection failed, start a timer to try again
            connection_timer.reset(new deadline_timer(agent.getAgentIOService(),
                                                        boost::posix_time::seconds(CONNECTION_RETRY)));
            connection_timer->async_wait(boost::bind(&NetFlowRenderer::delConnectCb, this,
                                                    boost::asio::placeholders::error, expSt));
            timerStarted = true;
            return;
        }
        if (expSt->getVersion() ==  CollectorVersionEnumT::CONST_V5) {
            deleteNetFlow();
        } else if(expSt->getVersion() == CollectorVersionEnumT::CONST_V9) {
            deleteIpfix();
        }
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
        if (!connect()) {
            LOG(DEBUG) << "failed to connect, retry in " << CONNECTION_RETRY << " seconds";
            // connection failed, start a timer to try again

            connection_timer.reset(new deadline_timer(agent.getAgentIOService(),
                                                      milliseconds(CONNECTION_RETRY * 1000)));
            connection_timer->async_wait(boost::bind(&NetFlowRenderer::updateConnectCb, this,
                                                     boost::asio::placeholders::error, netFlowURI));
            timerStarted = true;
            LOG(DEBUG) << "conn timer " << connection_timer << ", timerStarted: " << timerStarted;
            return;
        }
        std::string target = expSt.get()->getDstAddress() + ":";
        std::string port = std::to_string(expSt.get()->getDestinationPort());
        target += port;
        LOG(DEBUG) << "netflow/ipfix target " << target.c_str() << " version is " << std::to_string(expSt.get()->getVersion());

        if (expSt.get()->getVersion() == CollectorVersionEnumT::CONST_V5) {
            LOG(DEBUG) << "creating netflow";
            uint32_t timeout = expSt.get()->getActiveFlowTimeOut();
            LOG(DEBUG) << "netflow timeout " << timeout;
            createNetFlow(target, timeout);
        } else if (expSt.get()->getVersion() ==
                   CollectorVersionEnumT::CONST_V9) {
            LOG(DEBUG) << "creating IPFIX";
            uint32_t sampling = expSt.get()->getSamplingRate();
            createIpfix(target, sampling);
        }
    }

    bool NetFlowRenderer::deleteNetFlow() {
        LOG(DEBUG) << "deleting netflow";
        if (!jRpc->deleteNetFlow(switchName))
        {
            LOG(DEBUG) << "Unable to delete netflow";
            return false;
        }
        return true;
    }

      bool NetFlowRenderer::deleteIpfix() {
        LOG(DEBUG) << "deleting IPFIX";
        if (!jRpc->deleteIpfix(switchName))
        {
            LOG(DEBUG) << "Unable to delete ipfix";
            return false;
        }
        return true;
    }

    bool NetFlowRenderer::createNetFlow(const string &targets, int timeout) {
        // ensure any previous netflow/ipfix destinations are removed first
        jRpc->deleteNetFlow(switchName);
        jRpc->deleteIpfix(switchName);
        string brUuid;
        jRpc->getBridgeUuid(switchName, brUuid);
        LOG(DEBUG) << "bridge uuid " << brUuid;
        jRpc->createNetFlow(brUuid, targets, timeout);
        return true;
    }

    bool NetFlowRenderer::createIpfix(const string &targets, int sampling) {
        // ensure any previous netflow/ipfix destinations are removed first
        jRpc->deleteNetFlow(switchName);
        jRpc->deleteIpfix(switchName);
        string brUuid;
        jRpc->getBridgeUuid(switchName, brUuid);
        LOG(DEBUG) << "bridge uuid " << brUuid << "sampling rate is " << sampling;
        return jRpc->createIpfix(brUuid, targets, sampling);
    }

    void NetFlowRenderer::updateConnectCb(const boost::system::error_code& ec,
            const opflex::modb::URI& spanURI) {
        LOG(DEBUG) << "timer update cb";
        if (ec) {
            string cat = string(ec.category().name());
            LOG(DEBUG) << "timer error " << cat << ":" << ec.value();
            if (!(cat == "system" &&
                ec.value() == 125)) {
                connection_timer->cancel();
                timerStarted = false;
            }
            return;
        }

        exporterUpdated(spanURI);
    }

    void NetFlowRenderer::delConnectCb(const boost::system::error_code& ec,
                                       shared_ptr<ExporterConfigState> expSt) {
        if (ec) {
            connection_timer.reset();
            return;
        }
        LOG(DEBUG) << "timer span del cb";
        exporterDeleted(expSt);
    }


}
