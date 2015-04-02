/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <netinet/ip.h>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <modelgbp/gbp/AutoconfigEnumT.hpp>

#include "Packets.h"
#include "dhcp.h"
#include "udp.h"
#include "logging.h"
#include "arp.h"

namespace ovsagent {
namespace packets {

const uint8_t MAC_ADDR_BROADCAST[6] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const uint8_t MAC_ADDR_MULTICAST[6] =
    {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t MAC_ADDR_IPV6MULTICAST[6] =
    {0x33, 0x33, 0x00, 0x00, 0x00, 0x01};

using std::string;
using std::stringbuf;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using boost::algorithm::split;
using boost::algorithm::token_compress_on;
using boost::algorithm::is_any_of;

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

void compute_ipv6_subnet(boost::asio::ip::address_v6 netAddr,
                         uint8_t prefixLen,
                         /* out */ struct in6_addr* mask,
                         /* out */ struct in6_addr* addr) {
    std::memcpy(addr, netAddr.to_bytes().data(), sizeof(struct in6_addr));

    if (prefixLen == 0) {
        memset(mask, 0, sizeof(struct in6_addr));
    } else if (prefixLen <= 64) {
        ((uint64_t*)mask)[0] = htonll(~((uint64_t)0) << (64 - prefixLen));
        ((uint64_t*)mask)[1] = 0;
    } else {
        ((uint64_t*)mask)[0] = ~((uint64_t)0);
        ((uint64_t*)mask)[1] = htonll(~((uint64_t)0) << (128 - prefixLen));
    }
    ((uint64_t*)addr)[0] &= ((uint64_t*)mask)[0];
    ((uint64_t*)addr)[1] &= ((uint64_t*)mask)[1];

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
    char* buf = (char*)ofpbuf_push_zeros(b, len);
    eth = (struct eth_header*)buf;
    buf += sizeof(struct eth_header);
    ip6 = (struct ip6_hdr*)buf;
    buf += sizeof(struct ip6_hdr);
    neigh_ad = (struct nd_neighbor_advert*)buf;
    buf += sizeof(struct nd_neighbor_advert);
    target_ll = (struct nd_opt_hdr*)buf;
    buf += sizeof(struct nd_opt_hdr) + 6;
    
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
        address_v4 ip = address_v4::from_string(ipstr, ec);
        if (ec) continue;
        routerIps.push_back(ip);
        if (routerIps.size() >= MAX_IP) break;
    }
    if (routerIps.size() > 0) 
        router_len = 2 + 4*routerIps.size();

    vector<address_v4> dnsIps;
    BOOST_FOREACH(const string& ipstr, dnsServers) {
        address_v4 ip = address_v4::from_string(ipstr, ec);
        if (ec) continue;
        dnsIps.push_back(ip);
        if (dnsIps.size() >= MAX_IP) break;
    }
    if (dnsIps.size() >= 0) 
        dns_len = 2 + 4*dnsIps.size();

    if (domain && domain.get().size() <= 255)
        domain_len = 2 + domain.get().size();

    vector<Routev4> routes;
    BOOST_FOREACH(const static_route_t& route, staticRoutes) {
        address_v4 dst = address_v4::from_string(route.dest, ec);
        if (ec) continue;
        address_v4 nextHop = address_v4::from_string(route.nextHop, ec);
        if (ec) continue;
        uint8_t prefix = route.prefixLen;
        if (prefix > 32) prefix = 32;

        static_route_len += (prefix / 8) + (prefix % 8 != 0) + 5;
        routes.push_back(Routev4(dst, prefix, nextHop));
        if (routes.size() >= MAX_ROUTE) break;
    }
    if (static_route_len > 0) static_route_len += 2;

    size_t payloadLen = 
        sizeof(struct dhcp_hdr) +
        option::MESSAGE_TYPE_LEN + 2 +
        option::IP_LEN + 2 + /* subnet mask */
        router_len +
        dns_len +
        domain_len +
        option::LEASE_TIME_LEN + 2 + 
        option::IP_LEN + 2 + /* server identifier */
        static_route_len +
        1 /* end */;
    size_t len = sizeof(struct eth_header) + 
        sizeof(struct iphdr) +
        sizeof(struct udp_hdr) +
        payloadLen;

    // allocate the DHCP reply
    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);
    char* buf =  (char*)ofpbuf_push_zeros(b, len);
    eth = (struct eth_header*)buf;
    buf += sizeof(struct eth_header);
    ip = (struct iphdr*)buf;
    buf += sizeof(struct iphdr);
    udp = (struct udp_hdr*)buf;
    buf += sizeof(struct udp_hdr);
    dhcp = (struct dhcp_hdr*)buf;
    buf += sizeof(struct dhcp_hdr);
    message_type_opt = (struct dhcp_option_hdr*)buf;
    buf += option::MESSAGE_TYPE_LEN + 2;
    subnet_mask = (struct dhcp_option_hdr*)buf;
    buf += option::IP_LEN + 2;
    if (router_len > 0) {
        routers_opt = (struct dhcp_option_hdr*)buf;
        buf += router_len;
    }
    if (dns_len > 0) {
        dns = (struct dhcp_option_hdr*)buf;
        buf += dns_len;
    }
    if (domain_len > 0) {
        domain_opt = (struct dhcp_option_hdr*)buf;
        buf += domain_len;
    }
    lease_time = (struct dhcp_option_hdr*)buf;
    buf += option::LEASE_TIME_LEN + 2;
    server_identifier = (struct dhcp_option_hdr*)buf;
    buf += option::IP_LEN + 2;
    if (static_route_len > 0) {
        static_routes = (struct dhcp_option_hdr*)buf;
        buf += static_route_len;
    }
    end = (struct dhcp_option_hdr_base*)buf;
    buf += 1;

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
    message_type_opt->code = option::MESSAGE_TYPE;
    message_type_opt->len = option::MESSAGE_TYPE_LEN;
    ((char*)message_type_opt)[2] = message_type;

