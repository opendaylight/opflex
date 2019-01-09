/*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#pragma once
#ifndef _COMMS__INCLUDE__OPFLEX__RPC__INTERNAL__RPC_FNV_1A_64_HPP
#define _COMMS__INCLUDE__OPFLEX__RPC__INTERNAL__RPC_FNV_1A_64_HPP

#include <cstdint>

namespace yajr {
    namespace rpc {
        namespace internal {
            namespace fnv_1a_64 {

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// hash = FNV_offset_basis
//    for each byte_of_data to be hashed
//   	hash = hash XOR byte_of_data
//   	hash = hash × FNV_prime
//   return hash

constexpr uint64_t fnv_prime = 1099511628211ull;
constexpr uint64_t fnv_offset_basis = 14695981039346656037ull;

constexpr uint64_t hash_const(const char *str,
                              uint64_t hash=fnv_offset_basis) {
    return (*str == 0) ? hash : hash_const(str+1, (hash ^ *str) * fnv_prime);
}

inline uint64_t hash_runtime(const char *str) {

    uint64_t hash=fnv_offset_basis;

    while (*str) {
        hash ^= *str;
        hash *= fnv_prime;
        str++;
    }
    return hash;
}
} /* yajr::rpc::internal:fnv_1a_64 namespace */
} /* yajr::rpc::internal namespace */
} /* yajr::rpc namespace */
} /* yajr namespace */


#endif /* _COMMS__INCLUDE__OPFLEX__RPC__INTERNAL__RPC_FNV_1A_64_HPP */

