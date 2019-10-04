/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_FLOWBUILDER_H_
#define OPFLEXAGENT_FLOWBUILDER_H_

#include "TableState.h"
#include "ActionBuilder.h"

#include <opflex/modb/MAC.h>

#include <boost/asio/ip/address.hpp>

namespace opflexagent {

/**
 * Build a flow entry
 */
class FlowBuilder {
public:
    FlowBuilder();
    ~FlowBuilder();

    /**
     * Build the flow entry
     */
    FlowEntryPtr build();

    /**
     * Build the entry and add it to a flow entry list
     * @param list the list to add to
     */
    void build(FlowEntryList& list);

    /**
     * Get the action builder to set up the action
     */
    ActionBuilder& action();

    /**
     * Set the table ID for the flow entry
     * @param tableId the tableId to set
     * @return this flow builder for chaining
     */
    FlowBuilder& table(uint8_t tableId);

    /**
     * Set the priority for the flow entry
     * @param prio the priority to set
     * @return this flow builder for chaining
     */
    FlowBuilder& priority(uint16_t prio);

    /**
     * Set a flow cookie for the flow entry
     * @param cookie the priority to set
     * @return this flow builder for chaining
     */
    FlowBuilder& cookie(uint64_t cookie);

    /**
     * Set flag for the flow entry
     * @param flags to set
     * @return this flow builder for chaining
     */
    FlowBuilder& flags(uint32_t flags);

    /**
     * Add a match against the specified input port
     * @param port the port to match
     * @return this flow builder for chaining
     */
    FlowBuilder& inPort(uint32_t port);

    /**
     * Add a match against ethernet source
     * @param mac the mac to match
     * @param mask a mask or NULL to match exactly
     * @return this flow builder for chaining
     */
    FlowBuilder& ethSrc(const uint8_t mac[6], const uint8_t mask[6] = NULL);

    /**
     * Add a match against ethernet destination
     * @param mac the mac to match
     * @param mask a mask or NULL to match exactly
     * @return this flow builder for chaining
     */
    FlowBuilder& ethDst(const uint8_t mac[6], const uint8_t mask[6] = NULL);

    /**
     * Add a match against ethernet source
     * @param mac the mac to match
     * @param mask a mask or NULL to match exactly
     * @return this flow builder for chaining
     */
    FlowBuilder& ethSrc(const opflex::modb::MAC& mac,
                        const opflex::modb::MAC& mask =
                        opflex::modb::MAC("ff:ff:ff:ff:ff:ff"));

    /**
     * Add a match against ethernet destination
     * @param mac the mac to match
     * @param mask a mask or NULL to match exactly
     * @return this flow builder for chaining
     */
    FlowBuilder& ethDst(const opflex::modb::MAC& mac,
                        const opflex::modb::MAC& mask =
                        opflex::modb::MAC("ff:ff:ff:ff:ff:ff"));

    /**
     * Add a match against IP source.  Also sets appropriate ethertype
     * match.
     * @param ip the ip address to match
     * @param prefixLen prefix to compute a mask
     * @return this flow builder for chaining
     */
    FlowBuilder& ipSrc(const boost::asio::ip::address& ip,
                       uint8_t prefixLen = 128);

    /**
     * Add a match against IP destination.  Also sets appropriate
     * ethertype match.
     * @param ip the ip address to match
     * @param prefixLen prefix to compute a mask
     * @return this flow builder for chaining
     */
    FlowBuilder& ipDst(const boost::asio::ip::address& ip,
                       uint8_t prefixLen = 128);

    /**
     * Add a match against ARP source.  Also sets appropriate ethertype
     * match.
     * @param ip the ip address to match
     * @param prefixLen prefix to compute a mask
     * @return this flow builder for chaining
     */
    FlowBuilder& arpSrc(const boost::asio::ip::address& ip,
                        uint8_t prefixLen = 32);

    /**
     * Add a match against ARP destination.  Also sets appropriate
     * ethertype match.
     * @param ip the ip address to match
     * @param prefixLen prefix to compute a mask
     * @return this flow builder for chaining
     */
    FlowBuilder& arpDst(const boost::asio::ip::address& ip,
                        uint8_t prefixLen = 32);

