/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ovs.h"
#include "ActionBuilder.h"

extern const struct mf_field mf_fields[MFF_N_IDS];

namespace opflex {
namespace enforcer {
namespace flow {

ActionBuilder::ActionBuilder() {
    ofpbuf_init(&buf, 64);
}

ActionBuilder::~ActionBuilder() {
    ofpbuf_uninit(&buf);
}

void
ActionBuilder::Build(ofputil_flow_stats *dstEntry) {
    dstEntry->ofpacts_len = ofpbuf_size(&buf);
    dstEntry->ofpacts = (ofpact*)ofpbuf_steal_data(&buf);
}

static void
InitSubField(struct mf_subfield *sf, enum mf_field_id id) {
    const struct mf_field *field = &mf_fields[(int)id];
    sf->field = field;
    sf->ofs = 0;    /* start position */
    sf->n_bits = field->n_bits;     /* number of bits */
}

void
ActionBuilder::SetRegLoad(mf_field_id regId, uint32_t regValue) {
    struct ofpact_reg_load *load = ofpact_put_REG_LOAD(&buf);
    InitSubField(&load->dst, regId);
    load->subvalue.be32[0] = htonl(0);
    load->subvalue.be32[1] = htonl(0);
    load->subvalue.be32[2] = htonl(0);
    load->subvalue.be32[3] = htonl(regValue);
}

void
ActionBuilder::SetRegLoad(mf_field_id regId, const uint8_t *macValue) {
    struct ofpact_reg_load *load = ofpact_put_REG_LOAD(&buf);
    InitSubField(&load->dst, regId);
    memset(&load->subvalue, 0, sizeof(load->subvalue));
    memcpy(&(load->subvalue.u8[10]), macValue, ETH_ADDR_LEN);
}

void
ActionBuilder::SetRegMove(mf_field_id srcRegId, mf_field_id dstRegId) {
    struct ofpact_reg_move *move = ofpact_put_REG_MOVE(&buf);
    InitSubField(&move->src, srcRegId);
    InitSubField(&move->dst, dstRegId);
}

void
ActionBuilder::SetEthSrcDst(const uint8_t *srcMac, const uint8_t *dstMac) {
    if (srcMac) {
        struct ofpact_mac *eth = ofpact_put_SET_ETH_SRC(&buf);
        memcpy(&eth->mac, srcMac, sizeof(eth->mac));
    }
    if (dstMac) {
        struct ofpact_mac *eth = ofpact_put_SET_ETH_DST(&buf);
        memcpy(&eth->mac, dstMac, sizeof(eth->mac));
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
    InitSubField(&outputReg->src, srcRegId);
    outputReg->max_len = UINT16_MAX;
}

}   // namespace flow
}   // namespace enforcer
}   // namespace opflex



