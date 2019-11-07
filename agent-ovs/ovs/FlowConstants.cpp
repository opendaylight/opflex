/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "FlowConstants.h"
#include "ovs-shim.h"

namespace opflexagent {
namespace flow {

namespace cookie {

#define DEF_COOKIE(val) ovs_htonll((uint64_t)1 << 63 | val)
const uint64_t NEIGH_DISC      = DEF_COOKIE(3);
const uint64_t DHCP_V4         = DEF_COOKIE(4);
const uint64_t DHCP_V6         = DEF_COOKIE(5);
const uint64_t VIRTUAL_IP_V4   = DEF_COOKIE(6);
const uint64_t VIRTUAL_IP_V6   = DEF_COOKIE(7);
const uint64_t ICMP_ERROR_V4   = DEF_COOKIE(8);
const uint64_t ICMP_ERROR_V6   = DEF_COOKIE(9);
const uint64_t ICMP_ECHO_V4    = DEF_COOKIE(10);
const uint64_t ICMP_ECHO_V6    = DEF_COOKIE(11);
#undef DEF_COOKIE

} // namespace cookie

namespace meta {

const uint64_t POLICY_APPLIED = 0x100;
const uint64_t FROM_SERVICE_INTERFACE = 0x200;
const uint64_t ROUTED = 0x400;
const uint64_t DROP_LOG = 0x800;

namespace out {

const uint64_t MASK = 0xff;
const uint64_t RESUBMIT_DST = 0x1;
const uint64_t NAT = 0x2;
const uint64_t REV_NAT = 0x3;
const uint64_t TUNNEL = 0x4;
const uint64_t FLOOD = 0x5;
const uint64_t REMOTE_TUNNEL = 0x7;
const uint64_t HOST_ACCESS = 0x8;

} // namespace out

namespace access {

const uint64_t MASK = 0xff;
const uint64_t POP_VLAN = 0x1;
const uint64_t PUSH_VLAN = 0x1;

} // namespace access

} // namespace meta

} // namespace flow
} // namespace opflexagent
