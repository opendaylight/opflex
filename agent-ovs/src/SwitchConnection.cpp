/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sys/eventfd.h>
#include <string>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

#include "ovs.h"
#include "SwitchConnection.h"
#include "logging.h"

using namespace std;
using namespace boost;
using namespace ovsagent;

typedef lock_guard<mutex> mutex_guard;

const int LOST_CONN_BACKOFF_SEC = 5;

namespace opflex {
namespace enforcer {

SwitchConnection::SwitchConnection(const std::string& swName) :
        switchName(swName) {
    ofConn = NULL;
    connThread = NULL;
    ofProtoVersion = OFP10_VERSION;
    isDisconnecting = false;

    pollEventFd = eventfd(0, 0);

    RegisterMessageHandler(OFPTYPE_ECHO_REQUEST, &echoReqHandler);
}

SwitchConnection::~SwitchConnection() {
    Disconnect();
}

void
SwitchConnection::RegisterOnConnectListener(OnConnectListener *l) {
    if (l) {
        mutex_guard lock(connMtx);
        onConnectListeners.push_back(l);
    }
}

void
SwitchConnection::UnregisterOnConnectListener(OnConnectListener *l) {
    mutex_guard lock(connMtx);
    onConnectListeners.remove(l);
}

void
SwitchConnection::RegisterMessageHandler(ofptype msgType,
        MessageHandler *handler)
{
    if (handler) {
        mutex_guard lock(connMtx);
        msgHandlers[msgType].push_back(handler);
    }
}

void
SwitchConnection::UnregisterMessageHandler(ofptype msgType,
        MessageHandler *handler)
{
    mutex_guard lock(connMtx);
    HandlerMap::iterator itr = msgHandlers.find(msgType);
    if (itr != msgHandlers.end()) {
        itr->second.remove(handler);
    }
}

int
SwitchConnection::Connect(ofp_version protoVer) {
    if (ofConn != NULL) {    // connection already created
        return true;
    }

    ofProtoVersion = protoVer;
    int err = DoConnect();
    if (err != 0) {
        LOG(ERROR) << "Failed to connect to " << switchName << ": "
                << ovs_strerror(err);
        return err;
    }

    connThread = new boost::thread(boost::ref(*this));
    FireOnConnectListeners();
    return true;
}

int
SwitchConnection::DoConnect() {
    string swPath;
    swPath.append("unix:").append(ovs_rundir()).append("/")
            .append(switchName).append(".mgmt");

    uint32_t versionBitmap = 1u << ofProtoVersion;
    vconn *newConn;
    int error;
    error = vconn_open_block(swPath.c_str(), versionBitmap, DSCP_DEFAULT,
            &newConn);
    if (error) {
        return error;
    }

    /* Verify we have the correct protocol version */
    ofp_version connVersion = (ofp_version)vconn_get_version(newConn);
    if (connVersion != ofProtoVersion) {
        LOG(WARNING) << "Remote supports version " << connVersion <<
                ", wanted " << ofProtoVersion;
    }
    LOG(INFO) << "Connected to switch " << swPath
            << " using protocol version " << ofProtoVersion;
    {
        mutex_guard lock(connMtx);
        ofConn = newConn;
        ofProtoVersion = connVersion;
    }
}

void
SwitchConnection::Disconnect() {
    isDisconnecting = true;
    if (connThread && SignalPollEvent()) {
        connThread->join();
        delete connThread;
        connThread = NULL;
    }

    mutex_guard lock(connMtx);
    if (ofConn != NULL) {
        vconn_close(ofConn);
        ofConn = NULL;
    }
    isDisconnecting = false;
}

bool
SwitchConnection::IsConnected() {
    mutex_guard lock(connMtx);
    return IsConnectedLocked();
}

bool
SwitchConnection::IsConnectedLocked() {
    return ofConn != NULL && vconn_get_status(ofConn) == 0;
}

ofp_version
SwitchConnection::GetProtocolVersion() {
    return ofProtoVersion;
}

void
SwitchConnection::operator()() {
    Monitor();
}

bool
SwitchConnection::SignalPollEvent() {
    uint64_t data = 1;
    ssize_t szWrote = write(pollEventFd, &data, sizeof(data));
    if (szWrote != sizeof(data)) {
        LOG(ERROR) << "Failed to send event to poll loop: " << strerror(errno);
        return false;
    }
    return true;
}

void
SwitchConnection::WatchPollEvent() {
    poll_fd_wait(pollEventFd, POLLIN);
}

void
SwitchConnection::Monitor() {
    LOG(DEBUG) << "Connection monitor started ...";

    WatchPollEvent();
    bool connLost = false;
    while (true) {
        if (connLost) {
            LOG(ERROR) << "Connection lost, trying to auto reconnect";
            mutex_guard lock(connMtx);
            if (ofConn != NULL) {
                vconn_close(ofConn);
                ofConn = NULL;
            }
        }
        bool oldConnLost = connLost;
        while (connLost && !isDisconnecting) {
            sleep(LOST_CONN_BACKOFF_SEC);
            connLost = (DoConnect() != 0);
        }
        if (!connLost) {
            mutex_guard lock(connMtx);
            vconn_run_wait(ofConn);
        }
        if (isDisconnecting) {
            return;
        }
        if (oldConnLost != connLost) {
            FireOnConnectListeners();
        }
        connLost = (EOF == ReceiveMessage());
        WatchPollEvent();
    }
    return;
}

int
SwitchConnection::ReceiveMessage() {
    do {
        int err;
        ofpbuf *recvMsg;
        {
            mutex_guard lock(connMtx);
            err = vconn_recv(ofConn, &recvMsg);
        }
        if (err == EAGAIN) {
            return 0;
        } else if (err != 0) {
            LOG(ERROR) << "Error while receiving message: "
                    << ovs_strerror(err);
            ofpbuf_delete(recvMsg);
            return err;
        } else {
            ofptype type;
            if (!ofptype_decode(&type, (ofp_header *)ofpbuf_data(recvMsg))) {
                HandlerMap::const_iterator itr = msgHandlers.find(type);
                if (itr != msgHandlers.end()) {
                    BOOST_FOREACH(MessageHandler *h, itr->second) {
                        h->Handle(this, type, recvMsg);
                    }
                }
            }
            ofpbuf_delete(recvMsg);
        }
    } while (true);
    return 0;
}

int
SwitchConnection::SendMessage(ofpbuf *msg) {
    BOOST_SCOPE_EXIT((&msg)) {
        if (!msg) {
            ofpbuf_delete(msg);
        }
    } BOOST_SCOPE_EXIT_END
    while(true) {
        mutex_guard lock(connMtx);
        if (!IsConnectedLocked()) {
            return ENOTCONN;
        }
        int err = vconn_send(ofConn, msg);
        if (err == 0) {
            msg = NULL;
            return 0;
        } else if (err != EAGAIN) {
            LOG(ERROR) << "Error sending message: " << ovs_strerror(err);
            return err;
        } else {
            vconn_run(ofConn);
            vconn_send_wait(ofConn);
        }
    }
}

void
SwitchConnection::FireOnConnectListeners() {
    BOOST_FOREACH(OnConnectListener *l, onConnectListeners) {
        l->Connected(this);
    }
}

void
SwitchConnection::EchoRequestHandler::Handle(SwitchConnection *swConn,
        ofptype msgType, ofpbuf *msg) {
    LOG(DEBUG) << "Got ECHO request";
    const ofp_header *rq = (const ofp_header *)ofpbuf_data(msg);
    struct ofpbuf *echoReplyMsg = make_echo_reply(rq);
    swConn->SendMessage(echoReplyMsg);
}

}   // namespace enforcer
}   // namespace opflex
