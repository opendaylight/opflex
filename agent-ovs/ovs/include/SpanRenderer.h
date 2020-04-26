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
#include <opflexagent/SpanListener.h>
#include "JsonRpcRenderer.h"

namespace opflexagent {

/**
 * class to render span config on a virtual switch
 */
class SpanRenderer : public SpanListener,
                     public JsonRpcRenderer,
                     private boost::noncopyable {

public:
    /**
     * constructor for SpanRenderer
     * @param agent_ reference to an agent instance
     */
    SpanRenderer(Agent& agent_);

    /**
     * Start the renderer
     * @param swName Switch to connect to
     * @param conn OVSDB connection
     */
    virtual void start(const std::string& swName, OvsdbConnection* conn);

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
    virtual void spanDeleted(const shared_ptr<SessionState>& sesSt);

    /**
     * create mirror
     * @param[in] session name of mirror
     * @param[in] srcPorts source ports
     * @param[in] dstPorts dest ports
     * @return bool true if created successfully, false otherwise.
     */
    bool createMirror(const string& session, const set<string>& srcPorts,
                      const set<string>& dstPorts);

    /**
     * deletes mirror session
     * @param[in] session name of session
     * @return true if success, false otherwise.
     */
    bool deleteMirror(const string& session);

    /**
     * add port used for erspan
     * @param brName bridge to create port on
     * @param ipAddr remote destination
     * @param version erspan version
     * @return true if success, false otherwise.
     */
    bool addErspanPort(const string& brName, const string& ipAddr, const uint8_t version);

    /**
     * delete port used for erspan
     * @param name port name
     * @return true if success, false otherwise.
     */
    bool deleteErspanPort(const string& name);

private:
    /**
     * Compare and update span config
     *
     * @param spanURI URI of the changed span object
     */
    void handleSpanUpdate(const opflex::modb::URI& spanURI);
    virtual void sessionDeleted(const string &sessionName);
    void updateMirrorConfig(const shared_ptr<SessionState>& seSt);
    void updateConnectCb(const boost::system::error_code& ec, const opflex::modb::URI& uri);
    void delConnectPtrCb(const boost::system::error_code& ec, shared_ptr<SessionState> pSt);
};
}
#endif //OPFLEX_SPANRENDERER_H
