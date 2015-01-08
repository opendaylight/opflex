/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <algorithm>
#include <boost/assert.hpp>

#include "ovs.h"
#include "ActionBuilder.h"

extern const struct mf_field mf_fields[MFF_N_IDS];

namespace ovsagent {

ActionBuilder::ActionBuilder() {
    ofpbuf_init(&buf, 64);
}

ActionBuilder::~ActionBuilder() {
    ofpbuf_uninit(&buf);
}

ofpact * ActionBuilder::GetActionsFromBuffer(ofpbuf *buf, size_t& actsLen) {
    ofpact_pad(buf);
    actsLen = ofpbuf_size(buf);
    return (ofpact*)ofpbuf_steal_data(buf);
}

void
ActionBuilder::Build(ofputil_flow_stats *dstEntry) {
    dstEntry->ofpacts = GetActionsFromBuffer(&buf, dstEntry->ofpacts_len);
}

void
ActionBuilder::Build(ofputil_flow_mod *dstMod) {
    dstMod->ofpacts = GetActionsFromBuffer(&buf, dstMod->ofpacts_len);
}

void
ActionBuilder::Build(ofputil_packet_out *dstPkt) {
    dstPkt->ofpacts = GetActionsFromBuffer(&buf, dstPkt->ofpacts_len);
}

void
ActionBuilder::Build(ofputil_bucket *dstBucket) {
    dstBucket->ofpacts = GetActionsFromBuffer(&buf, dstBucket->ofpacts_len);
}

static void
InitSubField(struct mf_subfield *sf, enum mf_field_id id) {
    const struct mf_field *field = &mf_fields[(int)id];
    sf->field = field;
    sf->ofs = 0;                   /* start position */
    sf->n_bits = field->n_bits;    /* number of bits */
}

void
ActionBuilder::SetRegLoad8(mf_field_id regId, uint8_t regValue) {
    struct ofpact_set_field *load = ofpact_put_reg_load(&buf);
    load->field = &mf_fields[(int)regId];
    load->value.u8 = regValue;
    load->mask.u8 = 0xff;
}

void
ActionBuilder::SetRegLoad16(mf_field_id regId, uint16_t regValue) {
    struct ofpact_set_field *load = ofpact_put_reg_load(&buf);
    load->field = &mf_fields[(int)regId];
    load->value.be16 = htons(regValue);
    load->mask.be16 = 0xffff;
}

void
ActionBuilder::SetRegLoad(mf_field_id regId, uint32_t regValue) {
    struct ofpact_set_field *load = ofpact_put_reg_load(&buf);
    load->field = &mf_fields[(int)regId];
    load->value.be32 = htonl(regValue);
    load->mask.be32 = 0xffffffff;
}

void
ActionBuilder::SetRegLoad(mf_field_id regId, const uint8_t *macValue) {
    struct ofpact_set_field *load = ofpact_put_reg_load(&buf);
    load->field = &mf_fields[(int)regId];
    memcpy(&(load->value.mac), macValue, ETH_ADDR_LEN);
    memset(&(load->mask.mac), 0xff, ETH_ADDR_LEN);
}

void
ActionBuilder::SetRegMove(mf_field_id srcRegId, mf_field_id dstRegId) {
    struct ofpact_reg_move *move = ofpact_put_REG_MOVE(&buf);
    InitSubField(&move->src, srcRegId);
    InitSubField(&move->dst, dstRegId);

    int bitsToMove = std::min(move->src.n_bits, move->dst.n_bits);
    move->src.n_bits = bitsToMove;
    move->dst.n_bits  = bitsToMove;
}

void
ActionBuilder::SetEthSrcDst(const uint8_t *srcMac, const uint8_t *dstMac) {
    if (srcMac) {
        struct ofpact_set_field *sf = ofpact_put_SET_FIELD(&buf);
        sf->field = &mf_fields[MFF_ETH_SRC];
        memcpy(&(sf->value.mac), srcMac, ETH_ADDR_LEN);
        memset(&(sf->mask.mac), 0xff, ETH_ADDR_LEN);
    }
    if (dstMac) {
        struct ofpact_set_field *sf = ofpact_put_SET_FIELD(&buf);
        sf->field = &mf_fields[MFF_ETH_DST];
        memcpy(&(sf->value.mac), dstMac, ETH_ADDR_LEN);
        memset(&(sf->mask.mac), 0xff, ETH_ADDR_LEN);
    }
}

void
ActionBuilder::SetDecNwTtl() {
    struct ofpact_cnt_ids *ctlr = ofpact_put_DEC_TTL(&buf);
    uint16_t ctlrId = 0;
    ofpbuf_put(&buf, &ctlrId, sizeof(ctlrId));
    ctlr = (ofpact_cnt_ids*)buf.frame;      // needed because of put() above
    ctlr->n_controllers = 1;
    ofpact_update_len(&buf, &ctlr->ofpact);
}

void
ActionBuilder::SetGotoTable(uint8_t tableId) {
    struct ofpact_goto_table *goTab = ofpact_put_GOTO_TABLE(&buf);
    goTab->table_id = tableId;
}

void
ActionBuilder::SetOutputToPort(uint32_t port) {
    struct ofpact_output *output = ofpact_put_OUTPUT(&buf);
    output->port = port;
}

void
ActionBuilder::SetOutputReg(mf_field_id srcRegId) {
    struct ofpact_output_reg *outputReg = ofpact_put_OUTPUT_REG(&buf);
    assert(outputReg->ofpact.raw == (uint8_t)(-1));
    InitSubField(&outputReg->src, srcRegId);
}

void
ActionBuilder::SetGroup(uint32_t groupId) {
    ofpact_group *group = ofpact_put_GROUP(&buf);
    group->group_id = groupId;
}

void
ActionBuilder::SetController(uint16_t max_len) {
    struct ofpact_controller *contr = ofpact_put_CONTROLLER(&buf);
    contr->max_len = max_len;
    contr->controller_id = 0;
    contr->reason = OFPR_ACTION;
}

void
ActionBuilder::SetPushVlan() {
    ofpact_put_PUSH_VLAN(&buf);
}

} // namespace ovsagent



