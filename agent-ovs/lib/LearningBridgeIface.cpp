/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for LearningBridgeIface class.
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/LearningBridgeIface.h>

#include <boost/algorithm/string/join.hpp>

namespace std {

std::ostream&
operator<<(std::ostream &os,
           const opflexagent::LearningBridgeIface::vlan_range_t& range) {
    os << range.first;
    if (range.second != range.first)
        os << "-" << range.second;
    return os;
}

} /* namespace std */

namespace opflexagent {

std::ostream & operator<<(std::ostream &os, const LearningBridgeIface& i) {
    using boost::algorithm::join;

    os << "LearningBridgeIface["
       << "uuid=" << i.getUUID();

    const boost::optional<std::string>& iface = i.getInterfaceName();
    if (iface)
        os << ",iface=" << iface.get();

    bool first = true;
    if (i.getTrunkVlans().size() > 0) {
        os << ",trunk_vlans=[";
        for (auto& range : i.getTrunkVlans()) {
            if (first) first = false;
            else os << ",";
            os << range;
        }
        os << "]";
    }

    os << "]";

    return os;
}

} /* namespace opflexagent */
