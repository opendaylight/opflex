/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <netinet/ip.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/foreach.hpp>

#include <modelgbp/gbp/AutoconfigEnumT.hpp>

#include "Packets.h"
#include "dhcp.h"
#include "udp.h"
#include "logging.h"

namespace ovsagent {
namespace packets {

using std::string;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

void chksum_accum(uint32_t& chksum, uint16_t* addr, size_t len) {
    while (len > 1)  {
        chksum += *addr++;
        len -= 2;
    }
    if (len > 0)
        chksum += *(uint8_t*)addr;
}

uint16_t chksum_finalize(uint32_t chksum) {
    while (chksum>>16)
        chksum = (chksum & 0xffff) + (chksum >> 16);
    return ~chksum;
}

void construct_auto_ip(boost::asio::ip::address_v6 prefix,
                       const uint8_t* srcMac,
                       /* out */ struct in6_addr* dstAddr) {
    address_v6::bytes_type prefixb = prefix.to_bytes();
    memset(dstAddr, 0, sizeof(struct in6_addr));
    memcpy((char*)dstAddr, prefixb.data(), 8);
    memcpy(((char*)dstAddr) + 8, srcMac, 3);
    ((char*)dstAddr)[8] ^= 0x02;
    ((char*)dstAddr)[11] = 0xff;
    ((char*)dstAddr)[12] = 0xfe;
    memcpy(((char*)dstAddr) + 13, srcMac+3, 3);
}

struct nd_opt_def_route_info {
    uint8_t  nd_opt_ri_type;
    uint8_t  nd_opt_ri_len;
    uint8_t  nd_opt_ri_prefix_len;
    uint8_t  nd_opt_ri_flags;
    uint32_t nd_opt_ri_lifetime;
};

ofpbuf* compose_icmp6_router_ad(const uint8_t* srcMac,
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
        optional<const string&> networkAddrStr = sn->getAddress();
        if (!networkAddrStr) continue;
        
        boost::system::error_code ec;
        address networkAddr = address::from_string(networkAddrStr.get(), ec);
        if (ec) continue;
        
        if (networkAddr.is_v6())
            ipv6Subnets.push_back(sn);
    }

    if (ipv6Subnets.size() == 0)
        return NULL;

    struct ofpbuf* b = NULL;
    uint16_t payloadLen = sizeof(struct nd_router_advert) +
        sizeof(struct nd_opt_hdr) + 6 +
        + (mtu == 0 ? 0 : sizeof(struct nd_opt_mtu)) +
        sizeof(struct nd_opt_prefix_info) * ipv6Subnets.size() +
        sizeof(struct nd_opt_def_route_info);
    size_t len = sizeof(struct eth_header) + 
        sizeof(struct ip6_hdr) +
        payloadLen;

    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);

    // default route
    struct nd_opt_def_route_info* defroute = (struct  nd_opt_def_route_info*)
        ofpbuf_push_zeros(b, sizeof(struct nd_opt_def_route_info));
    defroute->nd_opt_ri_type = 24;
    defroute->nd_opt_ri_len = 1;
    defroute->nd_opt_ri_lifetime = 0xffffffff;

