/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for NetFlowRenderer
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEX_NETFLOWRENDERER_H
#define OPFLEX_NETFLOWRENDERER_H

#include <boost/noncopyable.hpp>
#include <opflexagent/PolicyListener.h>
#include <opflexagent/Agent.h>
#include <opflexagent/NetFlowListener.h>
#include "JsonRpc.h"

namespace opflexagent {

/**
 * class to render netflow export config on a virtual switch
 */
class NetFlowRenderer : public NetFlowListener,
                     private boost::noncopyable {

public:
    /**
     * constructor for NetFlowRenderer
     * @param agent_ reference to an agent instance
     */
    NetFlowRenderer(Agent& agent_);

    /**
     * Module start
     */
    void start();

    /**
     * Module stop
     */
    void stop();

    virtual void netflowUpdated(const opflex::modb::URI& netflowURI);

    virtual void netflowDeleted();

private:
    /**
     * Compare and update netflow exporter config
     *
     * @param netflowURI URI of the changed netflow exporter object
    */
    void handleNetFlowUpdate(const opflex::modb::URI& netflowURI);
    void cleanup();
    void connect();
    bool deleteNetFlow();
    bool createNetFlow(const string& targets, int timeout);
    Agent& agent;
    TaskQueue taskQueue;
    condition_variable cv;
    mutex handlerMutex;
    unique_ptr<JsonRpc> jRpc;
};
}
#endif //OPFLEX_NETFLOWRENDERER_H
