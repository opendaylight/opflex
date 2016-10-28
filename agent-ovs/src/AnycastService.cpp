/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for AnycastService class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "AnycastService.h"

#include <boost/algorithm/string/join.hpp>

namespace ovsagent {

std::ostream & operator<<(std::ostream &os, const AnycastService& s) {
    using boost::algorithm::join;

    os << "AnycastService["
       << "uuid=" << s.getUUID();

    if (s.getInterfaceName())
        os << ",interface=" << s.getInterfaceName().get();
    if (s.getDomainURI())
        os << ",domain=" << s.getDomainURI().get();

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
        }
        os << "]";
    }
    os << "]";

    return os;
}

size_t AnycastService::ServiceMappingHash::
operator()(const AnycastService::ServiceMapping& m) const noexcept {
    size_t v = 0;
    if (m.getServiceIP())
        boost::hash_combine(v, m.getServiceIP().get());
    if (m.getGatewayIP())
        boost::hash_combine(v, m.getGatewayIP().get());
    for (const auto& ip : m.getNextHopIPs()) {
        boost::hash_combine(v, ip);
    }
    return v;
}

void AnycastService::addServiceMapping(const ServiceMapping& serviceMapping) {
    serviceMappings.insert(serviceMapping);
}

bool operator==(const AnycastService::ServiceMapping& lhs,
                const AnycastService::ServiceMapping& rhs) {
    return (lhs.getServiceIP() == rhs.getServiceIP() &&
            lhs.getGatewayIP() == rhs.getGatewayIP() &&
            lhs.getNextHopIPs() == rhs.getNextHopIPs());
}

bool operator!=(const AnycastService::ServiceMapping& lhs,
                const AnycastService::ServiceMapping& rhs) {
    return !(lhs==rhs);
}

} /* namespace ovsagent */