    // prefix information
    BOOST_FOREACH(shared_ptr<Subnet>& sn, ipv6Subnets) {
        optional<const string&> networkAddrStr = sn->getAddress();
        if (!networkAddrStr) continue;
        
        boost::system::error_code ec;
        address networkAddr = address::from_string(networkAddrStr.get(), ec);
        if (ec) continue;
        
        if (!networkAddr.is_v6()) continue;

        struct nd_opt_prefix_info* prefix = (struct nd_opt_prefix_info*)
            ofpbuf_push_zeros(b, sizeof(struct nd_opt_prefix_info));

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
    router_ad->nd_ra_router_lifetime = htons(1800);
    uint8_t autoconf =
        rd->get()->getIpv6Autoconfig(AutoconfigEnumT::CONST_STATELESS);
    if (autoconf == AutoconfigEnumT::CONST_BOTH ||
        autoconf == AutoconfigEnumT::CONST_DHCP)
        router_ad->nd_ra_flags_reserved |= ND_RA_FLAG_OTHER;
    if (autoconf == AutoconfigEnumT::CONST_DHCP)
        router_ad->nd_ra_flags_reserved |= ND_RA_FLAG_MANAGED;

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
    construct_auto_ip(address_v6::from_string("fe80::"), srcMac, 
                      &ip6->ip6_src);

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

ofpbuf* compose_icmp6_neigh_ad(uint32_t naFlags,
                               const uint8_t* srcMac,
                               const uint8_t* dstMac,
                               const struct in6_addr* srcIp,
                               const struct in6_addr* dstIp) {
    struct ofpbuf* b = NULL;
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
    ip6 = (struct ip6_hdr*) ofpbuf_push_zeros(b, sizeof(struct ip6_hdr));
    eth = (struct eth_header*) ofpbuf_push_zeros(b, sizeof(struct eth_header));
    
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
    neigh_ad->nd_na_flags_reserved = naFlags;
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

static const uint32_t LINK_LOCAL_DHCP = 0xa9fe2020;
static const size_t MAX_IP = 32;
static const size_t MAX_ROUTE = 16;

class Routev4 {
public:
    Routev4(address_v4 dest_, uint8_t prefixLen_, address_v4 nextHop_)
        : dest(dest_), prefixLen(prefixLen_), nextHop(nextHop_) {}
    address_v4 dest;
    uint8_t prefixLen;
    address_v4 nextHop;
};

ofpbuf* compose_dhcpv4_reply(uint8_t message_type,
                             uint32_t xid,
                             const uint8_t* srcMac,
                             const uint8_t* clientMac,
                             uint32_t clientIp,
                             uint8_t prefixLen,
                             const vector<string>& routers,
                             const vector<string>& dnsServers,
                             const optional<string>& domain,
                             const vector<static_route_t>& staticRoutes) {
    using namespace dhcp;
    using namespace udp;

    // note that this does a lot of unaligned reads/writes.  Don't try
    // to run on ARM or Sparc I guess

    struct ofpbuf* b = NULL;
    struct eth_header* eth = NULL;
    struct iphdr* ip = NULL;
    struct udp_hdr* udp = NULL;
    struct dhcp_hdr* dhcp = NULL;
    struct dhcp_option_hdr* message_type_opt = NULL;
    struct dhcp_option_hdr* subnet_mask = NULL;
    struct dhcp_option_hdr* routers_opt = NULL;
    struct dhcp_option_hdr* dns = NULL;
    struct dhcp_option_hdr* domain_opt = NULL;
    struct dhcp_option_hdr* lease_time = NULL;
    struct dhcp_option_hdr* server_identifier = NULL;
    struct dhcp_option_hdr* static_routes = NULL;
    struct dhcp_option_hdr_base* end = NULL;

    boost::system::error_code ec;

    // Compute size of reply and options
    size_t router_len = 0;
    size_t dns_len = 0;
    size_t domain_len = 0;
    size_t static_route_len = 0;

    vector<address_v4> routerIps;
    BOOST_FOREACH(const string& ipstr, routers) {
        address ip = address::from_string(ipstr, ec);
        if (ec || !ip.is_v4()) continue;
        routerIps.push_back(ip.to_v4());
        if (routerIps.size() >= MAX_IP) break;
    }
    if (routerIps.size() > 0) 
        router_len = 2 + 4*routerIps.size();

    vector<address_v4> dnsIps;
    BOOST_FOREACH(const string& ipstr, dnsServers) {
        address ip = address::from_string(ipstr, ec);
        if (ec || !ip.is_v4()) continue;
        dnsIps.push_back(ip.to_v4());
        if (dnsIps.size() > MAX_IP) break;
    }
    if (dnsIps.size() >= 0) 
        dns_len = 2 + 4*dnsIps.size();

    if (domain && domain.get().size() <= 255)
        domain_len = 2 + domain.get().size();

    vector<Routev4> routes;
    BOOST_FOREACH(const static_route_t& route, staticRoutes) {
        address dst = address::from_string(route.dest, ec);
        if (ec || !dst.is_v4()) continue;
        address nextHop = address::from_string(route.nextHop, ec);
        if (ec || !nextHop.is_v4()) continue;
        uint8_t prefix = route.prefixLen;
        if (prefix > 32) prefix = 32;

        static_route_len += (prefix / 8) + (prefix % 8 != 0) + 5;
        routes.push_back(Routev4(dst.to_v4(), prefix, nextHop.to_v4()));
        if (routes.size() >= MAX_ROUTE) break;
    }
    if (static_route_len > 0) static_route_len += 2;

    size_t payloadLen = 
        sizeof(struct dhcp_hdr) +
        DHCP_OPTION_MESSAGE_TYPE_LEN + 2 +
        DHCP_OPTION_IP_LEN + 2 + /* subnet mask */
        router_len +
        dns_len +
        domain_len +
        DHCP_OPTION_LEASE_TIME_LEN + 2 + 
        DHCP_OPTION_IP_LEN + 2 + /* server identifier */
        static_route_len +
        1 /* end */;

    // allocate the DHCP reply
    size_t len = sizeof(struct eth_header) + 
        sizeof(struct iphdr) +
        sizeof(struct udp_hdr) +
        payloadLen;
    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);
    end = (struct dhcp_option_hdr_base*)
        ofpbuf_push_zeros(b, 1);
    if (static_route_len > 0)
        static_routes = 
            (struct dhcp_option_hdr*) ofpbuf_push_zeros(b, static_route_len);
    server_identifier = (struct dhcp_option_hdr*)
        ofpbuf_push_zeros(b, DHCP_OPTION_IP_LEN + 2);
    lease_time = (struct dhcp_option_hdr*)
        ofpbuf_push_zeros(b, DHCP_OPTION_LEASE_TIME_LEN + 2);
    if (domain_len > 0)
        domain_opt = (struct dhcp_option_hdr*) ofpbuf_push_zeros(b, domain_len);
    if (dns_len > 0)
        dns = (struct dhcp_option_hdr*) ofpbuf_push_zeros(b, dns_len);
    if (router_len > 0)
        routers_opt =
            (struct dhcp_option_hdr*) ofpbuf_push_zeros(b, router_len);
    subnet_mask = (struct dhcp_option_hdr*)
        ofpbuf_push_zeros(b, DHCP_OPTION_IP_LEN + 2);
    message_type_opt = (struct dhcp_option_hdr*)
        ofpbuf_push_zeros(b, DHCP_OPTION_MESSAGE_TYPE_LEN + 2);
    dhcp = (struct dhcp_hdr*) ofpbuf_push_zeros(b, sizeof(struct dhcp_hdr));
    udp = (struct udp_hdr*) ofpbuf_push_zeros(b, sizeof(struct udp_hdr));
    ip = (struct iphdr*) ofpbuf_push_zeros(b, sizeof(struct iphdr));
    eth = (struct eth_header*) ofpbuf_push_zeros(b, sizeof(struct eth_header));

    // initialize ethernet header
    memcpy(eth->eth_src, srcMac, ETH_ADDR_LEN);
    memset(eth->eth_dst, 0xff, ETH_ADDR_LEN);
    eth->eth_type = htons(ETH_TYPE_IP);

    // initialize IPv4 header
    ip->version = 4;
    ip->ihl = sizeof(struct iphdr)/4;
    ip->tot_len = htons(payloadLen + 
                        sizeof(struct iphdr) + 
                        sizeof(struct udp_hdr));
    ip->ttl = 64;
    ip->protocol = 17;

    ip->saddr = htonl(LINK_LOCAL_DHCP);
    ip->daddr = 0xffffffff;

    // compute IP header checksum
    uint32_t chksum = 0;
    chksum_accum(chksum, (uint16_t*)ip, sizeof(struct iphdr));
    ip->check = chksum_finalize(chksum);

    // initialize UDP header
    udp->src = htons(67);
    udp->dst = htons(68);
    udp->len = htons(payloadLen + sizeof(struct udp_hdr));

    // initialize DHCP header
    dhcp->op = 2;
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->xid = xid;
    dhcp->yiaddr = htonl(clientIp);
    dhcp->siaddr = htonl(LINK_LOCAL_DHCP);
    memcpy(dhcp->chaddr, clientMac, ETH_ADDR_LEN);
    dhcp->cookie[0] = 99;
    dhcp->cookie[1] = 130;
    dhcp->cookie[2] = 83;
    dhcp->cookie[3] = 99;

    // initialize DHCP options
    message_type_opt->code = DHCP_OPTION_MESSAGE_TYPE;
    message_type_opt->len = DHCP_OPTION_MESSAGE_TYPE_LEN;
    ((char*)message_type_opt)[2] = message_type;

    subnet_mask->code = DHCP_OPTION_SUBNET_MASK;
    subnet_mask->len = DHCP_OPTION_IP_LEN;
    if (prefixLen > 32) prefixLen = 32;
    *((uint32_t*)((char*)subnet_mask + 2)) =
        htonl(0xffffffff << (32-prefixLen));

    if (router_len > 0) {
        routers_opt->code = DHCP_OPTION_ROUTER;
        routers_opt->len = 4 * routerIps.size();
        uint32_t* ipptr =
            (uint32_t*)((char*)routers_opt + 2);
        BOOST_FOREACH(const address_v4& ip, routerIps) {
            *ipptr = htonl(ip.to_ulong());
            ipptr += 1;
        }
    }

    if (dns_len > 0) {
        dns->code = DHCP_OPTION_DNS;
        dns->len = 4 * dnsIps.size();
        uint32_t* ipptr =
            (uint32_t*)((char*)dns + 2);
        BOOST_FOREACH(const address_v4& ip, dnsIps) {
            *ipptr = htonl(ip.to_ulong());
            ipptr += 1;
        }
    }

    if (domain_len > 0) {
        domain_opt->code = DHCP_OPTION_DOMAIN_NAME;
        domain_opt->len = domain.get().size();
        memcpy(((char*)domain_opt + 2), domain.get().c_str(),
               domain.get().size());
    }

    lease_time->code = DHCP_OPTION_LEASE_TIME;
    lease_time->len = DHCP_OPTION_LEASE_TIME_LEN;
    *((uint32_t*)((char*)lease_time + 2)) = htonl(86400);

    server_identifier->code = DHCP_OPTION_SERVER_IDENTIFIER;
    server_identifier->len = DHCP_OPTION_IP_LEN;
    *((uint32_t*)((char*)server_identifier + 2)) = htonl(LINK_LOCAL_DHCP);

    if (static_route_len > 0) {
        static_routes->code = DHCP_OPTION_CLASSLESS_STATIC_ROUTE;
        static_routes->len = static_route_len - 2;
        char* cur = (char*)static_routes + 2;
        BOOST_FOREACH(const Routev4& route, routes) {
            uint8_t octets = (route.prefixLen / 8) + (route.prefixLen % 8 != 0);
            uint32_t dest = htonl(route.dest.to_ulong());
            uint32_t nexthop = htonl(route.nextHop.to_ulong());
            *cur++ = route.prefixLen;
            for (uint8_t i = 0; i < octets; i++) {
                *cur++ = ((char*)&dest)[i];
            }
            *((uint32_t*)cur) = nexthop;
        }
    }

    end->code = DHCP_OPTION_END;

    // compute UDP checksum
    chksum = 0;
    // pseudoheader
    chksum_accum(chksum, (uint16_t*)&ip->saddr, 4);
    chksum_accum(chksum, (uint16_t*)&ip->daddr, 4);
    struct {uint8_t zero; uint8_t proto;} proto;
    proto.zero = 0;
    proto.proto = ip->protocol;
    chksum_accum(chksum, (uint16_t*)&proto, 2);
    chksum_accum(chksum, (uint16_t*)&udp->len, 2);
    // payload
    chksum_accum(chksum, (uint16_t*)udp, 
                 payloadLen + sizeof(struct udp_hdr));
    udp->chksum = chksum_finalize(chksum);

    return b;
}


} /* namespace packets */
} /* namespace ovsagent */
