/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <yajr/rpc/internal/meta.hpp>
#include <yajr/rpc/methods.hpp>
#include <yajr/rpc/rpc.hpp>

namespace meta = yajr::rpc::internal::meta;

namespace yajr {
    namespace rpc {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"

yajr::rpc::InboundResult *
MessageFactory::InboundResult(
        yajr::Peer const & peer,
        rapidjson::Value const & result,
        rapidjson::Value const & id) {

    char const * method = id[rapidjson::SizeType(0)].GetString();

    LOG(DEBUG)
        << " peer=" << &peer
        << " method=" <<  method
    ;

#undef  PERFECT_RET_VAL
#define PERFECT_RET_VAL(method) \
    new (std::nothrow) InbRes<&method>(peer, result, id)
#include <yajr/rpc/method_lookup.hpp>

}

#pragma GCC diagnostic pop

} /* yajr::rpc namespace */
} /* yajr namespace */