    subnet_mask->code = option::SUBNET_MASK;
    subnet_mask->len = option::IP_LEN;
    if (prefixLen > 32) prefixLen = 32;
    *((uint32_t*)((char*)subnet_mask + 2)) =
        htonl(0xffffffff << (32-prefixLen));

    if (router_len > 0) {
        routers_opt->code = option::ROUTER;
        routers_opt->len = 4 * routerIps.size();
        uint32_t* ipptr =
            (uint32_t*)((char*)routers_opt + 2);
        BOOST_FOREACH(const address_v4& ip, routerIps) {
            *ipptr = htonl(ip.to_ulong());
            ipptr += 1;
        }
    }

    if (dns_len > 0) {
        dns->code = option::DNS;
        dns->len = 4 * dnsIps.size();
        uint32_t* ipptr =
            (uint32_t*)((char*)dns + 2);
        BOOST_FOREACH(const address_v4& ip, dnsIps) {
            *ipptr = htonl(ip.to_ulong());
            ipptr += 1;
        }
    }

    if (domain_len > 0) {
        domain_opt->code = option::DOMAIN_NAME;
        domain_opt->len = domain.get().size();
        memcpy(((char*)domain_opt + 2), domain.get().c_str(),
               domain.get().size());
    }

    lease_time->code = option::LEASE_TIME;
    lease_time->len = option::LEASE_TIME_LEN;
    *((uint32_t*)((char*)lease_time + 2)) = htonl(86400);

    server_identifier->code = option::SERVER_IDENTIFIER;
    server_identifier->len = option::IP_LEN;
    *((uint32_t*)((char*)server_identifier + 2)) = htonl(LINK_LOCAL_DHCP);

