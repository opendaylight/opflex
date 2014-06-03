/* Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (c) 2010, 2011, 2013 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef POLICYAGENT_TYPES_H
#define POLICYAGENT_TYPES_H 1

#include <linux/types.h>
#include <sys/types.h>
#include <stdint.h>

#ifdef __CHECKER__
#define PAG_BITWISE __attribute__((bitwise))
#define PAG_FORCE __attribute__((force))
#else
#define PAG_BITWISE
#define PAG_FORCE
#endif

/* The pag_be<N> types indicate that an object is in big-endian, not
 * native-endian, byte order.  They are otherwise equivalent to uint<N>_t.
 *
 * We bootstrap these from the Linux __be<N> types.  If we instead define our
 * own independently then __be<N> and pag_be<N> become mutually
 * incompatible. */
typedef __be16 pag_be16;
typedef __be32 pag_be32;
typedef __be64 pag_be64;
typedef __u32 HANDLE;

#define PAG_BE16_MAX ((PAG_FORCE pag_be16) 0xffff)
#define PAG_BE32_MAX ((PAG_FORCE pag_be32) 0xffffffff)
#define PAG_BE64_MAX ((PAG_FORCE pag_be64) 0xffffffffffffffffULL)

/* These types help with a few funny situations:
 *
 *   - The Ethernet header is 14 bytes long, which misaligns everything after
 *     that.  One can put 2 "shim" bytes before the Ethernet header, but this
 *     helps only if there is exactly one Ethernet header.  If there are two,
 *     as with GRE and VXLAN (and if the inner header doesn't use this
 *     trick--GRE and VXLAN don't) then you have the choice of aligning the
 *     inner data or the outer data.  So it seems better to treat 32-bit fields
 *     in protocol headers as aligned only on 16-bit boundaries.
 *
 *   - ARP headers contain misaligned 32-bit fields.
 *
 *   - Netlink and OpenFlow contain 64-bit values that are only guaranteed to
 *     be aligned on 32-bit boundaries.
 *
 * lib/unaligned.h has helper functions for accessing these. */

/* A 32-bit value, in host byte order, that is only aligned on a 16-bit
 * boundary.  */
typedef struct {
#ifdef WORDS_BIGENDIAN
        uint16_t hi, lo;
#else
        uint16_t lo, hi;
#endif
} pag_16aligned_u32;

/* A 32-bit value, in network byte order, that is only aligned on a 16-bit
 * boundary. */
typedef struct {
        pag_be16 hi, lo;
} pag_16aligned_be32;

/* A 64-bit value, in host byte order, that is only aligned on a 32-bit
 * boundary.  */
typedef struct {
#ifdef WORDS_BIGENDIAN
        uint32_t hi, lo;
#else
        uint32_t lo, hi;
#endif
} pag_32aligned_u64;

/* A 64-bit value, in network byte order, that is only aligned on a 32-bit
 * boundary. */
typedef struct {
        pag_be32 hi, lo;
} pag_32aligned_be64;

/* ofp_port_t represents the port number of a OpenFlow switch.
 * odp_port_t represents the port number on the datapath.
 * ofp11_port_t represents the OpenFlow-1.1 port number. */
typedef uint16_t PAG_BITWISE ofp_port_t;
typedef uint32_t PAG_BITWISE odp_port_t;
typedef uint32_t PAG_BITWISE ofp11_port_t;

/* Macro functions that cast int types to ofp/odp/ofp11 types. */
#define OFP_PORT_C(X) ((PAG_FORCE ofp_port_t) (X))
#define ODP_PORT_C(X) ((PAG_FORCE odp_port_t) (X))
#define OFP11_PORT_C(X) ((PAG_FORCE ofp11_port_t) (X))

#endif /* openvswitch/types.h */
