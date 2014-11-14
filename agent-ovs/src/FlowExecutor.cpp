/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include "logging.h"
#include "ovs.h"
#include "FlowExecutor.h"

using namespace std;
using namespace boost;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;
using namespace ovsagent;

typedef unique_lock<mutex> mutex_guard;

namespace opflex {
namespace enforcer {

FlowExecutor::FlowExecutor() : swConn(NULL) {
}

FlowExecutor::~FlowExecutor() {
}

void
FlowExecutor::InstallListenersForConnection(SwitchConnection *conn) {
    swConn = conn;
    conn->RegisterOnConnectListener(this);
    conn->RegisterMessageHandler(OFPTYPE_ERROR, this);
    conn->RegisterMessageHandler(OFPTYPE_BARRIER_REPLY, this);
}

void
FlowExecutor::UninstallListenersForConnection(SwitchConnection *conn) {
    conn->UnregisterMessageHandler(OFPTYPE_ERROR, this);
    conn->UnregisterMessageHandler(OFPTYPE_BARRIER_REPLY, this);
}

bool
FlowExecutor::Execute(const FlowEdit& fe) {
    if (fe.edits.empty()) {
        return true;
    }
    /* create the barrier request first to setup request-map */
    ofpbuf *barrReq = ofputil_encode_barrier_request(
            swConn->GetProtocolVersion());
    ovs_be32 barrXid = ((ofp_header *)ofpbuf_data(barrReq))->xid;
    assert(barrXid != -1);

    {
        mutex_guard lock(reqMtx);
        RequestState& barrReqState = requests[barrXid];
    }

    int error = DoExecuteNoBlock(fe, barrXid);
    if (error == 0) {
        error = WaitOnBarrier(barrReq);
    } else {
        mutex_guard lock(reqMtx);
        requests.erase(barrXid);
    }
    return error != 0;
}

bool
FlowExecutor::ExecuteNoBlock(const FlowEdit& fe) {
    if (fe.edits.empty()) {
        return true;
    }
    return DoExecuteNoBlock(fe, boost::none) != 0;
}

ofpbuf *
FlowExecutor::EncodeFlowMod(const FlowEdit::Entry& edit,
        ofp_version ofVersion) {
    ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVersion);
    assert(ofputil_protocol_is_valid(proto));

    FlowEdit::TYPE mod = edit.first;
    ofputil_flow_stats& flow = *(edit.second->entry);

    ofputil_flow_mod flowMod;
    memset(&flowMod, 0, sizeof(flowMod));
    flowMod.table_id = flow.table_id;
    flowMod.priority = flow.priority;
    if (mod != FlowEdit::add) {
        flowMod.cookie = flow.cookie;
        flowMod.cookie_mask = ~htonll(0);
    }
    flowMod.new_cookie = mod == FlowEdit::mod ? OVS_BE64_MAX :
            (mod == FlowEdit::add ? flow.cookie : htonll(0));
    memcpy(&flowMod.match, &flow.match, sizeof(flow.match));
    flowMod.ofpacts_len = flow.ofpacts_len;
    flowMod.ofpacts = (ofpact*)flow.ofpacts;
    flowMod.command = mod == FlowEdit::add ? OFPFC_ADD :
            (mod == FlowEdit::mod ? OFPFC_MODIFY : OFPFC_DELETE);
    /* fill out defaults */
    flowMod.modify_cookie = false;
    flowMod.idle_timeout = OFP_FLOW_PERMANENT;
    flowMod.hard_timeout = OFP_FLOW_PERMANENT;
    flowMod.buffer_id = UINT32_MAX;
    flowMod.out_port = OFPP_NONE;
    flowMod.out_group = OFPG_ANY;
    flowMod.flags = (ofputil_flow_mod_flags)0;
    flowMod.delete_reason = OFPRR_DELETE;

    return ofputil_encode_flow_mod(&flowMod, proto);
}

int
FlowExecutor::DoExecuteNoBlock(const flow::FlowEdit& fe,
        const boost::optional<ovs_be32>& barrXid) {
    ofp_version ofVersion = swConn->GetProtocolVersion();

    BOOST_FOREACH (const FlowEdit::Entry& e, fe.edits) {
        ofpbuf *msg = EncodeFlowMod(e, ofVersion);
        ovs_be32 xid = ((ofp_header *)ofpbuf_data(msg))->xid;
        if (barrXid) {
            mutex_guard lock(reqMtx);
            requests[barrXid.get()].reqXids.insert(xid);
        }
        int error = swConn->SendMessage(msg);
        if (error) {
            LOG(ERROR) << "Error sending flow mod message: "
                    << ovs_strerror(error);
            return error;
        }
    }
    return 0;
}

int
FlowExecutor::WaitOnBarrier(ofpbuf *barrReq) {
    ovs_be32 barrXid = ((ofp_header *)ofpbuf_data(barrReq))->xid;
    int err = swConn->SendMessage(barrReq);
    if (err) {
        LOG(ERROR) << "Error sending barrier request: "
                << ovs_strerror(err);
        mutex_guard lock(reqMtx);
        requests.erase(barrXid);
        return err;
    }

    mutex_guard lock(reqMtx);
    RequestState& barrReqState = requests[barrXid];
    while (barrReqState.done == false) {
        reqCondVar.wait(lock);
    }
    int reqStatus = barrReqState.status;
    requests.erase(barrXid);

    return reqStatus;
}

void
FlowExecutor::Handle(SwitchConnection *conn, ofptype msgType, ofpbuf *msg) {
    ofp_header *msgHdr = (ofp_header *)ofpbuf_data(msg);
    ovs_be32 recvXid = msgHdr->xid;

    mutex_guard lock(reqMtx);

    switch (msgType) {
    case OFPTYPE_ERROR:
        BOOST_FOREACH (RequestMap::value_type& kv, requests) {
            RequestState& req = kv.second;
            if (req.reqXids.find(recvXid) != req.reqXids.end()) {
                ofperr err = ofperr_decode_msg(msgHdr, NULL);
                LOG(INFO) << "Request " << recvXid << " failed: "
                        << ofperr_get_name(err) << " - "
                        << ofperr_get_description(err);
                req.status = err;
                break;
            }
        }
        break;

    case OFPTYPE_BARRIER_REPLY:
        RequestMap::iterator itr = requests.find(recvXid);
        if (itr != requests.end()) {    // request complete
            itr->second.done = true;
            reqCondVar.notify_all();
        }
        break;
    }
}

void
FlowExecutor::Connected(SwitchConnection *swConn) {
    /* If connection was re-established, fail outstanding requests */
    mutex_guard lock(reqMtx);
    BOOST_FOREACH (RequestMap::value_type& kv, requests) {
        RequestState& req = kv.second;
        req.status = ENOTCONN;
        req.done = true;
    }
    reqCondVar.notify_all();
}

}   // namespace enforcer
}   // namespace opflex
