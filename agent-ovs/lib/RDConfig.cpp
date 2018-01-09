/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for RDConfig class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/RDConfig.h>

namespace opflexagent {

std::ostream & operator<<(std::ostream &os, const RDConfig& s) {
    os << "RDConfig["
       << "rdURI=" << s.getDomainURI();

    if (s.getInternalSubnets().size() > 0) {
        bool first = true;
        os << ",internalSubnets=[";
        for (const std::string& sn : s.getInternalSubnets()) {
            if (first) first = false;
            else os << ",";
            os << sn;
        }
        os << "]";
    }
    os << "]";

    return os;
}

} /* namespace opflexagent */
