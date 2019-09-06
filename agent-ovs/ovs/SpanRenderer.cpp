
/*
 * Copyright (c) 2014-2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>

#include "SpanRenderer.h"
#include <opflexagent/logging.h>
#include <opflexagent/SpanManager.h>
#include <boost/optional.hpp>

#include <boost/range/adaptors.hpp>
#include <boost/format.hpp>


namespace opflexagent {
    using boost::optional;
    using namespace boost::adaptors;

    SpanRenderer::SpanRenderer(Agent& agent_) : agent(agent_),
                                                taskQueue(agent.getAgentIOService()) {

    }

    void SpanRenderer::start() {
        LOG(DEBUG) << "starting span renderer";
        agent.getSpanManager().registerListener(this);
    }

    void SpanRenderer::stop() {
        LOG(DEBUG) << "stopping span renderer";
        agent.getSpanManager().unregisterListener(this);
    }

    void SpanRenderer::spanUpdated(const opflex::modb::URI& spanURI) {
        handleSpanUpdate(spanURI);
    }

    void SpanRenderer::spanDeleted(shared_ptr<SessionState> seSt) {
        unique_lock<mutex> lock(handlerMutex);
        connect();
        sessionDeleted(seSt);
        cleanup();
    }

    void SpanRenderer::sessionDeleted(shared_ptr<SessionState> seSt) {
        deleteMirror(seSt->getName());
        // There is only one ERSPAN port.
        deleteErspnPort(ERSPAN_PORT_NAME);
    }

    void SpanRenderer::handleSpanUpdate(const opflex::modb::URI& spanURI) {
        LOG(DEBUG) << "Span handle update, thread " << std::this_thread::get_id();
        unique_lock<mutex> lock(handlerMutex);
        SpanManager& spMgr = agent.getSpanManager();
        optional<shared_ptr<SessionState>> seSt =
                                             spMgr.getSessionState(spanURI);
        // Is the session state pointer set
        if (!seSt) {
            return;
        }

        connect();

        // There should be at least one source and one destination.
        // TBD: need to accommodate for a change from previous configuration.
        if (seSt.get()->getSrcEndPointMap().empty() ||
            seSt.get()->getDstEndPointMap().empty()) {
            sessionDeleted(seSt.get());
            cleanup();
            return;
        }
        //get the source ports.
        set<string> srcPort;
        for (auto src : seSt.get()->getSrcEndPointMap()) {
            srcPort.emplace(src.second.get()->getPort());
        }
        // get the destination IPs
        set<address> dstIp;
        for (auto dst : seSt.get()->getDstEndPointMap()) {
            dstIp.emplace(dst.second.get()->getAddress());
        }
        // get the first element of the set as only one
        // destination is allowed in OVS 2.10
        string ipAddr = (*(dstIp.begin())).to_string();
        // delete existing mirror and erspan port, then create a new one.
        sessionDeleted(seSt.get());
        addErspanPort(BRIDGE, ipAddr);
        LOG(DEBUG) << "creating mirror";
        createMirror(seSt.get()->getName(), srcPort, dstIp);
        cleanup();
    }

    inline void SpanRenderer::connect() {
        // connect to OVSDB, destination is always the loopback address.
        // TBD: what happens if connect fails
        jRpc = unique_ptr<JsonRpc>(new JsonRpc());
        jRpc->start();
        jRpc->connect("127.0.0.1", jRpc->OVSDB_RPC_PORT);
    }

    void getOvsSpanArtifacts() {
        // get mirror entry

    }
    bool SpanRenderer::deleteMirror(const string& sess) {
        LOG(DEBUG) << "deleting mirror";
        if (!jRpc->deleteMirror(BRIDGE)) {
            LOG(DEBUG) << "Unable to delete mirror";
            cleanup();
            return false;
        }
        return true;
    }

    void SpanRenderer::cleanup() {
        jRpc->stop();
        jRpc.release();
    }

    bool SpanRenderer::addErspanPort(const string& brName, const string& ipAddr) {
        LOG(DEBUG) << "deleting erspan port";
        JsonRpc::erspan_port ep;
        ep.name = ERSPAN_PORT_NAME;
        ep.ip_address = ipAddr;
        if (!jRpc->addErspanPort(brName, ep)) {
            LOG(DEBUG) << "add erspan port failed";
            return false;
        }
        return true;
    }

    bool SpanRenderer::deleteErspnPort(const string& name) {
        string erspanUuid = jRpc->getPortUuid(ERSPAN_PORT_NAME);
        LOG(DEBUG) << ERSPAN_PORT_NAME << " uuid: " << erspanUuid;
        JsonRpc::BrPortResult res;
        if (jRpc->getBridgePortList("br-int", res)) {
            LOG(DEBUG) << "br UUID " << res.brUuid;
            for (auto elem : res.portUuids) {
                LOG(DEBUG) << elem;
            }
        }

        tuple<string, set<string>> ports =
                make_tuple(res.brUuid, res.portUuids);
        jRpc->updateBridgePorts(ports, erspanUuid, false);
        return true;
    }
    bool SpanRenderer::createMirror(const string& sess, const set<string>& srcPort,
            const set<address>& dstIp) {
        JsonRpc::mirror mir;
        mir.src_ports = srcPort;
        //.insert(src_ports.begin(), src_ports.end());
        mir.dst_ports = srcPort;
        //.insert(dst_ports.begin(), dst_ports.end());
        mir.out_ports.emplace(ERSPAN_PORT_NAME);

        jRpc->addMirrorData("msandhu-sess1", mir);
        string brUuid = jRpc->getBridgeUuid("br-int");
        LOG(DEBUG) << "bridge uuid " << brUuid;

        jRpc->createMirror(brUuid, "msandhu-sess1");
        return true;
    }
}