    if (static_route_len > 0) {
        static_routes->code = option::CLASSLESS_STATIC_ROUTE;
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

    end->code = option::END;

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

ofpbuf* compose_dhcpv6_reply(uint8_t message_type,
                             const uint8_t* xid,
                             const uint8_t* srcMac,
                             const uint8_t* clientMac,
                             const struct in6_addr* dstIp,
                             uint8_t* client_id,
                             uint16_t client_id_len,
                             uint8_t* iaid,
                             const vector<address_v6>& ips,
                             const vector<string>& dnsServers,
                             const vector<string>& searchList,
                             bool temporary,
                             bool rapid) {
    using namespace dhcp6;
    using namespace udp;

    // note that this does a lot of unaligned reads/writes.  Don't try
    // to run on ARM or Sparc I guess

    struct ofpbuf* b = NULL;
    struct eth_header* eth = NULL;
    struct ip6_hdr* ip6 = NULL;
    struct udp_hdr* udp = NULL;
    struct dhcp6_hdr* dhcp = NULL;
    struct dhcp6_opt_hdr* client_id_opt = NULL;
    struct dhcp6_opt_hdr* server_id_opt = NULL;
    struct dhcp6_opt_hdr* ia = NULL;
    struct dhcp6_opt_hdr* dns = NULL;
    struct dhcp6_opt_hdr* domain_list = NULL;
    struct dhcp6_opt_hdr* rapid_opt = NULL;

    boost::system::error_code ec;

    // Compute size of reply and options
    size_t dns_len = 0;
    size_t domain_list_len = 0;
    size_t ia_len = 0;

    const size_t opt_hdr_len = sizeof(struct dhcp6_opt_hdr);

    vector<address_v6> dnsIps;
    BOOST_FOREACH(const string& ipstr, dnsServers) {
        address_v6 ip = address_v6::from_string(ipstr, ec);
        if (ec) continue;
        dnsIps.push_back(ip);
        if (dnsIps.size() >= MAX_IP) break;
    }
    if (dnsIps.size() >= 0) 
        dns_len = opt_hdr_len + sizeof(struct in6_addr) * dnsIps.size();

    stringbuf domain_opt_buf;
    size_t domain_opt_len = 0;
    BOOST_FOREACH(const string& domain, searchList) {
        if (domain.size() > 255) continue;
        if (domain_opt_len > 512) break;

        vector<string> dchunks;
        split(dchunks, domain, is_any_of("."), token_compress_on);

        bool validdomain = true;
        BOOST_FOREACH(const string& dchunk, dchunks) {
            if (dchunk.size() > 63) {
                validdomain = false;
                break;
            }
        }
        if (!validdomain || dchunks.size() == 0) continue;

        BOOST_FOREACH(const string& dchunk, dchunks) {
            domain_opt_buf.sputc((uint8_t)dchunk.size());
            domain_opt_buf.sputn(dchunk.c_str(), dchunk.size());
            domain_opt_len += dchunk.size() + 1;
        }
        domain_opt_buf.sputc(0);
        domain_opt_len += 1;
    }
    if (domain_opt_len > 0) {
        domain_list_len = domain_opt_len + opt_hdr_len;
    }

    if (ips.size() > 0 && iaid != NULL) {
        ia_len = opt_hdr_len + 4 + ips.size() * (24 + opt_hdr_len);
        if (!temporary) ia_len += 8;
    }

    size_t payloadLen = 
        sizeof(struct dhcp6_hdr) +
        opt_hdr_len + client_id_len + /* client id */
        opt_hdr_len + 10 + /* server id */
        dns_len +
        domain_list_len +
        ia_len +
        (rapid ? opt_hdr_len : 0);
    size_t len = sizeof(struct eth_header) + 
        sizeof(struct ip6_hdr) +
        sizeof(struct udp_hdr) +
        payloadLen;

    // allocate the DHCPv6 reply
    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);
    char* buf = (char*)ofpbuf_push_zeros(b, len);
    eth = (struct eth_header*)buf;
    buf += sizeof(struct eth_header);
    ip6 = (struct ip6_hdr*)buf;
    buf += sizeof(struct ip6_hdr);
    udp = (struct udp_hdr*)buf;
    buf += sizeof(struct udp_hdr);
    dhcp = (struct dhcp6_hdr*)buf;
    buf += sizeof(struct dhcp6_hdr);
    server_id_opt = (struct dhcp6_opt_hdr*)buf;
    buf += opt_hdr_len + 10;
    client_id_opt = (struct dhcp6_opt_hdr*)buf;
    buf += opt_hdr_len + client_id_len;
    if (ia_len > 0) {
        ia = (struct dhcp6_opt_hdr*)buf;
        buf += ia_len;
    }
    if (dns_len > 0) {
        dns = (struct dhcp6_opt_hdr*)buf;
        buf += dns_len;
    }
    if (domain_list_len > 0) {
        domain_list = (struct dhcp6_opt_hdr*)buf;
        buf += domain_list_len;
    }
    if (rapid) {
        rapid_opt = (struct dhcp6_opt_hdr*)buf;
        buf += opt_hdr_len;
    }

    // initialize ethernet header
    memcpy(eth->eth_src, srcMac, ETH_ADDR_LEN);
    memcpy(eth->eth_dst, clientMac, ETH_ADDR_LEN);
    eth->eth_type = htons(ETH_TYPE_IPV6);

    // Initialize IPv6 header
    ip6->ip6_vfc = 0x60;
    ip6->ip6_hlim = 255;
    ip6->ip6_nxt = 17;
    ip6->ip6_plen = htons(payloadLen + sizeof(udp_hdr));
 
    // IPv6 link-local address made from the DHCP MAC
    construct_auto_ip(address_v6::from_string("fe80::"), srcMac, 
                      &ip6->ip6_src);

    memcpy(&ip6->ip6_dst, dstIp, sizeof(struct in6_addr));

    // initialize UDP header
    udp->src = htons(547);
    udp->dst = htons(546);
    udp->len = htons(payloadLen + sizeof(struct udp_hdr));

    // Initialize DHCPv6 header
    dhcp->msg_type = message_type;
    memcpy(dhcp->transaction_id, xid, 3);

    // Server ID
    server_id_opt->option_code = htons(option::SERVER_IDENTIFIER);
    server_id_opt->option_len = htons(10);
    uint16_t* duid_type = (uint16_t*)((char*)server_id_opt + opt_hdr_len);
    *duid_type = htons(3 /* DUID-LL */);
    uint16_t* hw_type = duid_type + 1;
    *hw_type = htons(1 /* ethernet */);
    uint8_t* lladdr = (uint8_t*)(hw_type + 1);
    memcpy(lladdr, srcMac, ETH_ADDR_LEN);

    // Client ID
    client_id_opt->option_code = htons(option::CLIENT_IDENTIFIER);
    client_id_opt->option_len = htons(client_id_len);
    uint8_t* clid = (uint8_t*)((char*)client_id_opt + opt_hdr_len);
    memcpy(clid, client_id, client_id_len);

    // Identity association
    if (ia_len > 0) {
        ia->option_code = htons(temporary ? option::IA_TA : option::IA_NA);
        ia->option_len = htons(ia_len - opt_hdr_len);
        uint8_t* iaid_ptr = (uint8_t*)((char*)ia + opt_hdr_len);
        memcpy(iaid_ptr, iaid, 4);
        struct dhcp6_opt_hdr* iaaddr = NULL;
        if (temporary) {
            iaaddr = (struct dhcp6_opt_hdr*)((char*)ia + opt_hdr_len + 4);
        } else {
            iaaddr = (struct dhcp6_opt_hdr*)((char*)ia + opt_hdr_len + 12);
            uint32_t* t1 = ((uint32_t*)iaid_ptr + 1);
            uint32_t* t2 = t1 + 1;
            *t1 = htonl(3600);
            *t2 = htonl(5400);
        }
        BOOST_FOREACH(const address_v6& ip, ips) {
            size_t iaaddr_len = 24;
            iaaddr->option_code = htons(option::IAADDR);
            iaaddr->option_len = htons(iaaddr_len);

            struct in6_addr* addr =
                (struct in6_addr*)((char*)iaaddr + opt_hdr_len);
            address_v6::bytes_type bytes = ip.to_bytes();
            std::memcpy(addr, bytes.data(), bytes.size());
            uint32_t* pl = (uint32_t*)(addr + 1);
            uint32_t* vl = pl + 1;
            *pl = htonl(7200);
            *vl = htonl(7500);

            iaaddr =
                (struct dhcp6_opt_hdr*)((char*)iaaddr + opt_hdr_len + iaaddr_len);
        }
    }

    // DNS servers
    if (dns_len > 0) {
        dns->option_code = htons(option::DNS_SERVERS);
        dns->option_len = htons(sizeof(struct in6_addr) * dnsIps.size());
        struct in6_addr* ipptr =
            (struct in6_addr*)((char*)dns + opt_hdr_len);
        BOOST_FOREACH(const address_v6& ip, dnsIps) {
            address_v6::bytes_type bytes = ip.to_bytes();
            std::memcpy(ipptr, bytes.data(), bytes.size());
            ipptr += 1;
        }
    }

    // Domain search list
    if (domain_list_len > 0) {
        domain_list->option_code = htons(option::DOMAIN_LIST);
        domain_list->option_len = htons(domain_opt_len);
        char* dbuf = (char*)domain_list + opt_hdr_len;
        domain_opt_buf.sgetn(dbuf, domain_opt_len);
    }

    // Rapid commit option
    if (rapid) {
        rapid_opt->option_code = htons(option::RAPID_COMMIT);
        rapid_opt->option_len = 0;
    }

    // compute checksum
    uint32_t chksum = 0;
    // pseudoheader
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_src, 
                 sizeof(struct in6_addr));
    chksum_accum(chksum, (uint16_t*)&ip6->ip6_dst, 
                 sizeof(struct in6_addr));
    uint32_t udpLen = htonl(payloadLen + sizeof(udp_hdr));
    chksum_accum(chksum, (uint16_t*)&udpLen, 4);
    struct {uint8_t zero[3]; uint8_t nh;} nh;
    memset(&nh.zero, 0, 3);
    nh.nh = ip6->ip6_nxt;
    chksum_accum(chksum, (uint16_t*)&nh, 4);
    // payload
    chksum_accum(chksum, (uint16_t*)udp, 
                 payloadLen + sizeof(struct udp_hdr));
    udp->chksum = chksum_finalize(chksum);

