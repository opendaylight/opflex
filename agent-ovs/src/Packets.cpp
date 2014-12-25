/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/foreach.hpp>

#include <modelgbp/gbp/AutoconfigEnumT.hpp>

#include "Packets.h"

namespace ovsagent {

using std::string;
using boost::shared_ptr;
using boost::optional;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;

void Packets::chksum_accum(uint32_t& chksum, uint16_t* addr, size_t len) {
    while (len > 1)  {
        chksum += *addr++;
        len -= 2;
    }
    if (len > 0)
        chksum += *(uint8_t*)addr;
}

uint16_t Packets::chksum_finalize(uint32_t chksum) {
    while (chksum>>16)
        chksum = (chksum & 0xffff) + (chksum >> 16);
    return ~chksum;
}

ofpbuf* Packets::compose_icmp6_router_ad(const uint8_t* srcMac,
                                         const uint8_t* dstMac,
                                         const struct in6_addr* dstIp,
                                         const opflex::modb::URI& egUri,
                                         PolicyManager& polMgr) {
    using namespace modelgbp::gbp;

    optional<shared_ptr<RoutingDomain> > rd = polMgr.getRDForGroup(egUri);
    if (!rd) return NULL;

    uint32_t mtu = 0;

    PolicyManager::subnet_vector_t subnets;
    PolicyManager::subnet_vector_t ipv6Subnets;
    polMgr.getSubnetsForGroup(egUri, subnets);
    BOOST_FOREACH(shared_ptr<Subnet>& sn, subnets) {
        optional<const string&> routerIpStr = sn->getVirtualRouterIp();
        if (!routerIpStr) continue;
        
        boost::system::error_code ec;
        address routerIp = address::from_string(routerIpStr.get(), ec);
        if (ec) continue;
        
        if (!routerIp.is_v6()) continue;

        ipv6Subnets.push_back(sn);
    }

    if (ipv6Subnets.size() == 0)
        return NULL;

    struct ofpbuf* b = NULL;
    uint16_t* chksum_addr = NULL;
    uint16_t payloadLen = sizeof(struct nd_router_advert) +
        sizeof(struct nd_opt_hdr) + 6 +
        + (mtu == 0 ? 0 : sizeof(struct nd_opt_mtu)) +
        sizeof(struct nd_opt_prefix_info) * (ipv6Subnets.size() + 1);
    size_t len = sizeof(struct eth_header) + 
        sizeof(struct ip6_hdr) +
        payloadLen;

    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);

    // prefix information
    BOOST_FOREACH(shared_ptr<Subnet>& sn, ipv6Subnets) {
        optional<const string&> routerIpStr = sn->getVirtualRouterIp();
        if (!routerIpStr) continue;
        
        boost::system::error_code ec;
        address routerIp = address::from_string(routerIpStr.get(), ec);
        if (ec) continue;

        address networkAddr = address::from_string(routerIpStr.get(), ec);
        if (ec || !networkAddr.is_v6()) networkAddr = routerIp;

        struct nd_opt_prefix_info* prefix = (struct nd_opt_prefix_info*)
            ofpbuf_push_zeros(b, sizeof(struct nd_opt_prefix_info));

        uint8_t autonomous = sn->getIpv6AdvAutonomousFlag(1);

        prefix->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
        prefix->nd_opt_pi_len = 4;
        prefix->nd_opt_pi_prefix_len = sn->getPrefixLen(64);
        prefix->nd_opt_pi_flags_reserved = ND_OPT_PI_FLAG_ONLINK;
        prefix->nd_opt_pi_valid_time = 
            htonl(sn->getIpv6AdvValidLifetime(2592000));
        prefix->nd_opt_pi_preferred_time = 
            htonl(sn->getIpv6AdvPreferredLifetime(604800));

        if (sn->getIpv6AdvAutonomousFlag(1))
            prefix->nd_opt_pi_flags_reserved |= ND_OPT_PI_FLAG_AUTO;

        address_v6::bytes_type bytes = networkAddr.to_v6().to_bytes();
        std::memcpy(&prefix->nd_opt_pi_prefix, bytes.data(), bytes.size());
    }

