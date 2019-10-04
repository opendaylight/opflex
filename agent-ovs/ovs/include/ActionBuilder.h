/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_ACTIONBUILDER_H_
#define OPFLEXAGENT_ACTIONBUILDER_H_

#include <boost/asio/ip/address.hpp>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>

extern "C" {
#include <openvswitch/meta-flow.h>
#include <openflow/nicira-ext.h>
}

struct ofputil_flow_stats;
struct ofputil_flow_mod;
struct ofputil_bucket;
struct ofputil_packet_out;
struct ofpbuf;
struct ofpact;

namespace opflexagent {

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
     * Set the flowHasVlan bit for actions
     * @return this action builder for chaining
     */
    ActionBuilder& setFlowHasVlan(bool hasVlan = true);

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
     * Set the L4 source port number
     * @return this action builder for chaining
     */
    ActionBuilder& l4Src(uint16_t port, uint8_t proto);

    /**
     * Set the L4 destination port number
     * @return this action builder for chaining
     */
    ActionBuilder& l4Dst(uint16_t port, uint8_t proto);

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
     * Set nat action in conntrack
     */
    ActionBuilder& nat(const boost::asio::ip::address& natIp,
                       uint16_t protoMin,
                       uint16_t protoMax,
                       bool snat);

   /**
    * unnat action
    */
    ActionBuilder& unnat();

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
     * @param nestedActions nested actions to execute as part of the
     * conntrack action.  These actions are allowed to set the ct_mark
     * and ct_label fields.
     */
    ActionBuilder& conntrack(uint16_t flags = 0,
                             mf_field_id zoneSrc = static_cast<mf_field_id>(0),
                             uint16_t zoneImm = 0,
                             uint8_t recircTable = 0xff,
                             uint16_t alg = 0,
                             boost::optional<ActionBuilder&> nestedActions
                             = boost::none);

    /**
     * Algorithm for multipath action
     */
    enum MultipathAlgo {
        /** link = hash(flow) % n_links.
         *
         * Redistributes all traffic when n_links changes.  O(1)
         * performance.  See RFC 2992.
         *
         * Use UINT16_MAX for max_link to get a raw hash value. */
        NX_MP_ALG_MODULO_N = 0,

        /** link = hash(flow) / (MAX_HASH / n_links).
         *
         * Redistributes between one-quarter and one-half of traffic
         * when n_links changes.  O(1) performance.  See RFC 2992.
         */
        NX_MP_ALG_HASH_THRESHOLD = 1,

        /** Highest Random Weight.
         *
         * for i in [0,n_links):
         *   weights[i] = hash(flow, i)
         * link = { i such that weights[i] >= weights[j] for all j != i }
         *
         * Redistributes 1/n_links of traffic when n_links changes.
         * O(n_links) performance.  If n_links is greater than a
         * threshold (currently 64, but subject to change), Open
         * vSwitch will substitute another algorithm automatically.
         * See RFC 2992.
         */
        NX_MP_ALG_HRW = 2,

        /** Iterative Hash.
         *
         * i = 0
         * repeat:
         *     i = i + 1
         *     link = hash(flow, i) % arg
         * while link > max_link
         *
         * Redistributes 1/n_links of traffic when n_links changes.  O(1)
         * performance when arg/max_link is bounded by a constant.
         *
         * Redistributes all traffic when arg changes.
         *
         * arg must be greater than max_link and for best performance
         * should be no more than approximately max_link * 2.  If arg
         * is outside the acceptable range, Open vSwitch will
         * automatically substitute the least power of 2 greater than
         * max_link.
         *
         * This algorithm is specific to Open vSwitch.
         */
        NX_MP_ALG_ITER_HASH = 3
    };

    /**
     * Multipath action
     *
     * @param fields the fields to apply the hash to
     * @param basis the basis parameter for the hash
     * @param algorithm the algorithm to apply for load balancing
     * @param arg an algorithm-specific parameter
     * @param maxLink the maximum index for "output links"
     * @param dst the destination register where the selected link
     * number is stored
     */
    ActionBuilder& multipath(enum nx_hash_fields fields,
                             uint16_t basis,
                             MultipathAlgo algorithm,
                             uint16_t maxLink,
                             uint32_t arg,
                             mf_field_id dst);

    /**
     * Perform a MAC/VLAN learning action and write the result to the
     * specified table with the specified priority and cookie.
     *
     * @param prio the priority for the resulting learned flow
     * @param cookie the cookie for the resulting learned flow
     * @param table the table to which the learned flow should be
     * written.
     */
    ActionBuilder& macVlanLearn(uint16_t prio,
                                uint64_t cookie,
                                uint8_t table);
    /**
     * Fill tunnel metadata with current openflow state
     *
     * @param table_id the table in which the drop/log occured.
     */
    ActionBuilder& dropLog(uint32_t table_id);

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

} // namespace opflexagent

#endif /* OPFLEXAGENT_ACTIONBUILDER_H_ */
