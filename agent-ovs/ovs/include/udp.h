/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Headers for the UDP protocol
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_UDP_H
#define OVSAGENT_UDP_H

namespace ovsagent {
namespace udp {

/**
 * The UDP header
 */
struct udp_hdr {
    /** source port */
    uint16_t src;
    /** destination port */
    uint16_t dst;
    /** length of data + header */
    uint16_t len;
    /** UDP checksum */
    uint16_t chksum;
};

} /* namespace udp */
} /* namespace ovsagent */

#endif /* OVSAGENT_UDP_H */