    // default prefix
    struct nd_opt_prefix_info* defprefix = (struct nd_opt_prefix_info*)
        ofpbuf_push_zeros(b, sizeof(struct nd_opt_prefix_info));
    defprefix->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
    defprefix->nd_opt_pi_len = 4;
    defprefix->nd_opt_pi_prefix_len = 0;
    defprefix->nd_opt_pi_flags_reserved = ND_OPT_PI_FLAG_ONLINK;
    defprefix->nd_opt_pi_valid_time = htonl(2592000);
    defprefix->nd_opt_pi_preferred_time = htonl(604800);

    // MTU option
    if (mtu) {
        struct nd_opt_mtu* opt_mtu = (struct nd_opt_mtu*)
            ofpbuf_push_zeros(b, sizeof(struct nd_opt_mtu));
        opt_mtu->nd_opt_mtu_type = ND_OPT_MTU;
        opt_mtu->nd_opt_mtu_len = 1;
        opt_mtu->nd_opt_mtu_mtu = htonl(mtu);
    }
    
    // source link-layer address option
    struct nd_opt_hdr* source_ll = (struct nd_opt_hdr*)
        ofpbuf_push_zeros(b, sizeof(struct nd_opt_hdr) + 6);
    source_ll->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
    source_ll->nd_opt_len = 1;
    memcpy(((char*)source_ll)+2, srcMac, ETH_ADDR_LEN);

    // fill in router advertisement
    struct nd_router_advert* router_ad = (struct nd_router_advert*)
        ofpbuf_push_zeros(b, sizeof(struct nd_router_advert));
    router_ad->nd_ra_hdr.icmp6_type = ND_ROUTER_ADVERT;
    router_ad->nd_ra_hdr.icmp6_code = 0;
    router_ad->nd_ra_router_lifetime = htonl(1800);
    uint8_t autoconf =
        rd->get()->getIpv6Autoconfig(AutoconfigEnumT::CONST_STATELESS);
    if (autoconf == AutoconfigEnumT::CONST_BOTH ||
        autoconf == AutoconfigEnumT::CONST_STATELESS)
        router_ad->nd_ra_flags_reserved |= ND_RA_FLAG_MANAGED;
    if (autoconf == AutoconfigEnumT::CONST_BOTH ||
        autoconf == AutoconfigEnumT::CONST_DHCP)
        router_ad->nd_ra_flags_reserved |= ND_RA_FLAG_OTHER;

    struct ip6_hdr* ip6 = (struct ip6_hdr*)
        ofpbuf_push_zeros(b, sizeof(struct ip6_hdr));
    struct eth_header* eth = (struct eth_header*)
        ofpbuf_push_zeros(b, sizeof(struct eth_header));

    // initialize ethernet header
    memcpy(eth->eth_src, srcMac, ETH_ADDR_LEN);
    memcpy(eth->eth_dst, dstMac, ETH_ADDR_LEN);
    eth->eth_type = htons(ETH_TYPE_IPV6);
    
    // Initialize IPv6 header
    ip6->ip6_vfc = 0x60;
    ip6->ip6_hlim = 255;
    ip6->ip6_nxt = 58;
    ip6->ip6_plen = htons(payloadLen);
    
    // IPv6 link-local address made from the router MAC
    ((char*)&ip6->ip6_src)[0] = 0xfe;
    ((char*)&ip6->ip6_src)[1] = 0x80;
    memcpy(((char*)&ip6->ip6_src) + 8, srcMac, 3);
    ((char*)&ip6->ip6_src)[8] ^= 0x02;
    ((char*)&ip6->ip6_src)[11] = 0xff;
    ((char*)&ip6->ip6_src)[12] = 0xfe;
    memcpy(((char*)&ip6->ip6_src) + 13, srcMac+3, 3);

