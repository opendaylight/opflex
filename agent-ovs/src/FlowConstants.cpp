/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "FlowConstants.h"
#include "ovs.h"

namespace ovsagent {
namespace flow {

namespace cookie {

#define DEF_COOKIE(val) htonll((uint64_t)1 << 63 | (uint64_t)val << 32)
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

namespace out {

const uint64_t MASK = 0xff;
const uint64_t RESUBMIT_DST = 0x1;
const uint64_t NAT = 0x2;
const uint64_t REV_NAT = 0x3;
const uint64_t TUNNEL = 0x4;
const uint64_t FLOOD = 0x5;

} // namespace out
} // namespace metadata

namespace id {

// Note that if you add to this, you must also add cleanup routine in
// StitchedModeRender
const std::string NAMESPACES[] =
    {"endpointGroup", "floodDomain", "bridgeDomain", "routingDomain",
     "contract", "externalNetwork", "subnet", "secGroup", "secGroupSet",
     "endpoint", "anycastService"};
const size_t NUM_NAMESPACES = sizeof(NAMESPACES)/sizeof(std::string);

const std::string EPG          = NAMESPACES[0];
const std::string FD           = NAMESPACES[1];
const std::string BD           = NAMESPACES[2];
const std::string RD           = NAMESPACES[3];
const std::string CONTRACT     = NAMESPACES[4];
const std::string EXTNET       = NAMESPACES[5];
const std::string SUBNET       = NAMESPACES[6];
const std::string SECGROUP     = NAMESPACES[7];
const std::string SECGROUP_SET = NAMESPACES[8];
const std::string ENDPOINT     = NAMESPACES[9];
const std::string SERVICE      = NAMESPACES[10];

} // namespace id

} // namespace flow
} // namespace ovsagent
