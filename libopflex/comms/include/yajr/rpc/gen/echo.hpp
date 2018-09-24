/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _________COMMS__INCLUDE__YAJR__RPC__GEN__ECHO_HPP
#define _________COMMS__INCLUDE__YAJR__RPC__GEN__ECHO_HPP

#include <yajr/internal/comms.hpp>

namespace yajr {
    namespace comms {
        namespace internal {

class EchoGen {
  public:
    explicit EchoGen(CommunicationPeer const & peer) : peer_(peer) {}

    bool operator()(rpc::SendHandler & handler);

#ifdef EXTRA_CHECKS
    static char const canary[];
    static size_t const kNcanaries;
#endif

  private:
    CommunicationPeer const & peer_;
};

} // namespace internal
} // namespace comms
} // namespace yajr

#endif /* _________COMMS__INCLUDE__YAJR__RPC__GEN__ECHO_HPP */
