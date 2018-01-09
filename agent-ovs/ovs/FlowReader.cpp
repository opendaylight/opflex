/*
 * Implementation of FlowReader class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/logging.h>
#include "FlowReader.h"
#include "ActionBuilder.h"
#include "ovs-shim.h"
#include "ovs-ofputil.h"

#include <openvswitch/list.h>
#include <lib/util.h>
extern "C" {
#include <openvswitch/ofp-msgs.h>
}

typedef std::lock_guard<std::mutex> mutex_guard;

namespace opflexagent {

FlowReader::FlowReader() : swConn(NULL) {
}

FlowReader::~FlowReader() {
}

void FlowReader::installListenersForConnection(SwitchConnection *conn) {
    swConn = conn;
    conn->RegisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    conn->RegisterMessageHandler(OFPTYPE_GROUP_DESC_STATS_REPLY, this);
}

void FlowReader::uninstallListenersForConnection(SwitchConnection *conn) {
    conn->UnregisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    conn->RegisterMessageHandler(OFPTYPE_GROUP_DESC_STATS_REPLY, this);
}

void FlowReader::clear() {
    mutex_guard lock(reqMtx);
    flowRequests.clear();
    groupRequests.clear();
}

bool FlowReader::getFlows(uint8_t tableId, const FlowCb& cb) {
    return getFlows(tableId, NULL, cb);
}

bool FlowReader::getFlows(uint8_t tableId, match* m, const FlowCb& cb) {
    ofpbuf *req = createFlowRequest(tableId, m);
    return sendRequest<FlowCb, FlowCbMap>(req, cb, flowRequests);
}

bool FlowReader::getGroups(const GroupCb& cb) {
    ofpbuf *req = createGroupRequest();
    return sendRequest<GroupCb, GroupCbMap>(req, cb, groupRequests);
}

ofpbuf *FlowReader::createFlowRequest(uint8_t tableId, match* m) {
    ofp_version ofVer = (ofp_version)swConn->GetProtocolVersion();
    ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVer);

    ofputil_flow_stats_request fsr;
    fsr.aggregate = false;
    if (m) {
        memcpy(&fsr.match, m, sizeof(fsr.match));
    } else {
        match_init_catchall(&fsr.match);
    }
    fsr.table_id = tableId;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;

    ofpbuf *req = ofputil_encode_flow_stats_request(&fsr, proto);
    return req;
}

ofpbuf *FlowReader::createGroupRequest() {
    return ofputil_encode_group_desc_request
        ((ofp_version)swConn->GetProtocolVersion(), OFPG_ALL);
}

// XXX TODO need a way to time out requests
template <typename U, typename V>
bool FlowReader::sendRequest(ofpbuf *req, const U& cb, V& reqMap) {
    ovs_be32 reqXid = ((ofp_header *)req->data)->xid;
    LOG(DEBUG) << "Sending flow/group read request xid=" << reqXid;

    {
        mutex_guard lock(reqMtx);
        reqMap[reqXid] = cb;
    }
    int err = swConn->SendMessage(req);
    if (err != 0) {
        LOG(ERROR) << "Failed to send flow/group read request: "
            << ovs_strerror(err);
        mutex_guard lock(reqMtx);
        reqMap.erase(reqXid);
    }
    return (err == 0);
}

void FlowReader::Handle(SwitchConnection*, int msgType, ofpbuf *msg) {
    if (msgType == OFPTYPE_FLOW_STATS_REPLY) {
        handleReply<FlowEntryList, FlowCb, FlowCbMap>(msg, flowRequests);
    } else if (msgType == OFPTYPE_GROUP_DESC_STATS_REPLY) {
        handleReply<GroupEdit::EntryList, GroupCb, GroupCbMap>(msg,
                                                               groupRequests);
    }
}

template<typename T, typename U, typename V>
void FlowReader::handleReply(ofpbuf *msg, V& reqMap) {
    ofp_header *msgHdr = (ofp_header *)msg->data;
    ovs_be32 recvXid = msgHdr->xid;

    U cb;
    {
        mutex_guard lock(reqMtx);
        typename V::iterator itr = reqMap.find(recvXid);
        if (itr == reqMap.end()) {
            return;
        }
        cb = itr->second;
    }

    T recv;
    bool replyDone = false;

    decodeReply<T>(msg, recv, replyDone);

    if (replyDone) {
        mutex_guard lock(reqMtx);
        reqMap.erase(recvXid);
    }
    cb(recv, replyDone);
}

template<>
void FlowReader::decodeReply(ofpbuf *msg, FlowEntryList& recvFlows,
        bool& replyDone) {
    do {
        FlowEntryPtr entry(new FlowEntry());

        ofpbuf actsBuf;
        ofpbuf_init(&actsBuf, 32);
        int ret = ofputil_decode_flow_stats_reply(entry->entry, msg, false,
                &actsBuf);
        entry->entry->ofpacts = ActionBuilder::getActionsFromBuffer(&actsBuf,
                entry->entry->ofpacts_len);
        ofpbuf_uninit(&actsBuf);

        /* HACK: override the "raw" field so that our comparisons work
         * properly XXX TODO See if ActionBuilder can construct
         * actions with proper "raw" type
         */
        override_raw_actions(entry->entry->ofpacts, entry->entry->ofpacts_len);

        if (ret != 0) {
            if (ret == EOF) {
                replyDone = !ofpmp_more((ofp_header*)msg->header);
            } else {
                LOG(ERROR) << "Failed to decode flow stats reply: "
                    << ovs_strerror(ret);
                replyDone = true;
            }
            break;
        }
        recvFlows.push_back(entry);
        LOG(DEBUG) << "Got flow: " << *entry;
    } while (true);
}

template<>
void FlowReader::decodeReply(ofpbuf *msg, GroupEdit::EntryList& recv,
        bool& replyDone) {
    ofp_version ver = (ofp_version)((ofp_header *)msg->data)->version;
    while (true) {
        ofputil_group_desc gd;
        int ret = ofputil_decode_group_desc_reply(&gd, msg, ver);
        if (ret != 0) {
            if (ret == EOF) {
                replyDone = !ofpmp_more((ofp_header*)msg->header);
            } else {
                LOG(ERROR) << "Failed to decode group desc reply: "
                    << ovs_strerror(ret);
                replyDone = true;
            }
            break;
        }

        GroupEdit::Entry entry(new GroupEdit::GroupMod());
        entry->mod->group_id = gd.group_id;
        entry->mod->type = gd.type;
        ovs_list_move(&entry->mod->buckets, &gd.buckets);
        recv.push_back(entry);

        LOG(DEBUG) << "Got group: " << entry;
     }
}

}   // namespace opflexagent

