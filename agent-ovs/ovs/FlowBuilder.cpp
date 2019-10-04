/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Network.h>

#include "FlowBuilder.h"
#include "eth.h"

#include "ovs-shim.h"
#include "ovs-ofputil.h"

namespace opflexagent {

FlowBuilder::FlowBuilder() : entry_(new FlowEntry()), ethType_(0) {

}

FlowBuilder::~FlowBuilder() {

}

ActionBuilder& FlowBuilder::action() {
    if (!action_)
        action_.reset(new ActionBuilder(*this));
    return *action_;
}

FlowEntryPtr FlowBuilder::build() {
    if (action_)
        action_->build(entry_->entry);
    return entry_;
}

void FlowBuilder::build(FlowEntryList& list) {
    list.push_back(build());
}

struct match* FlowBuilder::match() {
    return &entry_->entry->match;
}

FlowBuilder& FlowBuilder::table(uint8_t tableId) {
    entry_->entry->table_id = tableId;
    return *this;
}

FlowBuilder& FlowBuilder::priority(uint16_t prio) {
    entry_->entry->priority = prio;
    return *this;
}

FlowBuilder& FlowBuilder::cookie(uint64_t cookie) {
    entry_->entry->cookie = cookie;
    return *this;
}

FlowBuilder& FlowBuilder::flags(uint32_t flags) {
    entry_->entry->flags = (enum ofputil_flow_mod_flags)flags;
    return *this;
}

FlowBuilder& FlowBuilder::inPort(uint32_t port) {
    match_set_in_port(match(), port);
    return *this;
}

FlowBuilder& FlowBuilder::ethSrc(const uint8_t mac[6], const uint8_t mask[6]) {
    eth_addr macAddr;
    memcpy(&macAddr, mac, sizeof(eth_addr));
    if (mask) {
        eth_addr maskAddr;
        memcpy(&maskAddr, mask, sizeof(eth_addr));
        match_set_dl_src_masked(match(), macAddr, maskAddr);
    } else {
        match_set_dl_src(match(), macAddr);
    }
    return *this;
}

FlowBuilder& FlowBuilder::ethDst(const uint8_t mac[6], const uint8_t mask[6]) {
    eth_addr macAddr;
    memcpy(&macAddr, mac, sizeof(eth_addr));
    if (mask) {
        eth_addr maskAddr;
        memcpy(&maskAddr, mask, sizeof(eth_addr));
        match_set_dl_dst_masked(match(), macAddr, maskAddr);
    } else {
        match_set_dl_dst(match(), macAddr);
    }
    return *this;
}

FlowBuilder& FlowBuilder::ethSrc(const opflex::modb::MAC& mac,
                                 const opflex::modb::MAC& mask) {
    uint8_t macarr[6];
    uint8_t maskarr[6];
    mac.toUIntArray(macarr);
    mask.toUIntArray(maskarr);
    return ethSrc(macarr, maskarr);
}

FlowBuilder& FlowBuilder::ethDst(const opflex::modb::MAC& mac,
                                 const opflex::modb::MAC& mask) {
    uint8_t macarr[6];
    uint8_t maskarr[6];
    mac.toUIntArray(macarr);
    mask.toUIntArray(maskarr);
    return ethDst(macarr, maskarr);
}

static inline void fill_in6_addr(struct in6_addr& addr,
                                 const boost::asio::ip::address_v6& ip) {
    boost::asio::ip::address_v6::bytes_type bytes = ip.to_bytes();
    std::memcpy(&addr, bytes.data(), bytes.size());
}

static void addMatchSubnet(struct match* match,
                           const boost::asio::ip::address& ip,
                           uint8_t prefixLen, bool src,
                           uint16_t& ethType) {
   if (ip.is_v4()) {
       switch (ethType) {
       case 0:
           ethType = eth::type::IP;
           /* fall through */
       case eth::type::IP:
       case eth::type::ARP:
           break;
       default:
           return;
       }

       if (prefixLen > 32) prefixLen = 32;
       uint32_t mask = (prefixLen != 0)
           ? (~((uint32_t)0) << (32 - prefixLen))
           : 0;
       uint32_t addr = ip.to_v4().to_ulong() & mask;
       match_set_dl_type(match, htons(ethType));
       if (src)
           match_set_nw_src_masked(match, htonl(addr), htonl(mask));
       else
           match_set_nw_dst_masked(match, htonl(addr), htonl(mask));
   } else {
       switch (ethType) {
       case 0:
           ethType = eth::type::IPV6;
           /* fall through */
       case eth::type::IPV6:
           break;
       default:
           return;
       }

       if (prefixLen > 128) prefixLen = 128;
       struct in6_addr mask;
       struct in6_addr addr;
       network::compute_ipv6_subnet(ip.to_v6(), prefixLen, &mask, &addr);

       match_set_dl_type(match, htons(ethType));
       if (src)
           match_set_ipv6_src_masked(match, &addr, &mask);
       else
           match_set_ipv6_dst_masked(match, &addr, &mask);
   }
}

static void addMatchOuterSubnet(struct match* match,
                           const boost::asio::ip::address& ip,
                           uint8_t prefixLen, bool src,
                           uint16_t& ethType) {
   if (ip.is_v4()) {
       switch (ethType) {
       case 0:
           ethType = eth::type::IP;
           /* fall through */
       case eth::type::IP:
       case eth::type::ARP:
           break;
       default:
           return;
       }

       if (prefixLen > 32) prefixLen = 32;
       uint32_t mask = (prefixLen != 0)
           ? (~((uint32_t)0) << (32 - prefixLen))
           : 0;
       uint32_t addr = ip.to_v4().to_ulong() & mask;
       match_set_dl_type(match, htons(ethType));
       if (src)
           match_set_tun_src_masked(match, htonl(addr), htonl(mask));
       else
           match_set_tun_dst_masked(match, htonl(addr), htonl(mask));
   } else {
       switch (ethType) {
       case 0:
           ethType = eth::type::IPV6;
           /* fall through */
       case eth::type::IPV6:
           break;
       default:
           return;
       }

       if (prefixLen > 128) prefixLen = 128;
       struct in6_addr mask;
       struct in6_addr addr;
       network::compute_ipv6_subnet(ip.to_v6(), prefixLen, &mask, &addr);

       match_set_dl_type(match, htons(ethType));
       if (src)
           match_set_tun_ipv6_src_masked(match, &addr, &mask);
       else
           match_set_tun_ipv6_dst_masked(match, &addr, &mask);
   }
}

FlowBuilder& FlowBuilder::ipSrc(const boost::asio::ip::address& ip,
                                uint8_t prefixLen) {
    addMatchSubnet(match(), ip, prefixLen, true, ethType_);
    return *this;
}

FlowBuilder& FlowBuilder::ipDst(const boost::asio::ip::address& ip,
                                uint8_t prefixLen) {
    addMatchSubnet(match(), ip, prefixLen, false, ethType_);
    return *this;
}

FlowBuilder& FlowBuilder::arpSrc(const boost::asio::ip::address& ip,
                                 uint8_t prefixLen) {
    ethType(eth::type::ARP);
    addMatchSubnet(match(), ip, prefixLen, true, ethType_);
    return *this;
}

FlowBuilder& FlowBuilder::arpDst(const boost::asio::ip::address& ip,
                                 uint8_t prefixLen) {
    ethType(eth::type::ARP);
    addMatchSubnet(match(), ip, prefixLen, false, ethType_);
    return *this;
}

FlowBuilder& FlowBuilder::ndTarget(uint16_t type,
                                   const boost::asio::ip::address& ip,
                                   uint8_t prefixLen, uint16_t code) {
    ethType(eth::type::IPV6).proto(58 /* ICMPv6 */)
        .tpSrc(type)  // OVS uses tp_src for ICMPV6_TYPE
        .tpDst(code); // OVS uses tp_dst for ICMPV6_CODE

    if (ip.is_v6()) {
        struct in6_addr addr;
        struct in6_addr mask;
        fill_in6_addr(addr, ip.to_v6());
        network::get_subnet_mask_v6(prefixLen, &mask);
        match_set_nd_target_masked(match(), &addr, &mask);
    }

    return *this;
}

FlowBuilder& FlowBuilder::ethType(uint16_t ethType) {
    this->ethType_ = ethType;
    match_set_dl_type(match(), htons(ethType));
    return *this;
}

FlowBuilder& FlowBuilder::proto(uint8_t proto) {
    match_set_nw_proto(match(), proto);
    return *this;
}

FlowBuilder& FlowBuilder::tpSrc(uint16_t port, uint16_t mask) {
    match_set_tp_src_masked(match(), htons(port), htons(mask));
    return *this;
}

FlowBuilder& FlowBuilder::tpDst(uint16_t port, uint16_t mask) {
    match_set_tp_dst_masked(match(), htons(port), htons(mask));
    return *this;
}

FlowBuilder& FlowBuilder::tcpFlags(uint16_t tcpFlags, uint16_t mask) {
    match_set_tcp_flags_masked(match(), htons(tcpFlags), htons(mask));
    return *this;
}

FlowBuilder& FlowBuilder::vlan(uint16_t vlan) {
    match_set_dl_vlan(match(), htons(vlan));
    return *this;
}

FlowBuilder& FlowBuilder::tci(uint16_t tci, uint16_t mask) {
    match_set_dl_tci_masked(match(), htons(tci), htons(mask));
    return *this;
}

FlowBuilder& FlowBuilder::tunId(uint64_t tunId) {
    match_set_tun_id(match(), ovs_htonll(tunId));
    return *this;
}

FlowBuilder& FlowBuilder::reg(uint8_t reg, uint32_t value) {
    match_set_reg(match(), reg, value);
    return *this;
}

FlowBuilder& FlowBuilder::metadata(uint64_t value, uint64_t mask) {
    match_set_metadata_masked(match(), ovs_htonll(value), ovs_htonll(mask));
    return *this;
}

FlowBuilder& FlowBuilder::mark(uint32_t value, uint32_t mask) {
    match_set_pkt_mark_masked(match(), value, mask);
    return *this;
}

FlowBuilder& FlowBuilder::conntrackState(uint32_t ctState, uint32_t mask) {
    match_set_ct_state_masked(match(), ctState, mask);
    return *this;
}

FlowBuilder& FlowBuilder::ctMark(uint32_t ctMark, uint32_t mask) {
    match_set_ct_mark_masked(match(), ctMark, mask);
    return *this;
}

FlowBuilder& FlowBuilder::ctLabel(ovs_u128 ctLabel, ovs_u128 mask) {
    match_set_ct_label_masked(match(), ctLabel, mask);
    return *this;
}

FlowBuilder& FlowBuilder::outerIpSrc(const boost::asio::ip::address& ip,
                                uint8_t prefixLen) {
    addMatchOuterSubnet(match(), ip, prefixLen, true, ethType_);
    return *this;
}

FlowBuilder& FlowBuilder::outerIpDst(const boost::asio::ip::address& ip,
                                uint8_t prefixLen) {
    addMatchOuterSubnet(match(), ip, prefixLen, false, ethType_);
    return *this;
}
} // namespace opflexagent
