/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for StitchedModeRenderer
 *
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "Renderer.h"
#include "SwitchConnection.h"
#include "IntFlowManager.h"
#include "AccessFlowManager.h"
#include "FlowReader.h"
#include "FlowExecutor.h"
#include "PortMapper.h"
#include "InterfaceStatsManager.h"
#include "ContractStatsManager.h"
#include "SecGrpStatsManager.h"
#include "TunnelEpManager.h"

#pragma once
#ifndef OVSAGENT_STITCHEDMODERENDERER_H
#define OVSAGENT_STITCHEDMODERENDERER_H

namespace ovsagent {

/**
 * The stitched-mode renderer will enforce policy between devices that
 * are local to an OVS bridge, but relies on an underlying fabric to
 * enforce policy between devices that are on different bridges.  This
 * renderer allows integration with a hardware fabric such as ACI.
 */
class StitchedModeRenderer : public Renderer {
public:
    /**
     * Instantiate a stitched-mode renderer
     *
     * @param agent the agent object
     */
    StitchedModeRenderer(Agent& agent);

    /**
     * Destroy the renderer and clean up all state
     */
    virtual ~StitchedModeRenderer();

    /**
     * Allocate a new renderer on the heap
     *
     * @return a pointer to the renderer.  The memory is owned by the
     * caller
     */
    static Renderer* create(Agent& agent);

    // ********
    // Renderer
    // ********

    virtual void setProperties(const boost::property_tree::ptree& properties);
    virtual void start();
    virtual void stop();

private:
    IdGenerator idGen;
    CtZoneManager ctZoneManager;

    FlowExecutor intFlowExecutor;
    FlowReader intFlowReader;
    PortMapper intPortMapper;
    SwitchManager intSwitchManager;
    IntFlowManager intFlowManager;

    FlowExecutor accessFlowExecutor;
    FlowReader accessFlowReader;
    PortMapper accessPortMapper;
    SwitchManager accessSwitchManager;
    AccessFlowManager accessFlowManager;

    InterfaceStatsManager interfaceStatsManager;
    ContractStatsManager contractStatsManager;
    SecGrpStatsManager secGrpStatsManager;
    TunnelEpManager tunnelEpManager;

    std::string intBridgeName;
    std::string accessBridgeName;
    IntFlowManager::EncapType encapType;
    std::string encapIface;
    std::string tunnelRemoteIp;
    uint16_t tunnelRemotePort;
    std::string uplinkIface;
    uint16_t uplinkVlan;
    bool virtualRouter;
    std::string virtualRouterMac;
    bool routerAdv;
    AdvertManager::EndpointAdvMode endpointAdvMode;
    bool virtualDHCP;
    std::string virtualDHCPMac;
    std::string flowIdCache;
    std::string mcastGroupFile;
    bool connTrack;
    uint16_t ctZoneRangeStart;
    uint16_t ctZoneRangeEnd;

    bool started;

    /**
     * Timer callback to clean up IDs that have been erased
     */
    void onCleanupTimer(const boost::system::error_code& ec);
    std::unique_ptr<boost::asio::deadline_timer> cleanupTimer;
};

} /* namespace ovsagent */

#endif /* OVSAGENT_STITCHEDMODERENDERER_H */
