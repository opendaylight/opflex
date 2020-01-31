
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
    using namespace std;

    SpanRenderer::SpanRenderer(Agent& agent_) : JsonRpcRenderer(agent_) {

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
        if (!connect()) {
            LOG(DEBUG) << "failed to connect, retry in " << CONNECTION_RETRY << " seconds";
            // connection failed, start a timer to try again
            connection_timer.reset(new deadline_timer(agent.getAgentIOService(),
                                                      boost::posix_time::seconds(CONNECTION_RETRY)));
            connection_timer->async_wait(boost::bind(&SpanRenderer::delConnectPtrCb, this,
                                                     boost::asio::placeholders::error, seSt));
            timerStarted = true;
            return;
        }
        sessionDeleted(seSt);
        cleanup();
    }

    void SpanRenderer::spanDeleted() {
        LOG(DEBUG) << "deleting mirror";
        unique_lock<mutex> lock(handlerMutex);
         if (!connect()) {
            LOG(DEBUG) << "failed to connect, retry in " << CONNECTION_RETRY <<
                    " seconds";
            // connection failed, start a timer to try again
            connection_timer.reset(new deadline_timer(agent.getAgentIOService(),
                    boost::posix_time::seconds(CONNECTION_RETRY)));
            connection_timer->async_wait(boost::bind(&SpanRenderer::delConnectCb, this,
                    boost::asio::placeholders::error));
            timerStarted = true;
            return;
        }
        sessionDeleted();
        cleanup();
    }
    void SpanRenderer::updateConnectCb(const boost::system::error_code& ec,
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
        spanUpdated(spanURI);
    }

    void SpanRenderer::delConnectPtrCb(const boost::system::error_code& ec,
            shared_ptr<SessionState> pSt) {
        if (ec) {
            connection_timer.reset();
            return;
        }
        LOG(DEBUG) << "timer span del with ptr cb";
        spanDeleted(pSt);
    }

    void SpanRenderer::delConnectCb(const boost::system::error_code& ec) {
        if (ec) {
            connection_timer.reset();
            return;
        }
        LOG(DEBUG) << "timer span del cb";
        spanDeleted();
    }

    void SpanRenderer::sessionDeleted(shared_ptr<SessionState> seSt) {
        deleteMirror(seSt->getName());
        // There is only one ERSPAN port.
        deleteErspnPort(ERSPAN_PORT_NAME);
    }

    void SpanRenderer::sessionDeleted() {
        if (!jRpc->deleteMirror(agent.getOvsdbBridge())) {
            LOG(DEBUG) << "Unable to delete mirror";
            cleanup();
        }
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

        if (!connect()) {
            LOG(DEBUG) << "failed to connect, retry in " << CONNECTION_RETRY << " seconds";
            // connection failed, start a timer to try again

            connection_timer.reset(new deadline_timer(agent.getAgentIOService(),
                                                      milliseconds(CONNECTION_RETRY * 1000)));
            connection_timer->async_wait(boost::bind(&SpanRenderer::updateConnectCb, this,
                                                     boost::asio::placeholders::error, spanURI));
            timerStarted = true;
            LOG(DEBUG) << "conn timer " << connection_timer << ", timerStarted: " << timerStarted;
            cleanup();
            return;
        }

        // get mirror artifacts from OVSDB if provisioned
        JsonRpc::mirror mir;
        bool isMirProv = false;
        if (jRpc->getOvsdbMirrorConfig(mir)) {
            isMirProv = true;
        }

        // There should be at least one source and one destination.
        // Admin state should be ON.
        if (seSt.get()->getSrcEndPointSet().empty() ||
            seSt.get()->getDstEndPointMap().empty() ||
            seSt.get()->getAdminState() == 0) {
            if (isMirProv) {
                LOG(DEBUG) << "deleting mirror";
                sessionDeleted(seSt.get());
            }
            LOG(DEBUG) << "No mirror config";
            cleanup();
            return;
        }

        //get the source ports.
        set<string> srcPort;
        set<string> dstPort;
        modelgbp::gbp::DirectionEnumT dir;

        for (auto src : seSt.get()->getSrcEndPointSet()) {
            if (src->getDirection() == dir.CONST_BIDIRECTIONAL ||
                    src->getDirection() == dir.CONST_OUT) {
                srcPort.emplace(src->getPort());
            }
            if (src->getDirection() == dir.CONST_BIDIRECTIONAL ||
                    src->getDirection() == dir.CONST_IN) {
                dstPort.emplace(src->getPort());
            }
        }

        // check if the number of source and dest ports are the
        // same as provisioned.
        if (srcPort.size() != mir.src_ports.size() ||
                dstPort.size() != mir.dst_ports.size()) {
            LOG(DEBUG) << "updating mirror config";
            updateMirrorConfig(seSt.get());
            cleanup();
            return;
        }

        // compare source port names. If at least one is different, the config
        // has changed.
        for (auto itr = mir.src_ports.begin(); itr != mir.src_ports.end();
                itr++) {
            set<string>::iterator itr1 = srcPort.find(*itr);
            if (itr1 != srcPort.end()) {
                srcPort.erase(itr1);
            } else {
                updateMirrorConfig(seSt.get());
                cleanup();
                return;
            }
        }
        if (!srcPort.empty()) {
            updateMirrorConfig(seSt.get());
            cleanup();
            return;
        }

        for (auto itr = mir.dst_ports.begin(); itr != mir.dst_ports.end();
                itr++) {
            set<string>::iterator itr1 = dstPort.find(*itr);
            if (itr1 != dstPort.end()) {
                dstPort.erase(itr1);
            } else {
                updateMirrorConfig(seSt.get());
                cleanup();
                return;
            }
        }
        if (!dstPort.empty()) {
            updateMirrorConfig(seSt.get());
            cleanup();
            return;
        }

        // check out port config
        // get the destination IPs
        set<address> dstIp;
        for (auto dst : seSt.get()->getDstEndPointMap()) {
            dstIp.emplace(dst.second->getAddress());
        }
        // get the first element of the set as only one
        // destination is allowed in OVS 2.10
        string ipAddr = (*(dstIp.begin())).to_string();
        // get ERSPAN interface params if configured
        shared_ptr<JsonRpc::erspan_ifc> pEp;
        if (!jRpc->getErspanIfcParams(pEp)) {
            LOG(DEBUG) << "Unable to get ERSPAN parameters";
            return;
        }
        // check for change in config, push it if there is a change.
        if (pEp->remote_ip.compare(ipAddr) ||
            pEp->erspan_ver != seSt.get()->getVersion()) {
            updateMirrorConfig(seSt.get());
            cleanup();
            return;
        }
    }

    void SpanRenderer::updateMirrorConfig(shared_ptr<SessionState> seSt) {
        sessionDeleted(seSt);
        // get the source ports.
        set<string> srcPort;
        set<string> dstPort;
        modelgbp::gbp::DirectionEnumT dir;

        for (auto src : seSt->getSrcEndPointSet()) {
            if (src->getDirection() == dir.CONST_BIDIRECTIONAL ||
                    src->getDirection() == dir.CONST_OUT) {
                srcPort.emplace(src->getPort());
            }
            if (src->getDirection() == dir.CONST_BIDIRECTIONAL ||
                    src->getDirection() == dir.CONST_IN) {
                dstPort.emplace(src->getPort());
            }
        }

        // get the destination IPs
        set<address> dstIp;
        for (auto dst : seSt->getDstEndPointMap()) {
            dstIp.emplace(dst.second->getAddress());
        }
        // get the first element of the set as only one
        // destination is allowed in OVS 2.10
        string ipAddr = (*(dstIp.begin())).to_string();
        addErspanPort(agent.getOvsdbBridge(), ipAddr, seSt->getVersion());
        LOG(DEBUG) << "creating mirror";
        createMirror(seSt->getName(), srcPort, dstPort);
    }

    bool SpanRenderer::deleteMirror(const string &sess) {
        LOG(DEBUG) << "deleting mirror";
        if (!jRpc->deleteMirror(agent.getOvsdbBridge())) {
            LOG(DEBUG) << "Unable to delete mirror";
            cleanup();
            return false;
        }
        return true;
    }

    bool SpanRenderer::addErspanPort(const string &brName, const string &ipAddr,
            const uint8_t version) {
        LOG(DEBUG) << "adding erspan port";
       shared_ptr<JsonRpc::erspan_ifc> ep;
        if (version == 1) {
            ep = make_shared<JsonRpc::erspan_ifc_v1>();
            // current OVS implementation supports only one ERSPAN port.
            // use 1 as ersan_idx
            static_pointer_cast<JsonRpc::erspan_ifc_v1>(ep)->erspan_idx = 1;
        } else if (version == 2) {
            ep = make_shared<JsonRpc::erspan_ifc_v2>();
            // current OVS implementation supports one ERSPAN port and mirror
            // choose 1 for hw_id.
            // dir can be set to 0 as it does not have an affect on the mirror traffic.
            static_pointer_cast<JsonRpc::erspan_ifc_v2>(ep)->erspan_hw_id = 1;
            static_pointer_cast<JsonRpc::erspan_ifc_v2>(ep)->erspan_dir = 0;
        } else {
            return false;
        }
        ep->name = ERSPAN_PORT_NAME;
        ep->remote_ip = ipAddr;
        // current OVS implementation supports only one ERPSAN port and mirror.
        // choose 1 as the key.
        ep->key = 1;
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
        if (jRpc->getBridgePortList(agent.getOvsdbBridge(), res)) {
            LOG(DEBUG) << "br UUID " << res.brUuid;
            for (auto& elem : res.portUuids) {
                LOG(DEBUG) << elem;
            }
        }

        tuple<string, set<string>> ports =
                make_tuple(res.brUuid, res.portUuids);
        jRpc->updateBridgePorts(ports, erspanUuid, false);
        return true;
    }
    bool SpanRenderer::createMirror(const string& sess, const set<string>& srcPorts,
            const set<string>& dstPorts) {

        JsonRpc::mirror mir;
        mir.src_ports.insert(srcPorts.begin(), srcPorts.end());
        mir.dst_ports.insert(dstPorts.begin(), dstPorts.end());
        jRpc->addMirrorData("msandhu-sess1", mir);
        string brUuid = jRpc->getBridgeUuid(agent.getOvsdbBridge());
        LOG(DEBUG) << "bridge uuid " << brUuid;

        jRpc->createMirror(brUuid, "msandhu-sess1");
        return true;
    }
}
