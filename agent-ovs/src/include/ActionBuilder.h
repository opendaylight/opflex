/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_ACTIONBUILDER_H_
#define OVSAGENT_ACTIONBUILDER_H_

#include "ovs.h"

struct ofputil_flow_stats;

namespace ovsagent {

/**
 * Class to help construct the actions part of a table entry incrementally.
 */
class ActionBuilder {
public:
    ActionBuilder();
    ~ActionBuilder();

    /**
     * Construct and install the action structure to 'dstEntry'
     */
    void Build(ofputil_flow_stats *dstEntry);

    /**
     * Construct and install the action structure to 'dstMod'
     */
    void Build(ofputil_flow_mod *dstMod);

    /**
     * Construct and install the action structure to 'dstPkt'
     */
    void Build(ofputil_packet_out *dstPkt);

    /**
     * Construct and install the action structure to given bucket
     */
    void Build(ofputil_bucket *dstBucket);

    void SetRegLoad8(mf_field_id regId, uint8_t regValue);
    void SetRegLoad16(mf_field_id regId, uint16_t regValue);
    void SetRegLoad(mf_field_id regId, uint32_t regValue);
    void SetRegLoad(mf_field_id regId, const uint8_t *macValue);
    void SetRegMove(mf_field_id srcRegId, mf_field_id dstRegId);
    void SetEthSrcDst(const uint8_t *srcMac, const uint8_t *dstMac);
    void SetDecNwTtl();
    void SetGotoTable(uint8_t tableId);
    void SetOutputToPort(uint32_t port);
    void SetOutputReg(mf_field_id srcRegId);
    void SetGroup(uint32_t groupId);
    void SetController(uint16_t max_len = 128);
    void SetPushVlan();
    void SetConntrack(uint16_t zone, uint16_t flags);

    /**
     * Extract and return an array of flow actions from a buffer used
     * for constructing those actions.
     *
     * @param buf buffer to extract actions from
     * @param actsLen size, in bytes, of the extracted actions array
     * @return the actions present in the buffer
     */
    static ofpact * GetActionsFromBuffer(ofpbuf *buf, size_t& actsLen);
private:
    struct ofpbuf buf;
};

} // namespace ovsagent

#endif /* OVSAGENT_ACTIONBUILDER_H_ */
