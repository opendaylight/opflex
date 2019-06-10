/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Snat class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Snat.h>

namespace opflexagent {
std::ostream & operator<<(std::ostream &os, const Snat& s) {

    os << "Snat["
       << "uuid=" << s.getUUID()
       << ",snat-ip=" << s.getSnatIP()
       << ",interface-name=" << s.getInterfaceName()
       << ",local=" << s.isLocal();

    if (s.getDest())
        os << ",dest=" << s.getDest().get();
    if (s.getDestPrefix())
        os << ",prefix=" << unsigned(s.getDestPrefix().get());
    if (s.getZone())
        os << ",zone=" << s.getZone().get();
    if (s.getInterfaceMAC())
        os << ",interface-mac=" << s.getInterfaceMAC().get();
    if (s.getIfaceVlan())
        os << ",interface-vlan=" << s.getIfaceVlan().get();

    Snat::PortRangeMap portRangeMap = s.getPortRangeMap();
    bool firstpr = true;
    os << ",port-range-map=<";
    for (auto it : portRangeMap) {
        if (firstpr) firstpr = false;
        else os << ",";
        os << "(macaddr=" << it.first;
        boost::optional<Snat::PortRanges> prs = it.second;

        if (prs != boost::none && prs.get().size() > 0) {
            bool firstprs = true;
            os << " port-ranges=[";
            for (const auto& pr : prs.get()) {
                if (firstprs) firstprs = false;
                else os << ",";
                os << "(" << pr.start << "," << pr.end << ")";
            }
            os << "]";
        }
        os << ")";
    }
    os << ">";
    os << "]";

    return os;
}

bool operator==(const Snat::port_range_t& lhs,
                const Snat::port_range_t& rhs) {
    return (lhs.start == rhs.start &&
            lhs.end == rhs.end);
}

bool operator!=(const Snat::port_range_t& lhs,
                const Snat::port_range_t& rhs) {
    return !(lhs==rhs);
}

} /* namespace opflexagent */
