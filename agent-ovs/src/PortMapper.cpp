/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#include "ovs.h"
#include "PortMapper.h"
#include "logging.h"

using namespace std;
using namespace boost;
using namespace ovsagent;

typedef lock_guard<mutex> mutex_guard;

namespace opflex {
namespace enforcer {

PortMapper::PortMapper() : lastDescReqXid(-1) {
}

PortMapper::~PortMapper() {
}

void
PortMapper::InstallListenersForConnection(SwitchConnection *conn) {
    conn->RegisterOnConnectListener(this);
    conn->RegisterMessageHandler(OFPTYPE_PORT_STATUS, this);
    conn->RegisterMessageHandler(OFPTYPE_PORT_DESC_STATS_REPLY, this);
}

void
PortMapper::UninstallListenersForConnection(SwitchConnection *conn) {
    conn->UnregisterOnConnectListener(this);
    conn->UnregisterMessageHandler(OFPTYPE_PORT_STATUS, this);
    conn->UnregisterMessageHandler(OFPTYPE_PORT_DESC_STATS_REPLY, this);
}

void
PortMapper::Connected(SwitchConnection *conn) {

    struct ofpbuf *portDescReq;
    ovs_be32 reqId;

    portDescReq = ofputil_encode_port_desc_stats_request(
            conn->GetProtocolVersion(), OFPP_NONE);
    reqId = ((ofp_header *)ofpbuf_data(portDescReq))->xid;

    int err = conn->SendMessage(portDescReq);
    if (err != 0) {
        LOG(ERROR) << "Failed to send port description request: "
                << ovs_strerror(err);
    } else {
        mutex_guard lock(mapMtx);
        lastDescReqXid = reqId;
        tmpPortMap.clear();
    }
}

void
PortMapper::Handle(SwitchConnection *conn, ofptype msgType, ofpbuf *msg) {
    switch (msgType) {
    case OFPTYPE_PORT_DESC_STATS_REPLY:
        HandlePortDescReply(msg);
        break;
    case OFPTYPE_PORT_STATUS:
        HandlePortStatus(msg);
        break;
    }
}

void
PortMapper::HandlePortDescReply(ofpbuf *msg) {
    ofp_header *msgHdr = (ofp_header *)ofpbuf_data(msg);
    ovs_be32 recvXid = msgHdr->xid;
    if (recvXid != lastDescReqXid) {
        return;     // don't care
    }
    bool done = !(ofpmp_flags(msgHdr) & OFPSF_REPLY_MORE);

    ofpbuf tmpBuf;
    ofpbuf_use_const(&tmpBuf, msgHdr, ntohs(msgHdr->length));
    ofptype tmpType;
    if (ofptype_pull(&tmpType, &tmpBuf)) {
        return;     // bad header?
    }

    ofputil_phy_port portDesc;
    while (!ofputil_pull_phy_port((ofp_version)msgHdr->version,
                                  &tmpBuf, &portDesc)) {
        LOG(DEBUG) << "Found port: " << portDesc.port_no
                << " -> " << portDesc.name;
        tmpPortMap[portDesc.name] = portDesc;
    }

    if (done) {
        LOG(DEBUG) << "Got end of message";
        mutex_guard lock(mapMtx);
        lastDescReqXid = -1;
        portMap.swap(tmpPortMap);
        tmpPortMap.clear();
    }
}

void
PortMapper::HandlePortStatus(ofpbuf *msg) {
    ofp_header *msgHdr = (ofp_header *)ofpbuf_data(msg);
    ofputil_port_status portStatus;
    ofperr err = ofputil_decode_port_status(msgHdr, &portStatus);
    if (err) {
        LOG(ERROR) << "Failed to decode port status message: "
                << ofperr_get_description(err);
        return;
    }
    mutex_guard lock(mapMtx);
    if (portStatus.reason == OFPPR_ADD) {
        portMap[portStatus.desc.name] = portStatus.desc;
    } else if (portStatus.reason == OFPPR_DELETE) {
        portMap.erase(portStatus.desc.name);
    }
}

uint32_t
PortMapper::FindPort(const std::string& name) {
    mutex_guard lock(mapMtx);
    PortMap::const_iterator itr = portMap.find(name);
    return itr == portMap.end() ? OFPP_NONE : itr->second.port_no;
}

}   // namespace enforcer
}   // namespace opflex

