/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_ACTIONBUILDER_H_
#define OVSAGENT_ACTIONBUILDER_H_

#include <boost/asio/ip/address.hpp>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>

extern "C" {
#include <openvswitch/meta-flow.h>
}

struct ofputil_flow_stats;
struct ofputil_flow_mod;
struct ofputil_bucket;
struct ofputil_packet_out;
struct ofpbuf;
struct ofpact;

namespace ovsagent {

class FlowBuilder;

/**
 * Class to help construct the actions part of a table entry incrementally.
 */
class ActionBuilder : boost::noncopyable {
public:
    /**
     * Construct an action builder with a parent flow builder
     */
    ActionBuilder(FlowBuilder& fb);
    ActionBuilder();
    ~ActionBuilder();

    /**
     * Construct and install the action structure to 'dstEntry'
     * @param dstEntry the entry to write to
     */
    void build(ofputil_flow_stats *dstEntry);

    /**
     * Construct and install the action structure to 'dstMod'
     * @param dstMod the entry to write to
     */
    void build(ofputil_flow_mod *dstMod);

    /**
     * Construct and install the action structure to 'dstPkt'
     * @param dstPkt the entry to write to
     */
    void build(ofputil_packet_out *dstPkt);

    /**
     * Construct and install the action structure to given bucket
     * @param dstBucket the entry to write to
     */
    void build(ofputil_bucket *dstBucket);

    /**
     * If this action builder has a parent flow builder, return it.
     * Asserts if this builder has no parent.
     * @return the parent
     */
    FlowBuilder& parent();

    /**
     * Load the given byte into the given register
     * @param regId the register to load
     * @param regValue the value to load
     * @return this action builder for chaining
     */
    ActionBuilder& reg8(mf_field_id regId, uint8_t regValue);

    /**
     * Load the given two bytes into the given register
     * @param regId the register to load
     * @param regValue the value to load
     * @return this action builder for chaining
     */
    ActionBuilder& reg16(mf_field_id regId, uint16_t regValue);

    /**
     * Load the given four bytes into the given register
     * @param regId the register to load
     * @param regValue the value to load
     * @return this action builder for chaining
     */
    ActionBuilder& reg(mf_field_id regId, uint32_t regValue);

    /**
     * Load the given eight bytes into the given register
     * @param regId the register to load
     * @param regValue the value to load
     * @return this action builder for chaining
     */
    ActionBuilder& reg64(mf_field_id regId, uint64_t regValue);

    /**
     * Load the given 6 bytes into the given register
     * @param regId the register to load
     * @param macValue a pointer to an array of 6 bytes to load
     * @return this action builder for chaining
     */
    ActionBuilder& reg(mf_field_id regId, const uint8_t *macValue);

    /**
     * Copy the given source register into the given destination
     * register
     * @param srcRegId the source register
     * @param dstRegId the destination register
     * @return this action builder for chaining
     */
    ActionBuilder& regMove(mf_field_id srcRegId, mf_field_id dstRegId);

    /**
     * Copy the given source register into the given destination
     * register
     * @param srcRegId the source register
     * @param dstRegId the destination register
     * @param sourceOffset the offset in bits for the source
     * @param destOffset the offset in bits for the dest
     * @param nBits the number of bits to copy, or zero to copy the
     * whole field
     * @return this action builder for chaining
     */
    ActionBuilder& regMove(mf_field_id srcRegId, mf_field_id dstRegId,
                           uint8_t sourceOffset, uint8_t destOffset,
                           uint8_t nBits);

    /**
     * Write to the metadata field
     * @param metadata the metadata to write
     * @param mask the mask for the metadata
     * @return this action builder for chaining
     */
    ActionBuilder& metadata(uint64_t metadata, uint64_t mask);

    /**
     * Set the ethernet source field in the packet
     * @param srcMac the source MAC to set
     * @return this action builder for chaining
     */
    ActionBuilder& ethSrc(const uint8_t *srcMac);

