
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

    NetFlowRenderer::NetFlowRenderer(Agent& agent_) : agent(agent_),
                                                taskQueue(agent.getAgentIOService()) {

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
        handleNetFlowUpdate(netFlowURI);
    }

    void NetFlowRenderer::netflowDeleted(shared_ptr<ExporterConfigState> expSt) {
        unique_lock<mutex> lock(handlerMutex);
        connect();
        exporterconfigDeleted(expSt);
        cleanup();
    }

    void NetFlowRenderer::exporterconfigDeleted(shared_ptr<ExporterConfigState> expSt) {
        deleteNetFlow();
      
    }

    void NetFlowRenderer::handleNetFlowUpdate(const opflex::modb::URI& netFlowURI) {
        LOG(DEBUG) << "NetFlow handle update, thread " << std::this_thread::get_id();
        unique_lock<mutex> lock(handlerMutex);
        NetFlowManager& spMgr = agent.getNetFlowManager();
        optional<shared_ptr<ExporterConfigState>> expSt =
                                             spMgr.getExporterConfigState(netFlowURI);
        // Is the session state pointer set
        if (!expSt) {
            return;
        }
        connect();           
       // exporterconfigDeleted(expSt.get());    
        LOG(DEBUG) << "creating netflow";
        std::string target = expSt.get()->getDstAddress() + ":" ;
        target += expSt.get()->getDestinationPort();
        uint32_t timeout =  expSt.get()->getActiveFlowTimeOut();
        std::string strtimeout = std::to_string(timeout);
        createNetFlow(target, strtimeout);
        cleanup();
    }

    inline void NetFlowRenderer::connect() {
        // connect to OVSDB, destination is always the loopback address.
        jRpc = unique_ptr<NetFlowJsonRpc>(new NetFlowJsonRpc());
        jRpc->start();
        jRpc->connect("127.0.0.1", jRpc->OVSDB_RPC_PORT);
    }

    
    bool NetFlowRenderer::deleteNetFlow() {
        LOG(DEBUG) << "deleting mirror";
        if (!jRpc->deleteNetFlow(agent.getOvsdbBridge())) {
            LOG(DEBUG) << "Unable to delete mirror";
            cleanup();
            return false;
        }
        return true;
    }

    void NetFlowRenderer::cleanup() {
        jRpc->stop();
        jRpc.release();
    }

  
    bool NetFlowRenderer::createNetFlow(const string& targets, const string& timeout){
       
       string brUuid = jRpc->getBridgeUuid(agent.getOvsdbBridge());
        LOG(DEBUG) << "bridge uuid " << brUuid;
       jRpc->createNetFlow(brUuid,targets,timeout);
       return true;
    }
}
