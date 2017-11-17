/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "VppVirtualRouter.h"

namespace VPP {

VirtualRouter::VirtualRouter(const VOM::mac_address_t& mac) : m_mac(mac) {}

const VOM::mac_address_t& VirtualRouter::mac() const { return (m_mac); }
}

/*
 * Local Variables:
 * eval: (c-set-style "llvm.org")
 * End:
 */
