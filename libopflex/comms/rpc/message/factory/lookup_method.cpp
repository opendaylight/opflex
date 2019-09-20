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


#include <yajr/rpc/internal/fnv_1a_64.hpp>
#include <yajr/rpc/methods.hpp>

namespace fnv_1a_64 = yajr::rpc::internal::fnv_1a_64;

namespace yajr {
    namespace rpc {

MethodName const *
MessageFactory::lookupMethod(char const * method) {
    VLOG(6)
        << method
    ;
#undef  PERFECT_RET_VAL
#define PERFECT_RET_VAL(method) \
    &method
#include <yajr/rpc/method_lookup.hpp>
}

} /* yajr::rpc namespace */
} /* yajr namespace */

