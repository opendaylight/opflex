/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <yajr/rpc/methods.hpp>

namespace yajr { namespace rpc {

template<>
void InbReq<&yajr::rpc::method::echo>::process() const {

    LOG(DEBUG);

    /* TODO: provide a factory? */
    OutboundResponse * resp = new OutboundResult(
            *getPeer(),  /* reply to the sender! */
            GeneratorFromValue(getPayload()),/* payload from inbound req */
            getRemoteId()/* id from the inbound req */
        );

    resp->send();

    /* lifecycle of the response is handled by the framework */
}

}}
