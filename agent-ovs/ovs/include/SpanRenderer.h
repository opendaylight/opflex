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
#include <boost/asio.hpp>
#include <opflexagent/PolicyListener.h>
#include <opflexagent/Agent.h>
#include <opflexagent/SpanListener.h>
#include "JsonRpc.h"

namespace opflexagent {

using boost::asio::deadline_timer;
using boost::posix_time::milliseconds;

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
    bool connect();
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
    std::shared_ptr<boost::asio::deadline_timer> connection_timer;
    // retry interval in seconds
    const long CONNECTION_RETRY = 60;
    void updateConnectCb(const boost::system::error_code& ec, const opflex::modb::URI uri);
    void delConnectPtrCb(const boost::system::error_code& ec, shared_ptr<SessionState> pSt);
    void delConnectCb(const boost::system::error_code& ec);
    bool timerStarted = false;

};
}
#endif //OPFLEX_SPANRENDERER_H