    /**
     * Set the ethernet destination field in the packet
     * @param dstMac the dest MAC to set
     * @return this action builder for chaining
     */
    ActionBuilder& ethDst(const uint8_t *dstMac);

    /**
     * Set the IP source field in the packet
     * @param srcIp The source IP field to set
     * @return this action builder for chaining
     */
    ActionBuilder& ipSrc(const boost::asio::ip::address& srcIp);

    /**
     * Set the IP dest field in the packet
     * @param dstIp The dest IP field to set
     * @return this action builder for chaining
     */
    ActionBuilder& ipDst(const boost::asio::ip::address& dstIp);

    /**
     * Decrement the TTL of an IP packet
     * @return this action builder for chaining
     */
    ActionBuilder& decTtl();

    /**
     * Go to the given flow table
     * @param tableId the table ID of the flow table
     * @return this action builder for chaining
     */
    ActionBuilder& go(uint8_t tableId);

    /**
     * Resubmit to the given port and table
     *
     * @param inPort the input port for the resubmitted packet
     * @param tableId the table to submit to
     * @return this action builder for chaining
     */
    ActionBuilder& resubmit(uint32_t inPort, uint8_t tableId);

    /**
     * Output the packet to the given port
     * @param port the openflow port ID of the port
     * @return this action builder for chaining
     */
    ActionBuilder& output(uint32_t port);

    /**
     * Output the packet to the port contained in the given register
     * @param srcRegId the register containing the openflow port to
     * output to
     * @return this action builder for chaining
     */
    ActionBuilder& outputReg(mf_field_id srcRegId);

    /**
     * Output the packet to the given group table
     * @param groupId the group table
     * @return this action builder for chaining
     */
    ActionBuilder& group(uint32_t groupId);

    /**
     * Output the packet in a packet-out message to the controller
     * @param max_len the number of bytes of the packet to include
     * @return this action builder for chaining
     */
    ActionBuilder& controller(uint16_t max_len = 0xffff);

    /**
     * Push a VLAN tag onto the packet
     * @return this action builder for chaining
     */
    ActionBuilder& pushVlan();

    /**
     * Set the VLAN tag
     * @param vlan the vlan tag to set
     * @return this action builder for chaining
     */
    ActionBuilder& setVlanVid(uint16_t vlan);

    /**
     * Pop a VLAN tag from the packet
     * @return this action builder for chaining
     */
    ActionBuilder& popVlan();

    /**
     * Flas for conn track action
     */
    enum ConnTrackFlags {
        /**
         * The connection entry is moved from the unconfirmed to
         * confirmed list in the tracker.
         */
        CT_COMMIT = 1 << 0
    };

    /**
     * Set conntrack action
     *
     * @param flags conntrack flags for conntrack action
     * @param zoneSrc if zoneImm not set, use zone from the specified field
     * @param zoneImm if set, connection tracking zone to use
     * @param recircTable the table to recirculate the packet to, or
     * 0xff to not recirculate
     * @param alg the algorithm ID for connection tracking helpers
     * @param nested nested actions to take for this conntrack entry
     * @return this action builder for chaining
     */
    ActionBuilder& conntrack(uint16_t flags = 0,
                             mf_field_id zoneSrc = static_cast<mf_field_id>(0),
                             uint16_t zoneImm = 0,
                             uint8_t recircTable = 0xff,
                             uint16_t alg = 0,
                             boost::optional<ActionBuilder&> nested
                             = boost::none);

    /**
     * Extract and return an array of flow actions from a buffer used
     * for constructing those actions.
     *
     * @param buf buffer to extract actions from
     * @param actsLen size, in bytes, of the extracted actions array
     * @return the actions present in the buffer
     */
    static ofpact* getActionsFromBuffer(ofpbuf *buf, size_t& actsLen);

private:
    struct ofpbuf* buf;
    bool flowHasVlan;
    boost::optional<FlowBuilder&> fb;
};

} // namespace ovsagent

#endif /* OVSAGENT_ACTIONBUILDER_H_ */
