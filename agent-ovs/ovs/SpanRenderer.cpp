
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


namespace opflexagent {
    using boost::optional;

    SpanRenderer::SpanRenderer(Agent& agent_) : agent(agent_),
                                                taskQueue(agent.getAgentIOService()) {

    }

    void SpanRenderer::start() {
        LOG(DEBUG) << "starting span renderer";
        agent.getSpanManager().registerListener(this);
    }

    void SpanRenderer::spanUpdated(const opflex::modb::URI& spanURI) {
        taskQueue.dispatch(spanURI.toString(),
                           [=]() { handleSpanUpdate(spanURI); });
    }


    void SpanRenderer::handleSpanUpdate(const opflex::modb::URI& spanURI) {
        LOG(DEBUG) << "Span handle update";
        SpanManager& spMgr = agent.getSpanManager();
        optional<shared_ptr<SpanManager::SessionState>> seSt =
                                             spMgr.getSessionState(spanURI);
        // Is the session state pointer set
        if (!seSt) {
            return;
        }
        // There should be at least one source and one destination.
        // need to accomodate for a change from previous configuration.
        if (seSt.get()->getSrcEndPointMap().empty() ||
            seSt.get()->getDstEndPointMap().empty()) {
            return;
        }
    }


}
