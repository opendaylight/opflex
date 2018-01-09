/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_SWITCHCONNECTION_H_
#define OPFLEXAGENT_SWITCHCONNECTION_H_

#include <queue>
#include <list>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <chrono>

struct vconn;
struct ofpbuf;

namespace opflexagent {

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
    virtual void Handle(SwitchConnection *swConn, int msgType,
                        struct ofpbuf *msg) = 0;
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
 * switch is lost, tries to automatically reconnect till an explicit
 * disconnect is issued. OnConnect listeners are fired each time a
 * connection is established successfully.
 */
class SwitchConnection {
public:
    /**
     * Construct a new switch connection to the given switch name
     * @param swName the name of the OVS bridge to connect to
     */
    SwitchConnection(const std::string& swName);
    virtual ~SwitchConnection();

    /**
     * Connect to the Openvswitch daemon and the specificed switch
     * and monitor the connection.
     * @param protoVer Version of OpenFlow protocol to use
     * @return 0 on success, openvswitch error code on failure
     */
    virtual int Connect(int protoVer);

    /**
     * Disconnect from daemon and switch.
     */
    virtual void Disconnect();

    /**
     * Returns true if connected to the daemon and the switch.
     */
    virtual bool IsConnected();

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
    void RegisterMessageHandler(int msgType, MessageHandler *handler);

    /**
     * Unregister a previously registered handler for OpenFlow message.
     * @param msgType OpenFlow message type to unregister for
     * @param handler Handler to unregister
     */
    void UnregisterMessageHandler(int msgType, MessageHandler *handler);

    /**
     * Send an OpenFlow message to the switch.
     * @return 0 on success, openvswitch error code on failure
     */
    virtual int SendMessage(struct ofpbuf *msg);

    /**
     * Returns the OpenFlow protocol version being used by the connection.
     */
    virtual int GetProtocolVersion();

    /**
     * Get the name of switch that this connection is for.
     */
    std::string getSwitchName();

    /** Interface: Boost thread */
    void operator()();

    /**
     * Does actual work of establishing an OpenFlow connection to the switch.
     * @return 0 on success, openvswitch error code on failure
     */
    int doConnectOF();

    /**
     * Destroys and cleans up the OpenFlow connection if any.
     */
    void cleanupOFConn();

    /**
     * Main loop for receiving messages on the connection, monitoring
     * it for liveness and automatic reconnection.
     * Runs in a thread of its own.
     */
    void Monitor();

    /**
     * Check if any OpenFlow messages were received and handle them.
     * @return 0 on success, openvswitch error code on failure
     */
    int receiveOFMessage();

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

protected:
    /**
     * Notify connect listeners of a connect event
     */
    void notifyConnectListeners();

private:
    std::string switchName;
    vconn *ofConn;
    int ofProtoVersion;

    bool isDisconnecting;

    std::unique_ptr<std::thread> connThread;
    std::mutex connMtx;

    typedef std::list<MessageHandler *>     HandlerList;
    typedef std::unordered_map<int, HandlerList> HandlerMap;
    HandlerMap msgHandlers;

    typedef std::list<OnConnectListener *>  OnConnectList;
    OnConnectList onConnectListeners;

    int pollEventFd;
    std::chrono::time_point<std::chrono::steady_clock> lastEchoTime;

private:
    /**
     * @brief Handle ECHO requests from the switch by replying immediately.
     * Needed to keep the connection to switch alive.
     */
    class EchoRequestHandler : public MessageHandler {
        void Handle(SwitchConnection *swConn, int type, struct ofpbuf *msg);
    };

    EchoRequestHandler echoReqHandler;

    /**
     * @brief Handle ECHO replies from switch
     * Needed to keep the connection to switch alive.
     */
    class EchoReplyHandler : public MessageHandler {
        void Handle(SwitchConnection *swConn, int type, struct ofpbuf *msg);
    };

    EchoReplyHandler echoRepHandler;

    /**
     * @brief Handle errors from the switch by logging.
     */
    class ErrorHandler : public MessageHandler {
        void Handle(SwitchConnection *swConn, int type, struct ofpbuf *msg);
    };

    ErrorHandler errorHandler;
};

} // namespace opflexagent


#endif // OPFLEXAGENT_SWITCHCONNECTION_H_