    /**
     * Add a match against ND target.  Also sets appropriate
     * ethertype and proto match.
     * @param type ICMPv6 message type
     * @param ip the ip address to match
     * @param prefixLen prefix to compute a mask
     * @param code ICMPv6 message code
     * @return this flow builder for chaining
     */
    FlowBuilder& ndTarget(uint16_t type,
                          const boost::asio::ip::address& ip,
                          uint8_t prefixLen = 128,
                          uint16_t code = 0);

    /**
     * Add a match against ethernet type
     * @param ethType the ethernet type to match
     * @return this flow builder for chaining
     */
    FlowBuilder& ethType(uint16_t ethType);

    /**
     * Add a match against IPv4/IPv6 protocol number
     * @param proto the protocol number to match
     * @return this flow builder for chaining
     */
    FlowBuilder& proto(uint8_t proto);

    /**
     * Add a match against TCP/UDP source port number.  Must have
     * appropriate ethertype match and protocol number match.
     * @param port the port number to match
     * @param mask The mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& tpSrc(uint16_t port, uint16_t mask = ~0);

    /**
     * Add a match against TCP/UDP destination port number.  Must have
     * appropriate ethertype match and protocol number match.
     * @param port the port number to match
     * @param mask The mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& tpDst(uint16_t port, uint16_t mask = ~0);

    /**
     * Add a match against TCP flags.  Must have apprioate ethertype
     * match and protocol number match
     * @param tcpFlags the flags to match
     * @param mask The mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& tcpFlags(uint16_t tcpFlags, uint16_t mask = ~0);

    /**
     * Add a match against vlan ID
     * @param vlan the vlan ID to match
     * @return this flow builder for chaining
     */
    FlowBuilder& vlan(uint16_t vlan);

    /**
     * Add a match against tag control information bits
     * @param tci the tci to match
     * @param mask the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& tci(uint16_t tci, uint16_t mask);

    /**
     * Add a match against tunnel ID
     * @param tunId the tunnel ID to match
     * @return this flow builder for chaining
     */
    FlowBuilder& tunId(uint64_t tunId);

    /**
     * Add a match against the value of a register
     * @param reg the register to match against
     * @param value the value of the register to match
     * @return this flow builder for chaining
     */
    FlowBuilder& reg(uint8_t reg, uint32_t value);

    /**
     * Add a match against the value of the flow metadata
     * @param value the value of the metadata to match
     * @param mask the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& metadata(uint64_t value, uint64_t mask = ~0ll);

    /**
     * Add a match against the value of the packet mark
     * @param value the value of the mark to match
     * @param mask the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& mark(uint32_t value, uint32_t mask = ~0l);

    /**
     * Connection tracking state flags
     */
    enum CtState {
        /**
         * This is the beginning of a new connection.
         */
        CT_NEW         = 0x01,
        /**
         * This is part of an already existing connection.
         */
        CT_ESTABLISHED = 0x02,
        /**
         * This is a separate connection that is related to an
         * existing connection.
         */
        CT_RELATED     = 0x04,
        /**
         * This flow is in the reply direction, ie it did not initiate
         * the connection.
         */
        CT_REPLY       = 0x08,
        /**
         * This flow could not be associated with a connection.
         */
        CT_INVALID     = 0x10,
        /**
         * Connection tracking has occurred.
         */
        CT_TRACKED     = 0x20,
    };

    /**
     * Match against the connection tracking state for the packet
     * @param ctState the value of the connection tracking state
     * @param mask the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& conntrackState(uint32_t ctState, uint32_t mask = ~0l);

    /**
     * Match against the connection tracking mark
     * @param ctMark the value of the connection tracking mark
     * @param mask the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& ctMark(uint32_t ctMark, uint32_t mask = ~0l);

    /**
     * Match against the connection tracking label
     * @param ctLabel the value of the connection tracking mark
     * @param mask the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& ctLabel(ovs_u128 ctLabel, ovs_u128 mask);

    /**
     * Match against the connection tracking label
     * @param ip the address used to match the outer ip source
     * @param prefixLen the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& outerIpSrc(const boost::asio::ip::address& ip,
                                    uint8_t prefixLen = 32);

    /**
     * Match against the connection tracking label
     * @param ip the address used to match the outer ip dst
     * @param prefixLen the mask for the match
     * @return this flow builder for chaining
     */
    FlowBuilder& outerIpDst(const boost::asio::ip::address& ip,
                                    uint8_t prefixLen = 32);

private:
    std::unique_ptr<ActionBuilder> action_;
    FlowEntryPtr entry_;
    uint16_t ethType_;

    struct match* match();
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_FLOWBUILDER_H_ */
