
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
        deleteMirror(expSt->getName());
        // There is only one ERSPAN port.
       // deleteErspnPort(ERSPAN_PORT_NAME);
    }

    void NetFlowRenderer::handleNetFlowUpdate(const opflex::modb::URI& netFlowURI) {
        LOG(DEBUG) << "NetFlow handle update, thread " << std::this_thread::get_id();
        unique_lock<mutex> lock(handlerMutex);
        NetFlowManager& spMgr = agent.getNetFlowManager();
        optional<shared_ptr<ExporterConfigState>> seSt =
                                             spMgr.getExporterConfigState(netFlowURI);
        // Is the session state pointer set
        if (!seSt) {
            return;
        }

        connect();      
       
     //   sessionDeleted(seSt.get());
     //   addErspanPort(BRIDGE, ipAddr);
        LOG(DEBUG) << "creating mirror";
      // createMirror(seSt.get()->getName(), srcPort, dstIp);
        cleanup();
    }

    inline void NetFlowRenderer::connect() {
        // connect to OVSDB, destination is always the loopback address.
        // TBD: what happens if connect fails
        jRpc = unique_ptr<NetFlowJsonRpc>(new NetFlowJsonRpc());
        jRpc->start();
        jRpc->connect("127.0.0.1", jRpc->OVSDB_RPC_PORT);
    }

    void getOvsSpanArtifacts() {
        // get mirror entry

    }
    bool NetFlowRenderer::deleteMirror(const string& sess) {
        LOG(DEBUG) << "deleting mirror";
        if (!jRpc->deleteMirror(BRIDGE)) {
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

  
    bool NetFlowRenderer::createMirror(const string& sess, const set<string>& srcPort,
            const set<address>& dstIp) {
        NetFlowJsonRpc::mirror mir;
        mir.src_ports = srcPort;
    
        mir.dst_ports = srcPort;
       
     //   mir.out_ports.emplace(ERSPAN_PORT_NAME);

        jRpc->addMirrorData("msandhu-sess1", mir);
        
        

    
        return true;
    }
}
