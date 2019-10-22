/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SpanRenderer
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
#include "JsonRpc.h"

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

    /**
     * handle updates to span artifacts
     * @param[in] spanURI URI pointing to a span session.
     */
    virtual void spanUpdated(const opflex::modb::URI& spanURI);

    /**
     * delete span pointed to by the pointer
     * @param[in] sesSt shared pointer to a Session object
     */
    virtual void spanDeleted(shared_ptr<SessionState> sesSt);

    /**
     * delete all span artifacts
     */
    virtual void spanDeleted();

private:
    /**
     * Compare and update span config
     *
     * @param spanURI URI of the changed span object
     */
    void handleSpanUpdate(const opflex::modb::URI& spanURI);
    virtual void sessionDeleted(shared_ptr<SessionState> sesSt);
    virtual void sessionDeleted();
    void cleanup();
    void connect();
    bool deleteErspnPort(const string& name);
    bool deleteMirror(const string& session);
    bool createMirror(const string& session, const set<string>& srcPorts,
            const set<string>& dstPorts);
    bool addErspanPort(const string& brName, const string& ipAddr);
    void updateMirrorConfig(shared_ptr<SessionState> seSt);
    Agent& agent;
    TaskQueue taskQueue;
    condition_variable cv;
    mutex handlerMutex;
    unique_ptr<JsonRpc> jRpc;
};
}
#endif //OPFLEX_SPANRENDERER_H
