/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _COMMS__INCLUDE__OPFLEX__RPC__INTERNAL__RPC_META_HPP
#define _COMMS__INCLUDE__OPFLEX__RPC__INTERNAL__RPC_META_HPP

#include <boost/mpl/string.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/size_t.hpp>
#include <boost/functional/hash/hash.hpp>

namespace yajr {
    namespace rpc {
        namespace internal {
            namespace meta {

template <typename Seed, typename Value>
struct hash_combine
{
  typedef boost::mpl::size_t<
    Seed::value ^ (static_cast<std::size_t>(Value::value)
      + 0x9e3779b9 + (Seed::value << 6) + (Seed::value >> 2))
  > type;
};

/* Hash any sequence of integral wrapper types */
template <typename Sequence>
struct hash_sequence
  : boost::mpl::fold<
        Sequence
      , boost::mpl::size_t<0>
      , hash_combine<boost::mpl::_1, boost::mpl::_2>
    >::type
{};

/* For hashing std::strings et al that don't include the zero-terminator */
template <typename String>
struct hash_string
  : hash_sequence<String>
{};

/* Hash including terminating zero for char arrays */
template <typename String>
struct hash_cstring
  : hash_combine<
        hash_sequence<String>
      , boost::mpl::size_t<0>
    >::type
{};

} /* yajr::rpc::internal::meta namespace */
} /* yajr::rpc::internal namespace */
} /* yajr::rpc namespace */
} /* yajr namespace */

#endif /* _COMMS__INCLUDE__OPFLEX__RPC__INTERNAL__RPC_META_HPP */

