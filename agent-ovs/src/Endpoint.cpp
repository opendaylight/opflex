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

#include <boost/foreach.hpp>

#include "Endpoint.h"

namespace ovsagent {

std::ostream & operator<<(std::ostream &os, const Endpoint& ep) {
    os << "Endpoint["
       << "uuid=" << ep.getUUID()
       << ",ips=[";

    bool first = true;
    BOOST_FOREACH(const std::string& ip, ep.getIPs()) {
        if (first) first = false;
        else os << ",";
        os << ip;
    }
    os << "]";
    if (ep.getIPAddressMappings().size() > 0) {
        os << ",ipAddressMappings=[";

        first = true;
        BOOST_FOREACH(const Endpoint::IPAddressMapping& ipm,
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
    const boost::optional<opflex::modb::MAC>& m = ep.getMAC();
    if (m)
        os << ",mac=" << m.get();
    const boost::optional<std::string>& iface = ep.getInterfaceName();
    if (iface)
        os << ",iface=" << iface.get();
    if (ep.getDHCPv4Config())
        os << ",dhcpv4";
    if (ep.getDHCPv6Config())
        os << ",dhcpv6";

    return os;
}

size_t hash_value(Endpoint::IPAddressMapping const& ip) {
    size_t v = 0;
    boost::hash_combine(v, ip.getUUID());
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

} /* namespace ovsagent */
