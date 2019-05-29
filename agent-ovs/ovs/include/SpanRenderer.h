/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SwitchManager
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEX_SPANRENDERER_H
#define OPFLEX_SPANRENDERER_H

#include <boost/noncopyable.hpp>
#include <opflexagent/PolicyListener.h>
#include <opflexagent/Agent.h>
#include <opflexagent/SpanListener.h>

namespace opflexagent {

/**
 * class to render span config on a virtual switch
 */
class SpanRenderer : public SpanListener,
                     private boost::noncopyable {

public:
    /**
     * constructor for SpanRenderer
     * @param agent_ reference to an agent instance
     */
    SpanRenderer(Agent& agent_);

    /**
     * Module start
     */
    void start();

    /**
     * Module stop
     */
    void stop();

    virtual void spanUpdated(const opflex::modb::URI& spanURI);
    virtual void sessionDeleted(shared_ptr<SessionState> sesSt);

private:
    /**
     * Compare and update span config
     *
     * @param spanURI URI of the changed span object
     */
    void handleSpanUpdate(const opflex::modb::URI& spanURI);

    bool deleteErspnPort(string name);
    bool deleteMirror(string session);
    bool createMirror(string session, vector<string> srcPorts, set<address> dstIPs);
    const string ERSPAN_PORT_NAME = "erspn";
    Agent& agent;
    TaskQueue taskQueue;
};
}
#endif //OPFLEX_SPANRENDERER_H
