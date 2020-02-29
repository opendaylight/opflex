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
#include <opflexagent/NetFlowListener.h>
#include "JsonRpcRenderer.h"


namespace opflexagent {

/**
 * class to render netflow export config on a virtual switch
 */
class NetFlowRenderer : public NetFlowListener,
                     private JsonRpcRenderer,
                     private boost::noncopyable {

public:
    /**
     * constructor for NetFlowRenderer
     * @param agent_ reference to an agent instance
     */
    NetFlowRenderer(Agent& agent_);

    /**
     * Start the renderer
     * @param swName Switch to connect to
     */
    virtual void start(const std::string& swName);

    /**
     * Module stop
     */
    void stop();

    virtual void exporterUpdated(const opflex::modb::URI& netflowURI);

    virtual void exporterDeleted(shared_ptr<ExporterConfigState>& expSt);

private:
    /**
     * Compare and update netflow exporter config
     *
     * @param netflowURI URI of the changed netflow exporter object
    */
    void handleNetFlowUpdate(const opflex::modb::URI& netflowURI);
    bool deleteNetFlow();
    bool createNetFlow(const string& targets, int timeout);
    bool deleteIpfix();
    bool createIpfix(const string& targets, int sample);
    void updateConnectCb(const boost::system::error_code& ec, const opflex::modb::URI& uri);
    void delConnectCb(const boost::system::error_code& ec, shared_ptr<ExporterConfigState> expSt);
};
}
#endif //OPFLEX_NETFLOWRENDERER_H
