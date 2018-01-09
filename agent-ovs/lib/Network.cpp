/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Network.h>

#include <boost/functional/hash.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include <vector>

#include <endian.h>

namespace std {
std::size_t hash<opflexagent::network::subnet_t>::
operator()(const opflexagent::network::subnet_t& u) const {
    return boost::hash_value(u);
}
} /* namespace std */

namespace opflexagent {
namespace network {

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;
using boost::algorithm::is_any_of;
using boost::algorithm::split;

std::ostream & operator<<(std::ostream &os, const subnet_t& subnet) {
    os << subnet.first << "/" << (int)subnet.second;
    return os;
}

std::ostream & operator<<(std::ostream &os, const subnets_t& subnets) {
    bool first = true;
    for (subnet_t s : subnets) {
        if (first) first = false;
        else os << ",";
        os << s;
    }
    return os;
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

address_v6 construct_auto_ip_addr(address_v6 prefix,
                                  const uint8_t* srcMac) {
    address_v6::bytes_type ip;
    construct_auto_ip(prefix, srcMac, (struct in6_addr*)ip.data());
    return address_v6(ip);
}

address_v6 construct_link_local_ip_addr(const uint8_t* srcMac) {
    return construct_auto_ip_addr(address_v6::from_string("fe80::"),
                                  srcMac);
}

address_v6 construct_link_local_ip_addr(const opflex::modb::MAC& srcMac) {
    uint8_t bytes[6];
    srcMac.toUIntArray(bytes);
    return construct_auto_ip_addr(address_v6::from_string("fe80::"),
                                  bytes);
}

address_v6 construct_solicited_node_ip(const address_v6& ip) {
    static const uint8_t solicitedPrefix[13] =
        {0xff, 0x02, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0xff};
    address_v6::bytes_type bytes = ip.to_bytes();
    memcpy(bytes.data(), solicitedPrefix, sizeof(solicitedPrefix));
    return address_v6(bytes);
}

uint32_t get_subnet_mask_v4(uint8_t prefixLen) {
    return (prefixLen != 0)
           ? (~((uint32_t)0) << (32 - prefixLen))
           : 0;
}

void compute_ipv6_subnet(boost::asio::ip::address_v6 netAddr,
                         uint8_t prefixLen,
                         /* out */ struct in6_addr* mask,
                         /* out */ struct in6_addr* addr) {
    std::memcpy(addr, netAddr.to_bytes().data(), sizeof(struct in6_addr));
    get_subnet_mask_v6(prefixLen, mask);
    ((uint64_t*)addr)[0] &= ((uint64_t*)mask)[0];
    ((uint64_t*)addr)[1] &= ((uint64_t*)mask)[1];

}

void get_subnet_mask_v6(uint8_t prefixLen, in6_addr *mask) {
    if (prefixLen == 0) {
        memset(mask, 0, sizeof(struct in6_addr));
    } else if (prefixLen <= 64) {
        ((uint64_t*)mask)[0] = htobe64(~((uint64_t)0) << (64 - prefixLen));
        ((uint64_t*)mask)[1] = 0;
    } else {
        ((uint64_t*)mask)[0] = ~((uint64_t)0);
        ((uint64_t*)mask)[1] = htobe64(~((uint64_t)0) << (128 - prefixLen));
    }
}

address mask_address(const address& addrIn, uint8_t prefixLen) {
    if (addrIn.is_v4()) {
        prefixLen = std::min<uint8_t>(prefixLen, 32);
        uint32_t mask = get_subnet_mask_v4(prefixLen);
        return address_v4(addrIn.to_v4().to_ulong() & mask);
    }
    struct in6_addr mask;
    struct in6_addr addr6;
    prefixLen = std::min<uint8_t>(prefixLen, 128);
    compute_ipv6_subnet(addrIn.to_v6(), prefixLen, &mask, &addr6);
    address_v6::bytes_type data;
    std::memcpy(data.data(), &addr6, sizeof(addr6));
    return address_v6(data);
}

bool is_link_local(const boost::asio::ip::address& addr) {
    if (addr.is_v6() && addr.to_v6().is_link_local())
        return true;
    if (addr.is_v4() &&
        (mask_address(addr, 16) == address::from_string("169.254.0.0")))
        return true;
    return false;
}

bool cidr_from_string(const std::string& cidrStr, cidr_t& cidr) {
    std::vector<std::string> parts;
    uint8_t prefixLen = 32;

    split(parts, cidrStr, boost::is_any_of("/"));
    boost::system::error_code ec;
    address baseIp = address::from_string(
        parts.size() == 2 ? parts[0] : cidrStr, ec);
    if (ec) {
        return false;
    }

    if (parts.size() == 2) {
        try {
            prefixLen = boost::lexical_cast<uint16_t>(parts[1]);
        } catch (const boost::bad_lexical_cast& ex) {
            return false;
        }
        baseIp = mask_address(baseIp, prefixLen);
    } else if (baseIp.is_v6()) {
        prefixLen = 128;
    }

    cidr = std::make_pair(baseIp, prefixLen);
    return true;
}

bool cidr_contains(const cidr_t& cidr, const address& addr) {
    return (cidr.first == mask_address(addr, cidr.second));
}

} /* namespace packets */
} /* namespace opflexagent */
