/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "PortMapper.h"
#include <opflexagent/logging.h>

#include "ovs-ofputil.h"


// OVS lib
#include <lib/util.h>
extern "C" {
#include <openvswitch/ofp-msgs.h>
}

typedef std::lock_guard<std::mutex> mutex_guard;

namespace opflexagent {

PortMapper::PortMapper() : lastDescReqXid(-1) {
}

PortMapper::~PortMapper() {
}

void PortMapper::registerPortStatusListener(PortStatusListener *l) {
    if (l) {
        mutex_guard lock(mapMtx);
        portStatusListeners.push_back(l);
    }
}

void PortMapper::unregisterPortStatusListener(PortStatusListener *l) {
    mutex_guard lock(mapMtx);
    portStatusListeners.remove(l);
}

void PortMapper::notifyListeners(const std::string& portName, uint32_t portNo,
                                 bool fromDesc) {
    for (PortStatusListener *l : portStatusListeners) {
        l->portStatusUpdate(portName, portNo, fromDesc);
    }
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
        (ofp_version)conn->GetProtocolVersion(), OFPP_NONE);
    reqId = ((ofp_header *)portDescReq->data)->xid;

    int err = conn->SendMessage(portDescReq);
    if (err != 0) {
        LOG(ERROR) << "Failed to send port description request: "
                << ovs_strerror(err);
    } else {
        mutex_guard lock(mapMtx);
        lastDescReqXid = reqId;
        tmpPortMap.clear();
        tmprPortMap.clear();
    }
}

void
PortMapper::Handle(SwitchConnection*, int msgType, ofpbuf *msg) {
    switch (msgType) {
    case OFPTYPE_PORT_DESC_STATS_REPLY:
        HandlePortDescReply(msg);
        break;
    case OFPTYPE_PORT_STATUS:
        HandlePortStatus(msg);
        break;
    default:
        LOG(ERROR) << "Unexpected message of type " << msgType;
        break;
    }
}

void
PortMapper::HandlePortDescReply(ofpbuf *msg) {
    ofp_header *msgHdr = (ofp_header *)msg->data;
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
        *tmpPortMap[portDesc.name].get() = portDesc;
        tmprPortMap[portDesc.port_no] = portDesc.name;
    }

    if (done) {
        LOG(DEBUG) << "Got end of message";
        {
            mutex_guard lock(mapMtx);
            lastDescReqXid = -1;
            portMap.swap(tmpPortMap);
            tmpPortMap.clear();
            rportMap.swap(tmprPortMap);
            tmprPortMap.clear();
        }
        for (const PortMap::value_type& kv : portMap) {
            notifyListeners(kv.first, kv.second.get()->port_no, true);
        }
    }
}

void
PortMapper::HandlePortStatus(ofpbuf *msg) {
    ofp_header *msgHdr = (ofp_header *)msg->data;
    ofputil_port_status portStatus;
    ofperr err = ofputil_decode_port_status(msgHdr, &portStatus);
    if (err) {
        LOG(ERROR) << "Failed to decode port status message: "
                << ofperr_get_description(err);
        return;
    }
    {
        mutex_guard lock(mapMtx);
        if (portStatus.reason == OFPPR_ADD ||
            portStatus.reason == OFPPR_MODIFY) {
            *portMap[portStatus.desc.name].get() = portStatus.desc;
            rportMap[portStatus.desc.port_no] = portStatus.desc.name;
        } else if (portStatus.reason == OFPPR_DELETE) {
            portMap.erase(portStatus.desc.name);
            rportMap.erase(portStatus.desc.port_no);
        }
    }
    notifyListeners(portStatus.desc.name, portStatus.desc.port_no,
                    false);
}

uint32_t
PortMapper::FindPort(const std::string& name) {
    mutex_guard lock(mapMtx);
    PortMap::const_iterator itr = portMap.find(name);
    return itr == portMap.end() ? OFPP_NONE : itr->second.get()->port_no;
}

const std::string&
PortMapper::FindPort(uint32_t of_port_no) {
    mutex_guard lock(mapMtx);
    return rportMap.at(of_port_no);
}

} // namespace opflexagent

