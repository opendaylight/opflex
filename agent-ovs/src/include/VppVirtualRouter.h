/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef __VPP_VIRTUALROUTER_H__
#define __VPP_VIRTUALROUTER_H__

#include <vom/types.hpp>

using namespace VOM;

namespace VPP {
/**
 * A description of the VirtualRouter.
 *  Can be one of VLAN< VXLAN or iVXLAN
 */
class VirtualRouter {
public:
    /**
     * Default Constructor
     */
    VirtualRouter(const VOM::mac_address_t& mac);
    VirtualRouter(const VirtualRouter& vr) = default;

    /**
     * Get the router virtual MAC
     */
    const VOM::mac_address_t& mac() const;

private:
    /**
     * Virtual Router Mac
     */
    VOM::mac_address_t m_mac;
};
};

/*
 * Local Variables:
 * eval: (c-set-style "llvm.org")
 * End:
 */

#endif
