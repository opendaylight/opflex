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

#include <boost/foreach.hpp>

#include "AnycastService.h"

namespace ovsagent {

std::ostream & operator<<(std::ostream &os, const AnycastService& s) {
    os << "AnycastService["
       << "uuid=" << s.getUUID();

    if (s.getInterfaceName())
        os << ",interface=" << s.getInterfaceName().get();
    if (s.getDomainURI())
        os << ",domain=" << s.getDomainURI().get();

    if (s.getServiceMappings().size() > 0) {
        bool first = true;
        os << ",serviceMappings=[";
        BOOST_FOREACH(const AnycastService::ServiceMapping& sm,
                      s.getServiceMappings()) {
            if (first) first = false;
            else os << ",";
            if (sm.getServiceIP())
                os << sm.getServiceIP().get();
            else
                os << "None";
            if (sm.getNextHopIP())
                os << "->" << sm.getNextHopIP().get();
        }
        os << "]";
    }
    os << "]";

    return os;
}

size_t hash_value(AnycastService::ServiceMapping const& m) {
    size_t v = 0;
    if (m.getServiceIP())
        boost::hash_combine(v, m.getServiceIP().get());
    if (m.getGatewayIP())
        boost::hash_combine(v, m.getGatewayIP().get());
    if (m.getNextHopIP())
        boost::hash_combine(v, m.getNextHopIP().get());
    return v;
}

void AnycastService::addServiceMapping(const ServiceMapping& serviceMapping) {
    serviceMappings.insert(serviceMapping);
}

bool operator==(const AnycastService::ServiceMapping& lhs,
                const AnycastService::ServiceMapping& rhs) {
    return (lhs.getServiceIP() == rhs.getServiceIP() &&
            lhs.getGatewayIP() == rhs.getGatewayIP() &&
            lhs.getNextHopIP() == rhs.getNextHopIP());
}

bool operator!=(const AnycastService::ServiceMapping& lhs,
                const AnycastService::ServiceMapping& rhs) {
    return !(lhs==rhs);
}

} /* namespace ovsagent */
