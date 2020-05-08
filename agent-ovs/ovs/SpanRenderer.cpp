
/*
 * Copyright (c) 2014-2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "SpanRenderer.h"
#include <opflexagent/logging.h>
#include <opflexagent/SpanManager.h>
#include <boost/optional.hpp>


namespace opflexagent {
    using boost::optional;
    using namespace std;
    using modelgbp::gbp::DirectionEnumT;

    SpanRenderer::SpanRenderer(Agent& agent_) : JsonRpcRenderer(agent_) {}

    void SpanRenderer::start(const std::string& swName, OvsdbConnection* conn) {
        LOG(DEBUG) << "starting span renderer";
        JsonRpcRenderer::start(swName, conn);
        agent.getSpanManager().registerListener(this);
    }

    void SpanRenderer::stop() {
        LOG(DEBUG) << "stopping span renderer";
        agent.getSpanManager().unregisterListener(this);
    }

    void SpanRenderer::spanUpdated(const opflex::modb::URI& spanURI) {
        handleSpanUpdate(spanURI);
    }

    void SpanRenderer::spanDeleted(const shared_ptr<SessionState>& seSt) {
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
        sessionDeleted(seSt->getName());
    }

    void SpanRenderer::updateConnectCb(const boost::system::error_code& ec,
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
        spanUpdated(spanURI);
    }

    void SpanRenderer::delConnectPtrCb(const boost::system::error_code& ec, const shared_ptr<SessionState>& pSt) {
        if (ec) {
            connection_timer.reset();
            return;
        }
        LOG(DEBUG) << "timer span del with ptr cb";
        spanDeleted(pSt);
    }

    void SpanRenderer::sessionDeleted(const string& sessionName) {
        LOG(INFO) << "deleting session " << sessionName;
        deleteMirror(sessionName);
        LOG(INFO) << "deleting erspan port " << (ERSPAN_PORT_PREFIX + sessionName);
        deleteErspanPort(ERSPAN_PORT_PREFIX + sessionName);
    }

    void SpanRenderer::handleSpanUpdate(const opflex::modb::URI& spanURI) {
        unique_lock<mutex> lock(handlerMutex);
        SpanManager& spMgr = agent.getSpanManager();
        lock_guard<recursive_mutex> guard(opflexagent::SpanManager::updates);
        optional<shared_ptr<SessionState>> seSt = spMgr.getSessionState(spanURI);
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
            return;
        }

        // get mirror artifacts from OVSDB if provisioned
        JsonRpc::mirror mir;
        bool isMirProv = false;
        if (jRpc->getOvsdbMirrorConfig(seSt.get()->getName(), mir)) {
            isMirProv = true;
        }

        // There should be at least one source and the destination should be set
        // Admin state should be ON.
        if (seSt.get()->hasSrcEndpoints() ||
            !seSt.get()->getDestination().is_unspecified() ||
            seSt.get()->getAdminState() == 0) {
            if (isMirProv) {
                sessionDeleted(seSt.get()->getName());
            }
        } else {
            LOG(INFO) << "Incomplete mirror config. Either admin down or missing src/dest EPs";
            return;
        }

        //get the source ports.
        set<string> srcPort;
        set<string> dstPort;

        SessionState::srcEpSet srcEps;
        seSt.get()->getSrcEndpointSet(srcEps);
        for (auto& src : srcEps) {
            if (src.getDirection() == DirectionEnumT::CONST_BIDIRECTIONAL ||
                    src.getDirection() == DirectionEnumT::CONST_OUT) {
                srcPort.emplace(src.getPort());
            }
            if (src.getDirection() == DirectionEnumT::CONST_BIDIRECTIONAL ||
                    src.getDirection() == DirectionEnumT::CONST_IN) {
                dstPort.emplace(src.getPort());
            }
        }

        // check if the number of source and dest ports are the
        // same as provisioned.
        if (srcPort.size() != mir.src_ports.size() ||
                dstPort.size() != mir.dst_ports.size()) {
            LOG(DEBUG) << "updating mirror config";
            updateMirrorConfig(seSt.get());
            return;
        }

        // compare source port names. If at least one is different, the config
        // has changed.
        for (const auto& src_port : mir.src_ports) {
            auto itr = srcPort.find(src_port);
            if (itr != srcPort.end()) {
                srcPort.erase(itr);
            } else {
                updateMirrorConfig(seSt.get());
                return;
            }
        }
        if (!srcPort.empty()) {
            updateMirrorConfig(seSt.get());
            return;
        }

        for (const auto& dst_port : mir.dst_ports) {
            auto itr = dstPort.find(dst_port);
            if (itr != dstPort.end()) {
                dstPort.erase(itr);
            } else {
                updateMirrorConfig(seSt.get());
                return;
            }
        }
        if (!dstPort.empty()) {
            updateMirrorConfig(seSt.get());
            return;
        }

        // get ERSPAN interface params if configured
        ErspanParams params;
        if (!jRpc->getCurrentErspanParams(ERSPAN_PORT_PREFIX + seSt.get()->getName(), params)) {
            LOG(DEBUG) << "Unable to get ERSPAN parameters";
            return;
        }
        // check for change in config, push it if there is a change.
        if (params.getRemoteIp() != seSt.get()->getDestination().to_string() ||
            params.getVersion() != seSt.get()->getVersion()) {
            LOG(INFO) << "Mirror config has changed for " << seSt.get()->getName();
            updateMirrorConfig(seSt.get());
            return;
        }
    }

    void SpanRenderer::updateMirrorConfig(const shared_ptr<SessionState>& seSt) {
        // get the source ports.
        set<string> srcPort;
        set<string> dstPort;
        SessionState::srcEpSet srcEps;
        seSt->getSrcEndpointSet(srcEps);
        for (auto& src : srcEps) {
            if (src.getDirection() == DirectionEnumT::CONST_BIDIRECTIONAL ||
                src.getDirection() == DirectionEnumT::CONST_OUT) {
                srcPort.emplace(src.getPort());
            }
            if (src.getDirection() == DirectionEnumT::CONST_BIDIRECTIONAL ||
                src.getDirection() == DirectionEnumT::CONST_IN) {
                dstPort.emplace(src.getPort());
            }
        }

        deleteErspanPort(ERSPAN_PORT_PREFIX + seSt->getName());
        addErspanPort(ERSPAN_PORT_PREFIX + seSt->getName(), seSt->getDestination().to_string(), seSt->getVersion());
        LOG(DEBUG) << "creating mirror";
        deleteMirror(seSt->getName());
        createMirror(seSt->getName(), srcPort, dstPort);
    }

    bool SpanRenderer::deleteMirror(const string& sessionName) {
        LOG(DEBUG) << "deleting mirror " << sessionName;
        if (!jRpc->deleteMirror(switchName, sessionName)) {
            LOG(DEBUG) << "Unable to delete mirror";
            return false;
        }
        return true;
    }

    bool SpanRenderer::addErspanPort(const string& portName, const string &ipAddr, const uint8_t version) {
        LOG(DEBUG) << "adding erspan port " << portName << " IP " << ipAddr << " and version " << std::to_string(version);
        ErspanParams params;
        params.setVersion(version);
        params.setPortName(portName);
        params.setRemoteIp(ipAddr);
        if (!jRpc->addErspanPort(switchName, params)) {
            LOG(DEBUG) << "add erspan port failed";
            return false;
        }
        return true;
    }

    bool SpanRenderer::deleteErspanPort(const string& name) {
        LOG(DEBUG) << "deleting erspan port " << name;
        string erspanUuid;
        jRpc->getPortUuid(name, erspanUuid);
        if (erspanUuid.empty()) {
            LOG(WARNING) << "Can't find port named " << name;
            return false;
        }
        LOG(DEBUG) << name << " port uuid: " << erspanUuid;
        jRpc->updateBridgePorts(switchName, erspanUuid, false);
        return true;
    }
    bool SpanRenderer::createMirror(const string& sess, const set<string>& srcPorts,
            const set<string>& dstPorts) {
        string brUuid;
        jRpc->getBridgeUuid(switchName, brUuid);
        LOG(DEBUG) << "bridge uuid " << brUuid;
        jRpc->createMirror(brUuid, sess, srcPorts, dstPorts);
        return true;
    }
}