    return b;
}

ofpbuf* compose_arp(uint16_t op,
                    const uint8_t* srcMac,
                    const uint8_t* dstMac,
                    const uint8_t* sha,
                    const uint8_t* tha,
                    uint32_t spa,
                    uint32_t tpa) {
    using namespace arp;

    struct ofpbuf* b = NULL;
    struct eth_header* eth = NULL;
    struct arp_hdr* arp = NULL;
    uint8_t* shaptr = NULL;
    uint8_t* thaptr = NULL;
    uint32_t* spaptr = NULL;
    uint32_t* tpaptr = NULL;

    size_t len = 
        sizeof(eth_header) + sizeof(arp_hdr) + 
        2 * ETH_ADDR_LEN + 2 * 4;
    b = ofpbuf_new(len);
    ofpbuf_clear(b);
    ofpbuf_reserve(b, len);
    char* buf = (char*)ofpbuf_push_zeros(b, len);
    eth = (struct eth_header*)buf;
    buf += sizeof(struct eth_header);
    arp = (struct arp_hdr*)buf;
    buf += sizeof(struct arp_hdr);
    shaptr = (uint8_t*)buf;
    buf += ETH_ADDR_LEN;
    spaptr = (uint32_t*)buf;
    buf += 4;
    thaptr = (uint8_t*)buf;
    buf += ETH_ADDR_LEN;
    tpaptr = (uint32_t*)buf;
    buf += 4;

    // initialize ethernet header
    memcpy(eth->eth_src, srcMac, ETH_ADDR_LEN);
    memcpy(eth->eth_dst, dstMac, ETH_ADDR_LEN);
    eth->eth_type = htons(ETH_TYPE_ARP);

    // initialize the ARP packet
    arp->htype = htons(1);
    arp->ptype = htons(0x800);
    arp->hlen = ETH_ADDR_LEN;
    arp->plen = 4;
    arp->op = htons(op);

    memcpy(shaptr, sha, ETH_ADDR_LEN);
    memcpy(thaptr, tha, ETH_ADDR_LEN);
    *spaptr = htonl(spa);
    *tpaptr = htonl(tpa);

    return b;
}


} /* namespace packets */
} /* namespace ovsagent */
