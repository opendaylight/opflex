
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
        agent.getSpanManager().unregisterListener(this);
    }

    void SpanRenderer::spanUpdated(const opflex::modb::URI& spanURI) {
        taskQueue.dispatch(spanURI.toString(),
                           [=]() { handleSpanUpdate(spanURI); });
    }


    void SpanRenderer::sessionDeleted(shared_ptr<SessionState> seSt) {
        deleteMirror(seSt->getName());
        // There is only one ERSPAN port.
        deleteErspnPort(ERSPAN_PORT_NAME);
    }

    void SpanRenderer::handleSpanUpdate(const opflex::modb::URI& spanURI) {
        LOG(DEBUG) << "Span handle update";
        SpanManager& spMgr = agent.getSpanManager();
        optional<shared_ptr<SessionState>> seSt =
                                             spMgr.getSessionState(spanURI);
        // Is the session state pointer set
        if (!seSt) {
            return;
        }
        // There should be at least one source and one destination.
        // need to accommodate for a change from previous configuration.
        if (seSt.get()->getSrcEndPointMap().empty() ||
            seSt.get()->getDstEndPointMap().empty()) {
            LOG(DEBUG) << "Delete existing mirror if any";
            sessionDeleted(seSt.get());
            return;
        }
        //get the source ports.
        vector<string> srcPort;
        for (auto src : seSt.get()->getSrcEndPointMap()) {
            srcPort.push_back(src.second.get()->getPort());
        }
        // get the destination IPs
        set<address> dstIp;
        for (auto dst : seSt.get()->getDstEndPointMap()) {
            dstIp.insert(dst.second.get()->getAddress());
        }

        // delete existing mirror and erspan port, then create a new one.
        sessionDeleted(seSt.get());
        LOG(DEBUG) << "creating mirror";
        createMirror(seSt.get()->getName(), srcPort, dstIp);
    }

    bool SpanRenderer::deleteMirror(string sess) {
    }

    bool SpanRenderer::deleteErspnPort(const string name) {
    }
    bool SpanRenderer::createMirror(string sess, vector<string> srcPort, set<address> dstIp) {
    }
}
