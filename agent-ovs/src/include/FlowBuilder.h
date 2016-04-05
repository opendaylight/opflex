/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OVSAGENT_FLOWBUILDER_H_
#define OVSAGENT_FLOWBUILDER_H_

#include "TableState.h"
#include "ActionBuilder.h"
#include "ovs.h"

#include <boost/asio/ip/address.hpp>
#include <boost/scoped_ptr.hpp>

namespace ovsagent {

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

private:
    boost::scoped_ptr<ActionBuilder> action_;
    FlowEntryPtr entry_;
    uint16_t ethType_;

    struct match* match() { return &entry_->entry->match; }
};

} // namespace ovsagent

#endif /* OVSAGENT_FLOWBUILDER_H_ */
