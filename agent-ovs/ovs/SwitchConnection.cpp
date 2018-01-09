/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "SwitchConnection.h"
#include <opflexagent/logging.h>

#include <sys/eventfd.h>
#include <string>
#include <fstream>

#include <boost/scope_exit.hpp>

#include <unordered_map>

#include "ovs-ofputil.h"

extern "C" {
#include <lib/util.h>
#include <lib/dirs.h>
#include <lib/socket-util.h>
#include <lib/stream.h>
#include <lib/poll-loop.h>
#include <openvswitch/vconn.h>
#include <openvswitch/ofp-msgs.h>
}

typedef std::lock_guard<std::mutex> mutex_guard;

const int LOST_CONN_BACKOFF_MSEC = 5000;
const std::chrono::seconds ECHO_INTERVAL(5);
const std::chrono::seconds MAX_ECHO_INTERVAL(30);

#define OVS_VSWITCH_DAEMON  "ovs-vswitchd"

namespace opflexagent {

SwitchConnection::SwitchConnection(const std::string& swName) :
    switchName(swName), ofConn(NULL) {
    connThread = NULL;
    ofProtoVersion = OFP10_VERSION;
    isDisconnecting = false;

    pollEventFd = eventfd(0, 0);

    RegisterMessageHandler(OFPTYPE_ECHO_REQUEST, &echoReqHandler);
    RegisterMessageHandler(OFPTYPE_ECHO_REPLY, &echoRepHandler);
    RegisterMessageHandler(OFPTYPE_ERROR, &errorHandler);
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
SwitchConnection::RegisterMessageHandler(int msgType, MessageHandler *handler)
{
    if (handler) {
        mutex_guard lock(connMtx);
        msgHandlers[msgType].push_back(handler);
    }
}

void
SwitchConnection::UnregisterMessageHandler(int msgType,
        MessageHandler *handler)
{
    mutex_guard lock(connMtx);
    HandlerMap::iterator itr = msgHandlers.find(msgType);
    if (itr != msgHandlers.end()) {
        itr->second.remove(handler);
    }
}

int
SwitchConnection::Connect(int protoVer) {
    if (ofConn != NULL) {    // connection already created
        return true;
    }

    ofProtoVersion = protoVer;
    int err = doConnectOF();
    if (err != 0) {
        LOG(ERROR) << "Failed to connect to " << switchName << ": "
            << ovs_strerror(err);
    }
    connThread.reset(new std::thread(std::ref(*this)));
    return err;
}

int
SwitchConnection::doConnectOF() {
    std::string swPath;
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
    int connVersion = vconn_get_version(newConn);
    if (connVersion != ofProtoVersion) {
        LOG(WARNING) << "Remote supports version " << connVersion <<
                ", wanted " << ofProtoVersion;
    }
    LOG(INFO) << "Connected to switch " << swPath
            << " using protocol version " << ofProtoVersion;
    {
        mutex_guard lock(connMtx);
        lastEchoTime = std::chrono::steady_clock::now();
        cleanupOFConn();
        ofConn = newConn;
        ofProtoVersion = connVersion;
    }
    return 0;
}

void SwitchConnection::cleanupOFConn() {
    if (ofConn != NULL) {
        vconn_close(ofConn);
        ofConn = NULL;
    }
}

void
SwitchConnection::Disconnect() {
    isDisconnecting = true;
    if (connThread && SignalPollEvent()) {
        connThread->join();
        connThread.reset();
    }

    mutex_guard lock(connMtx);
    cleanupOFConn();
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

int
SwitchConnection::GetProtocolVersion() {
    return ofProtoVersion;
}

std::string SwitchConnection::getSwitchName() {
    return switchName;
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

    bool connLost = (IsConnected() == false);
    if (!connLost) {
        FireOnConnectListeners();
    }

    while (true) {
        if (connLost) {
            LOG(ERROR) << "Connection lost, trying to auto reconnect";
            mutex_guard lock(connMtx);
            cleanupOFConn();
        }
        bool oldConnLost = connLost;
        while (connLost && !isDisconnecting) {
            WatchPollEvent();
            poll_timer_wait(LOST_CONN_BACKOFF_MSEC);
            poll_block(); // block till timer expires or disconnect is requested
            if (!isDisconnecting) {
                connLost = (doConnectOF() != 0);
            }
        }
        if (isDisconnecting) {
            return;
        }
        if (oldConnLost != connLost) {
            FireOnConnectListeners();
        }
        WatchPollEvent();
        if (!connLost) {
            {
                mutex_guard lock(connMtx);
                poll_timer_wait(LOST_CONN_BACKOFF_MSEC);

                vconn_run(ofConn);
                vconn_run_wait(ofConn);
                vconn_recv_wait(ofConn);
            }
            poll_block();
        }
        connLost = (EOF == receiveOFMessage());

        if (!connLost) {
            std::chrono::time_point<std::chrono::steady_clock> echoTime;
            {
                mutex_guard lock(connMtx);
                echoTime = lastEchoTime;
            }

            auto diff = std::chrono::steady_clock::now() - echoTime;
            if (diff >= MAX_ECHO_INTERVAL) {
                LOG(ERROR) << "Timed out reading from switch socket";
                connLost = true;
            } else if (diff >= ECHO_INTERVAL) {
                struct ofpbuf *msg =
                    make_echo_request((ofp_version)GetProtocolVersion());
                SendMessage(msg);
            }
        }
    }
    return;
}

int
SwitchConnection::receiveOFMessage() {
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
            if (!ofptype_decode(&type, (ofp_header *)recvMsg->data)) {
                HandlerMap::const_iterator itr = msgHandlers.find(type);
                if (itr != msgHandlers.end()) {
                    for (MessageHandler *h : itr->second) {
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
        if (msg) {
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
            LOG(ERROR) << "Error sending OF message: " << ovs_strerror(err);
            return err;
        } else {
            vconn_run(ofConn);
            vconn_send_wait(ofConn);
        }
    }
}

void
SwitchConnection::FireOnConnectListeners() {
    if (GetProtocolVersion() >= OFP12_VERSION) {
        // Set controller role to MASTER
        ofpbuf *b0;
        ofp12_role_request *rr;
        b0 = ofpraw_alloc(OFPRAW_OFPT12_ROLE_REQUEST,
                          GetProtocolVersion(), sizeof *rr);
        rr = (ofp12_role_request*)ofpbuf_put_zeros(b0, sizeof *rr);
        rr->role = htonl(OFPCR12_ROLE_MASTER);
        SendMessage(b0);
    }
    {
        // Set default miss length to non-zero value to enable
        // asynchronous messages
        ofpbuf *b1;
        ofp_switch_config *osc;
        b1 = ofpraw_alloc(OFPRAW_OFPT_SET_CONFIG,
                          GetProtocolVersion(), sizeof *osc);
        osc = (ofp_switch_config*)ofpbuf_put_zeros(b1, sizeof *osc);
        osc->miss_send_len = htons(OFP_DEFAULT_MISS_SEND_LEN);
        SendMessage(b1);
    }
    {
        // Set packet-in format to nicira-extended format
        ofpbuf *b2;
        nx_set_packet_in_format* pif;
        b2 = ofpraw_alloc(OFPRAW_NXT_SET_PACKET_IN_FORMAT,
                          GetProtocolVersion(), sizeof *pif);
        pif = (nx_set_packet_in_format*)ofpbuf_put_zeros(b2, sizeof *pif);
        pif->format = htonl(NXPIF_NXT_PACKET_IN);
        SendMessage(b2);
    }
    notifyConnectListeners();
}

void
SwitchConnection::notifyConnectListeners() {
    for (OnConnectListener *l : onConnectListeners) {
        l->Connected(this);
    }
}

void
SwitchConnection::EchoRequestHandler::Handle(SwitchConnection *swConn,
                                             int, ofpbuf *msg) {
    const ofp_header *rq = (const ofp_header *)msg->data;
    struct ofpbuf *echoReplyMsg = make_echo_reply(rq);
    swConn->SendMessage(echoReplyMsg);
}

void
SwitchConnection::EchoReplyHandler::Handle(SwitchConnection *swConn,
                                    int, ofpbuf *msg) {
    mutex_guard lock(swConn->connMtx);
    swConn->lastEchoTime = std::chrono::steady_clock::now();
}

void
SwitchConnection::ErrorHandler::Handle(SwitchConnection*, int,
                                       ofpbuf *msg) {
    const struct ofp_header *oh = (ofp_header *)msg->data;
    ofperr err = ofperr_decode_msg(oh, NULL);
    LOG(ERROR) << "Got error reply from switch ("
               << ntohl(oh->xid) << "): "
               << ofperr_get_name(err) << ": "
               << ofperr_get_description(err);
}

} // namespace opflexagent
