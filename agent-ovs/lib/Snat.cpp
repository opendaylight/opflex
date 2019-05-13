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
       << ",local=" << s.isLocal();

    if (s.getDest())
        os << ",dest=" << s.getDest().get();
    if (s.getDestPrefix())
        os << ",prefix=" << unsigned(s.getDestPrefix().get());
    if (s.getZone())
        os << ",zone=" << s.getZone().get();
    if (s.getInterfaceName())
        os << ",interface-name=" << s.getInterfaceName().get();
    if (s.getInterfaceMAC())
        os << ",interface-mac=" << s.getInterfaceMAC().get();
    if (s.getIfaceVlan())
        os << ",interface-vlan=" << s.getIfaceVlan().get();

    if (s.getPortRanges().size() > 0) {
        bool first = true;
        os << ",port-ranges=[";
        for (const auto& pr : s.getPortRanges()) {
            if (first) first = false;
            else os << ",";
            os << "(" << pr.start << "," << pr.end << ")";
        }
        os << "]";
    }
    os << "]";

    return os;
}

} /* namespace opflexagent */