    memcpy(&ip6->ip6_dst, dstIp, sizeof(struct in6_addr));
   
    // compute checksum
    uint32_t chksum = 0;
    // pseudoheader
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_src, 
                 sizeof(struct in6_addr));
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_dst, 
                 sizeof(struct in6_addr));
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_plen, 2);
    chksum += (uint16_t)htons(58);
    // payload
    chksum_accum(chksum, (uint16_t*)router_ad, payloadLen);
    router_ad->nd_ra_hdr.icmp6_cksum = chksum_finalize(chksum);

    return b;
}

ofpbuf* Packets::compose_icmp6_neigh_ad(uint32_t naFlags,
                                        const uint8_t* srcMac,
                                        const uint8_t* dstMac,
                                        const struct in6_addr* srcIp,
                                        const struct in6_addr* dstIp) {
    struct ofpbuf* b = NULL;
    uint16_t* chksum_addr = NULL;
    struct eth_header* eth = NULL;
    struct ip6_hdr* ip6 = NULL;
    uint16_t* payload;
    uint16_t payloadLen = 0;

    struct nd_neighbor_advert* neigh_ad = NULL;
    struct nd_opt_hdr* target_ll = NULL;
    
    size_t len = sizeof(struct eth_header) + 
        sizeof(struct ip6_hdr) +
        sizeof(struct nd_neighbor_advert) +
        sizeof(struct nd_opt_hdr) + 6;
    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);
    target_ll = (struct nd_opt_hdr*)
        ofpbuf_push_zeros(b, sizeof(struct nd_opt_hdr) + 6);
    neigh_ad = (struct nd_neighbor_advert*)
        ofpbuf_push_zeros(b, sizeof(struct nd_neighbor_advert));
    ip6 = (struct ip6_hdr*)
        ofpbuf_push_zeros(b, sizeof(struct ip6_hdr));
    eth = (struct eth_header*)
        ofpbuf_push_zeros(b, sizeof(struct eth_header));
    
    payload = (uint16_t*)neigh_ad;
    payloadLen = sizeof(struct nd_neighbor_advert) +
            sizeof(struct nd_opt_hdr) + 6;

    // initialize ethernet header
    memcpy(eth->eth_src, srcMac, ETH_ADDR_LEN);
    memcpy(eth->eth_dst, dstMac, ETH_ADDR_LEN);
    eth->eth_type = htons(ETH_TYPE_IPV6);
    
    // Initialize IPv6 header
    ip6->ip6_vfc = 0x60;
    ip6->ip6_hlim = 255;
    ip6->ip6_nxt = 58;
    ip6->ip6_plen = htons(payloadLen);
    memcpy(&ip6->ip6_src, srcIp, sizeof(struct in6_addr));
    memcpy(&ip6->ip6_dst, dstIp, sizeof(struct in6_addr));
    
    // fill in neighbor advertisement
    neigh_ad->nd_na_hdr.icmp6_type = ND_NEIGHBOR_ADVERT;
    neigh_ad->nd_na_hdr.icmp6_code = 0;
    neigh_ad->nd_na_flags_reserved = 
        ND_NA_FLAG_ROUTER | ND_NA_FLAG_SOLICITED | ND_NA_FLAG_OVERRIDE;
    memcpy(&neigh_ad->nd_na_target, srcIp, sizeof(struct in6_addr));
    target_ll->nd_opt_type = ND_OPT_TARGET_LINKADDR;
    target_ll->nd_opt_len = 1;
    memcpy(((char*)target_ll)+2, srcMac, ETH_ADDR_LEN);
    
    // compute checksum
    uint32_t chksum = 0;
    // pseudoheader
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_src, 
                 sizeof(struct in6_addr));
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_dst, 
                 sizeof(struct in6_addr));
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_plen, 2);
    chksum += (uint16_t)htons(58);
    // payload
    chksum_accum(chksum, payload, payloadLen);
    neigh_ad->nd_na_hdr.icmp6_cksum = chksum_finalize(chksum);

    return b;
}


}
