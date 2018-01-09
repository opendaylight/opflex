/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <algorithm>
#include <boost/assert.hpp>

#include "ActionBuilder.h"
#include "FlowBuilder.h"
#include "ovs-shim.h"
#include "ovs-ofputil.h"

extern const struct mf_field mf_fields[MFF_N_IDS];

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace opflexagent {

ActionBuilder::ActionBuilder(FlowBuilder& fb_)
    : buf(new ofpbuf), flowHasVlan(false), fb(fb_) {
    ofpbuf_init(buf, 64);
}

ActionBuilder::ActionBuilder()
    : buf(new ofpbuf), flowHasVlan(false) {
    ofpbuf_init(buf, 64);
}

ActionBuilder::~ActionBuilder() {
    if (buf) {
        ofpbuf_uninit(buf);
        delete buf;
    }
}

ofpact * ActionBuilder::getActionsFromBuffer(ofpbuf *buf, size_t& actsLen) {
    actsLen = buf->size;
    return (ofpact*)ofpbuf_steal_data(buf);
}

void ActionBuilder::build(ofputil_flow_stats *dstEntry) {
    dstEntry->ofpacts = getActionsFromBuffer(buf, dstEntry->ofpacts_len);
}

void ActionBuilder::build(ofputil_flow_mod *dstMod) {
    dstMod->ofpacts = getActionsFromBuffer(buf, dstMod->ofpacts_len);
}

void ActionBuilder::build(ofputil_packet_out *dstPkt) {
    dstPkt->ofpacts = getActionsFromBuffer(buf, dstPkt->ofpacts_len);
}

void ActionBuilder::build(ofputil_bucket *dstBucket) {
    dstBucket->ofpacts = getActionsFromBuffer(buf, dstBucket->ofpacts_len);
}

FlowBuilder& ActionBuilder::parent() {
    return fb.get();
}

ActionBuilder& ActionBuilder::reg8(mf_field_id regId, uint8_t regValue) {
    uint8_t mask = 0xff;
    ofpact_put_reg_load(buf,
                        &mf_fields[(int)regId],
                        &regValue, &mask);
    return *this;
}

ActionBuilder& ActionBuilder::reg16(mf_field_id regId, uint16_t regValue) {
    uint16_t value = htons(regValue);
    uint16_t mask = ~((uint16_t)0);
    ofpact_put_reg_load(buf,
                        &mf_fields[(int)regId],
                        &value, &mask);
    return *this;
}

ActionBuilder& ActionBuilder::reg(mf_field_id regId, uint32_t regValue) {
    uint32_t value = htonl(regValue);
    uint32_t mask = ~((uint32_t)0);
    ofpact_put_reg_load(buf,
                        &mf_fields[(int)regId],
                        &value, &mask);
    return *this;
}

ActionBuilder& ActionBuilder::reg64(mf_field_id regId, uint64_t regValue) {
    uint64_t value = ovs_htonll(regValue);
    uint64_t mask = ~((uint64_t)0);
    ofpact_put_reg_load(buf,
                        &mf_fields[(int)regId],
                        &value, &mask);

    return *this;
}

ActionBuilder& ActionBuilder::reg(mf_field_id regId, const uint8_t *macValue) {
    uint8_t mask[6];
    memset(mask, 0xff, sizeof(mask));

    ofpact_put_reg_load(buf, &mf_fields[(int)regId],
                        macValue, mask);
    return *this;
}

ActionBuilder& ActionBuilder::regMove(mf_field_id srcRegId,
                                      mf_field_id dstRegId) {
    regMove(srcRegId, dstRegId, 0, 0, 0);
    return *this;
}

ActionBuilder& ActionBuilder::regMove(mf_field_id srcRegId,
                                      mf_field_id dstRegId,
                                      uint8_t sourceOffset,
                                      uint8_t destOffset,
                                      uint8_t nBits) {
    act_reg_move(buf, srcRegId, dstRegId, sourceOffset, destOffset, nBits);
    return *this;
}

ActionBuilder& ActionBuilder::metadata(uint64_t metadata, uint64_t mask) {
    act_metadata(buf, metadata, mask);
    return *this;
}

ActionBuilder& ActionBuilder::ethSrc(const uint8_t *srcMac) {
    act_eth_src(buf, srcMac, flowHasVlan);
    return *this;
}

ActionBuilder& ActionBuilder::ethDst(const uint8_t *dstMac) {
    act_eth_dst(buf, dstMac, flowHasVlan);
    return *this;
}

ActionBuilder& ActionBuilder::ipSrc(const address& srcIp) {
    if (srcIp.is_v4()) {
        act_ip_src_v4(buf, htonl(srcIp.to_v4().to_ulong()));
    } else {
        act_ip_src_v6(buf, srcIp.to_v6().to_bytes().data());
    }
    return *this;
}

ActionBuilder& ActionBuilder::ipDst(const address& dstIp) {
    if (dstIp.is_v4()) {
        act_ip_dst_v4(buf, htonl(dstIp.to_v4().to_ulong()));
    } else {
        act_ip_dst_v6(buf, dstIp.to_v6().to_bytes().data());
    }
    return *this;
}

ActionBuilder& ActionBuilder::l4Src(uint16_t port, uint8_t proto) {
    act_l4_src(buf, port, proto);
    return *this;
}

ActionBuilder& ActionBuilder::l4Dst(uint16_t port, uint8_t proto) {
    act_l4_dst(buf, port, proto);
    return *this;
}

ActionBuilder& ActionBuilder::decTtl() {
    act_decttl(buf);
    return *this;
}

ActionBuilder& ActionBuilder::go(uint8_t tableId) {
    act_go(buf, tableId);
    return *this;
}

ActionBuilder& ActionBuilder::resubmit(uint32_t inPort, uint8_t tableId) {
    act_resubmit(buf, inPort, tableId);
    return *this;
}

ActionBuilder& ActionBuilder::output(uint32_t port) {
    act_output(buf, port);
    return *this;
}

ActionBuilder& ActionBuilder::outputReg(mf_field_id srcRegId) {
    act_output_reg(buf, srcRegId);
    return *this;
}

ActionBuilder& ActionBuilder::group(uint32_t groupId) {
    act_group(buf, groupId);
    return *this;
}

ActionBuilder& ActionBuilder::controller(uint16_t max_len) {
    act_controller(buf, max_len);
    return *this;
}

ActionBuilder& ActionBuilder::pushVlan() {
    act_push_vlan(buf);
    flowHasVlan = true;
    return *this;
}

ActionBuilder& ActionBuilder::setVlanVid(uint16_t vlan) {
    act_set_vlan_vid(buf, vlan);
    return *this;
}

ActionBuilder& ActionBuilder::popVlan() {
    act_pop_vlan(buf);
    return *this;
}

ActionBuilder&
ActionBuilder::conntrack(uint16_t flags,
                         mf_field_id zoneSrc,
                         uint16_t zoneImm,
                         uint8_t recircTable,
                         uint16_t alg,
                         boost::optional<ActionBuilder&> nestedActions) {
    struct ofpact* actions = NULL;
    size_t actsLen = 0;
    if (nestedActions) {
        struct ofpbuf* b = nestedActions.get().buf;
        actions = static_cast<struct ofpact*>(b->data);
        actsLen = b->size;
    }
    act_conntrack(buf, flags, zoneImm, zoneSrc, recircTable, alg,
                  actions, actsLen);
    return *this;
}

ActionBuilder& ActionBuilder::multipath(enum nx_hash_fields fields,
                                        uint16_t basis,
                                        MultipathAlgo algorithm,
                                        uint16_t maxLink,
                                        uint32_t arg,
                                        mf_field_id dst) {
    act_multipath(buf, fields, basis, algorithm, maxLink, arg, dst);
    return *this;
}

} // namespace opflexagent



