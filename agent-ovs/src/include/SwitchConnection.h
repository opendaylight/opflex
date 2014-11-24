/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef _SWITCHCONNECTION_H_
#define _SWITCHCONNECTION_H_

#include <queue>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include "ovs.h"

namespace opflex {
namespace enforcer {

class SwitchConnection;

/**
 * @brief Abstract base-class for a OpenFlow message handler.
 */
class MessageHandler {
public:
    /**
     * Called after a message is received.
     * @param swConn Connection where message was received
     * @param msgType Type of the received message
     * @param msg The received message
     */
    virtual void Handle(SwitchConnection *swConn, ofptype msgType,
            ofpbuf *msg) = 0;
};

/**
 * @brief Abstract base-class for handling on-connect events.
 */
class OnConnectListener {
public:
    /**
     * Called after every successful connection attempt.
     * @param swConn Connection that became connected
     */
    virtual void Connected(SwitchConnection *swConn) = 0;
};

/**
 * @brief Class to handle communication with the OpenFlow switch.
 * Allows sending of OpenFlow messages and respond to received
 * messages by registering message-handlers. If connection to the
 * switch is lost, tries to automatically reconnect till an
 * explicit disconnect is issued. OnConnect listeners are fired
 * each time a connection is established successfully.
 */
class SwitchConnection {
public:
    SwitchConnection(const std::string& swName);
    ~SwitchConnection();

    /**
     * Connect to the switch and monitor the connection.
     * @param protoVer Version of OpenFlow protocol to use
     * @return 0 on success, openvswitch error code on failure
     */
    int Connect(ofp_version protoVer);

    /**
     * Disconnect from switch.
     */
    void Disconnect();

    /**
     * Returns true if connected to the switch.
     */
    bool IsConnected();

    /**
     * Register handler for on-connect events.
     * @param l Listener to register
     */
    void RegisterOnConnectListener(OnConnectListener *l);

    /**
     * Unregister a previously registered on-connect listener.
     * @param l Listener to unregister
     */
    void UnregisterOnConnectListener(OnConnectListener *l);

    /**
     * Register handler for an OpenFlow message.
     * @param msgType OpenFlow message type to register for
     * @param handler Handler to register
     */
    void RegisterMessageHandler(ofptype msgType, MessageHandler *handler);

    /**
     * Unregister a previously registered handler for OpenFlow message.
     * @param msgType OpenFlow message type to unregister for
     * @param handler Handler to unregister
     */
    void UnregisterMessageHandler(ofptype msgType, MessageHandler *handler);

    /**
     * Send a message to the switch.
     * @return 0 on success, openvswitch error code on failure
     */
    virtual int SendMessage(ofpbuf *msg);

    /**
     * Returns the OpenFlow protocol version being used by the connection.
     */
    virtual ofp_version GetProtocolVersion();

    /** Interface: Boost thread */
    void operator()();

private:
    /**
     * Does actual work of establishing connection to the switch.
     * @return 0 on success, openvswitch error code on failure
     */
    int DoConnect();

    /**
     * Main loop for receiving messages on the connection, monitoring
     * it for liveness and automatic reconnection.
     * Runs in a thread of its own.
     */
    void Monitor();

    /**
     * Check if any messages were received and handle them.
     * @return 0 on success, openvswitch error code on failure
     */
    int ReceiveMessage();

    /**
     * Make the poll-loop watch for activity on poll-event-FD.
     * Needs to be done repeatedly because the poll-loop
     * watches the FD exactly once.
     * Must be called from the poll-loop thread.
     */
    void WatchPollEvent();

    /**
     * Cause monitor poll-loop to break-out.
     * @return true if successful in sending event
     */
    bool SignalPollEvent();

    /**
     * Invoke all on-connect listeners
     */
    void FireOnConnectListeners();

    /**
     * Same as IsConnected() but assumes lock is held by caller.
     */
    bool IsConnectedLocked();

private:
    std::string switchName;
    vconn *ofConn;
    ofp_version ofProtoVersion;
    bool isDisconnecting;

    boost::thread *connThread;
    boost::mutex connMtx;

    typedef std::list<MessageHandler *>     HandlerList;
    typedef boost::unordered_map<ofptype, HandlerList> HandlerMap;
    HandlerMap msgHandlers;

    typedef std::list<OnConnectListener *>  OnConnectList;
    OnConnectList onConnectListeners;

    int pollEventFd;

private:
    /**
     * @brief Handle ECHO requests from the switch by replying immediately.
     * Needed to keep the connection to switch alive.
     */
    class EchoRequestHandler : public MessageHandler {
        void Handle(SwitchConnection *swConn, ofptype type, ofpbuf *msg);
    };

    EchoRequestHandler echoReqHandler;
};

}   // namespace enforcer
}   // namespace opflex


#endif // _SWITCHCONNECTION_H_
