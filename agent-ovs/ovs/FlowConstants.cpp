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
const uint64_t PROACTIVE_LEARN = DEF_COOKIE(1);
const uint64_t LEARN           = DEF_COOKIE(2);
const uint64_t NEIGH_DISC      = DEF_COOKIE(3);
const uint64_t DHCP_V4         = DEF_COOKIE(4);
const uint64_t DHCP_V6         = DEF_COOKIE(5);
const uint64_t VIRTUAL_IP_V4   = DEF_COOKIE(6);
const uint64_t VIRTUAL_IP_V6   = DEF_COOKIE(7);
const uint64_t ICMP_ERROR_V4   = DEF_COOKIE(8);
const uint64_t ICMP_ERROR_V6   = DEF_COOKIE(9);
#undef DEF_COOKIE

} // namespace cookie

namespace meta {

const uint64_t POLICY_APPLIED = 0x100;
const uint64_t FROM_SERVICE_INTERFACE = 0x200;
const uint64_t ROUTED = 0x400;

namespace out {

const uint64_t MASK = 0xff;
const uint64_t RESUBMIT_DST = 0x1;
const uint64_t NAT = 0x2;
const uint64_t REV_NAT = 0x3;
const uint64_t TUNNEL = 0x4;
const uint64_t FLOOD = 0x5;

} // namespace out
} // namespace metadata

} // namespace flow
} // namespace opflexagent
