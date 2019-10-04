/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for OVSRenderer
 *
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/Renderer.h>
#include "SwitchConnection.h"
#include "IntFlowManager.h"
#include "AccessFlowManager.h"
#include "FlowReader.h"
#include "FlowExecutor.h"
#include "PortMapper.h"
#include "InterfaceStatsManager.h"
#include "ContractStatsManager.h"
#include "SecGrpStatsManager.h"
#include <opflexagent/TunnelEpManager.h>
#include "PacketInHandler.h"
#include "CtZoneManager.h"
#include "SpanRenderer.h"

#pragma once
#ifndef OPFLEXAGENT_OVSRENDERER_H
#define OPFLEXAGENT_OVSRENDERER_H

namespace opflexagent {

/**
 * The stitched-mode renderer will enforce policy between devices that
 * are local to an OVS bridge, but relies on an underlying fabric to
 * enforce policy between devices that are on different bridges.  This
 * renderer allows integration with a hardware fabric such as ACI.
 */
class OVSRenderer : public Renderer {
public:
    /**
     * Instantiate a stitched-mode renderer
     *
     * @param agent the agent object
     */
    OVSRenderer(Agent& agent);

    /**
     * Destroy the renderer and clean up all state
     */
    virtual ~OVSRenderer();

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
    TunnelEpManager tunnelEpManager;
    IntFlowManager intFlowManager;

    FlowExecutor accessFlowExecutor;
    FlowReader accessFlowReader;
    PortMapper accessPortMapper;
    SwitchManager accessSwitchManager;
    AccessFlowManager accessFlowManager;

    PacketInHandler pktInHandler;

    InterfaceStatsManager interfaceStatsManager;
    ContractStatsManager contractStatsManager;
    SecGrpStatsManager secGrpStatsManager;

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
    AdvertManager::EndpointAdvMode tunnelEndpointAdvMode;
    uint64_t tunnelEndpointAdvIntvl;
    bool virtualDHCP;
    std::string virtualDHCPMac;
    std::string flowIdCache;
    std::string mcastGroupFile;
    bool connTrack;
    uint16_t ctZoneRangeStart;
    uint16_t ctZoneRangeEnd;

    bool ifaceStatsEnabled;
    long ifaceStatsInterval;
    bool contractStatsEnabled;
    long contractStatsInterval;
    bool secGroupStatsEnabled;
    long secGroupStatsInterval;

    SpanRenderer spanRenderer;

    bool started;
    std::string dropLogIntIface, dropLogAccessIface, dropLogRemoteIp;
    uint16_t dropLogRemotePort;

    /**
     * Timer callback to clean up IDs that have been erased
     */
    void onCleanupTimer(const boost::system::error_code& ec);
    std::unique_ptr<boost::asio::deadline_timer> cleanupTimer;
};

/**
 * Plugin implementation for dynamically loading stitched-mode
 * renderer.
 */
class OVSRendererPlugin : public RendererPlugin {
public:
    OVSRendererPlugin();

    // **************
    // RendererPlugin
    // **************
    virtual std::unordered_set<std::string> getNames() const;
    virtual Renderer* create(Agent& agent) const;
};

} /* namespace opflexagent */

/**
 * Return a non-owning pointer to the renderer plugin instance.
 */
extern "C" const opflexagent::RendererPlugin* init_renderer_plugin();

#endif /* OPFLEXAGENT_OVSRENDERER_H */
