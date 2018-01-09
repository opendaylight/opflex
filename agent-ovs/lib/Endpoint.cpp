/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Endpoint class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iomanip>
#include <arpa/inet.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/functional/hash.hpp>

#include <opflexagent/Endpoint.h>

namespace opflexagent {

std::ostream & operator<<(std::ostream &os, const Endpoint& ep) {
    using boost::algorithm::join;

    os << "Endpoint["
       << "uuid=" << ep.getUUID()
       << ",ips=[" << join(ep.getIPs(), ",") << "]";

    bool first = true;
    if (ep.getIPAddressMappings().size() > 0) {
        os << ",ipAddressMappings=[";

        for(const Endpoint::IPAddressMapping& ipm :
                ep.getIPAddressMappings()) {
            if (!ipm.getMappedIP()) continue;
            if (first) first = false;
            else os << ",";
            os << ipm.getMappedIP().get();
            if (ipm.getFloatingIP())
                os << "->" << ipm.getFloatingIP().get();

            if (ipm.getNextHopIf()) {
                os << " (nextHop=" << ipm.getNextHopIf().get();
                if (ipm.getNextHopMAC())
                    os << "," << ipm.getNextHopMAC().get();
                os << ")";
            }

        }
        os << "]";
    }

    const boost::optional<opflex::modb::URI>& u = ep.getEgURI();
    if (u)
        os << ",eg=" << u.get().toString();

    if (!ep.getSecurityGroups().empty()) {
        os << ",secGroups=[";
        first = true;
        for (const opflex::modb::URI& sg : ep.getSecurityGroups()) {
            if (first) first = false;
            else os << ",";
            os << sg.toString();
        }
        os << "]";
    }

    const boost::optional<opflex::modb::MAC>& m = ep.getMAC();
    if (m)
        os << ",mac=" << m.get();
    const boost::optional<std::string>& iface = ep.getInterfaceName();
    if (iface)
        os << ",iface=" << iface.get();
    const boost::optional<std::string>& accessIface = ep.getAccessInterface();
    if (accessIface)
        os << ",access-iface=" << accessIface.get();
    const boost::optional<std::string>& accessUplink =
        ep.getAccessUplinkInterface();
    if (accessUplink)
        os << ",access-uplink=" << accessUplink.get();
    if (!ep.getVirtualIPs().empty()) {
        os << ",virtual-ip=[";
        first = true;
        for (auto& vip : ep.getVirtualIPs()) {
            if (first) first = false;
            else os << ",";
            os << "(" << vip.first << "," << vip.second << ")";
        }
        os << "]";
    }
    if (ep.getDHCPv4Config())
        os << ",dhcpv4";
    if (ep.getDHCPv6Config())
        os << ",dhcpv6";

    return os;
}

size_t Endpoint::VirtIpTHash::
operator()(const Endpoint::virt_ip_t& m) const noexcept {
    return boost::hash_value(m);
}

size_t Endpoint::IPAddressMappingHash::
operator()(const Endpoint::IPAddressMapping& m) const noexcept {
    size_t v = 0;
    boost::hash_combine(v, m.getUUID());
    return v;
}

void Endpoint::addIPAddressMapping(const IPAddressMapping& ipAddressMapping) {
    ipAddressMappings.insert(ipAddressMapping);
}

bool operator==(const Endpoint::IPAddressMapping& lhs,
                const Endpoint::IPAddressMapping& rhs) {
    return lhs.getUUID() == rhs.getUUID();
}

bool operator!=(const Endpoint::IPAddressMapping& lhs,
                const Endpoint::IPAddressMapping& rhs) {
    return !(lhs==rhs);
}

void Endpoint::addAttestation(const Attestation& attest) {
    attestations.push_back(attest);
}

} /* namespace opflexagent */
