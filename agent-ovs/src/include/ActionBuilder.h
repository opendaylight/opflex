/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
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

    /**
     * Load the given byte into the given register
     * @param regId the register to load
     * @param regValue the value to load
     */
    void SetRegLoad8(mf_field_id regId, uint8_t regValue);
    /**
     * Load the given two bytes into the given register
     * @param regId the register to load
     * @param regValue the value to load
     */
    void SetRegLoad16(mf_field_id regId, uint16_t regValue);
    /**
     * Load the given four bytes into the given register
     * @param regId the register to load
     * @param regValue the value to load
     */
    void SetRegLoad(mf_field_id regId, uint32_t regValue);
    /**
     * Load the given 6 bytes into the given register
     * @param regId the register to load
     * @param macValue a pointer to an array of 6 bytes to load
     */
    void SetRegLoad(mf_field_id regId, const uint8_t *macValue);
    /**
     * Copy the given source register into the given destination
     * register
     * @param srcRegId the source register
     * @param dstRegId the destination register
     */
    void SetRegMove(mf_field_id srcRegId, mf_field_id dstRegId);
    /**
     * Set the ethernet source and/or destination fields in the packet
     * @param srcMac the source MAC to set, or NULL to not set a
     * source MAC
     * @param dstMac the dest MAC to set, or NULL to not set a dest
     * MAC
     */
    void SetEthSrcDst(const uint8_t *srcMac, const uint8_t *dstMac);
    /**
     * Decrement the TTL of an IP packet
     */
    void SetDecNwTtl();
    /**
     * Go to the given flow table
     * @param tableId the table ID of the flow table
     */
    void SetGotoTable(uint8_t tableId);
    /**
     * Output the packet to the given port
     * @param port the openflow port ID of the port
     */
    void SetOutputToPort(uint32_t port);
    /**
     * Output the packet to the port contained in the given register
     * @param srcRegId the register containing the openflow port to
     * output to
     */
    void SetOutputReg(mf_field_id srcRegId);
    /**
     * Output the packet to the given group table
     * @param groupId the group table
     */
    void SetGroup(uint32_t groupId);
    /**
     * Output the packet in a packet-out message to the controller
     * @param max_len the number of bytes of the packet to include
     */
    void SetController(uint16_t max_len = 128);
    /**
     * Push a VLAN tag onto the packet
     */
    void SetPushVlan();
    /**
     * Pop a VLAN tag from the packet
     */
    void SetPopVlan();
    /**
     * Use the connection tracking tables with the given zone and flags
     * @param zone the connection tracking zone to use
     * @param flags the flags to set (starting with "NX_CT_")
     */
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
