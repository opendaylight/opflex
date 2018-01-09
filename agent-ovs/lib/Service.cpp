/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Service class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Service.h>

#include <boost/algorithm/string/join.hpp>

namespace opflexagent {

std::ostream & operator<<(std::ostream &os, const Service& s) {
    using boost::algorithm::join;

    os << "Service["
       << "uuid=" << s.getUUID();

    if (s.getServiceMode() == Service::LOCAL_ANYCAST) {
        os << ",local-anycast";
    } else if (s.getServiceMode() == Service::LOADBALANCER) {
        os << ",loadbalancer";
    }
    if (s.getDomainURI())
        os << ",domain=" << s.getDomainURI().get();

    if (s.getInterfaceName())
        os << ",interface=" << s.getInterfaceName().get();
    if (s.getIfaceVlan())
        os << ",vlan=" << s.getIfaceVlan().get();
    if (s.getIfaceIP())
        os << ",ip=" << s.getIfaceIP().get();
    if (s.getServiceMAC())
        os << ",mac=" << s.getServiceMAC().get();

    if (s.getServiceMappings().size() > 0) {
        bool first = true;
        os << ",serviceMappings=[";
        for (const auto& sm : s.getServiceMappings()) {
            if (first) first = false;
            else os << ",";

            if (sm.getServiceProto())
                os << sm.getServiceProto().get() << "://";

            if (sm.getServiceIP())
                os << sm.getServiceIP().get();
            else
                os << "None";

            if (sm.getServicePort())
                os << ":" << sm.getServicePort().get();

            if (!sm.getNextHopIPs().empty()) {
                os << "->[" << join(sm.getNextHopIPs(), ",") << "]";
            }

            if (sm.getNextHopPort())
                os << ":" << sm.getNextHopPort().get();

            if (sm.isConntrackMode())
                os << " (conntrack)";
        }

        os << "]";
    }

    os << "]";

    return os;
}

size_t Service::ServiceMappingHash::
operator()(const Service::ServiceMapping& m) const noexcept {
    size_t v = 0;
    if (m.getServiceIP())
        boost::hash_combine(v, m.getServiceIP().get());
    if (m.getServiceProto())
        boost::hash_combine(v, m.getServiceProto().get());
    if (m.getServicePort())
        boost::hash_combine(v, m.getServicePort().get());
    if (m.getGatewayIP())
        boost::hash_combine(v, m.getGatewayIP().get());
    if (m.getNextHopPort())
        boost::hash_combine(v, m.getNextHopPort().get());
    boost::hash_combine(v, m.isConntrackMode());
    for (const auto& ip : m.getNextHopIPs()) {
        boost::hash_combine(v, ip);
    }
    return v;
}

void Service::addServiceMapping(const ServiceMapping& serviceMapping) {
    serviceMappings.insert(serviceMapping);
}

bool operator==(const Service::ServiceMapping& lhs,
                const Service::ServiceMapping& rhs) {
    return (lhs.getServiceIP() == rhs.getServiceIP() &&
            lhs.getServiceProto() == rhs.getServiceProto() &&
            lhs.getServicePort() == rhs.getServicePort() &&
            lhs.getGatewayIP() == rhs.getGatewayIP() &&
            lhs.getNextHopIPs() == rhs.getNextHopIPs() &&
            lhs.getNextHopPort() == rhs.getNextHopPort() &&
            lhs.isConntrackMode() == rhs.isConntrackMode());
}

bool operator!=(const Service::ServiceMapping& lhs,
                const Service::ServiceMapping& rhs) {
    return !(lhs==rhs);
}

} /* namespace opflexagent */
