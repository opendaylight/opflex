/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for extra config manager manager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_EXTRACONFIGMANAGER_H
#define OPFLEXAGENT_EXTRACONFIGMANAGER_H

#include <opflexagent/ExtraConfigListener.h>
#include <opflexagent/RDConfig.h>
#include <opflexagent/PacketDropLogConfig.h>
#include <opflex/ofcore/OFFramework.h>

#include <boost/noncopyable.hpp>
#include <opflex/modb/URI.h>

#include <unordered_map>
#include <mutex>

namespace opflexagent {

/**
 * Manage extra configuration objects
 */
class ExtraConfigManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new extra config manager
     */
    ExtraConfigManager(opflex::ofcore::OFFramework& framework);

    /**
     * Register a listener for config change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see ExtraConfigListener
     */
    void registerListener(ExtraConfigListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(ExtraConfigListener* listener);

    /**
     * Get the detailed information for an RD config
     *
     * @param domain the URI for the RD config
     * @return the object containing the detailed information, or a
     * NULL pointer if there is no such RD config
     */
    std::shared_ptr<const RDConfig>
        getRDConfig(const opflex::modb::URI& domain);

private:
    opflex::ofcore::OFFramework& framework;

    /**
     * Add or update a routing domain config object
     *
     * @param rdConfig the routing domain config object to update
     */
    void updateRDConfig(const RDConfig& rdConfig);

    /**
     * Remove the routing domain config with the specified URI.
     *
     * @param domainURI the URI of the routing domain
     */
    void removeRDConfig(const opflex::modb::URI& domainURI);

    class RDConfigState {
    public:
        std::shared_ptr<const RDConfig> rdConfig;
    };

    typedef std::unordered_map<opflex::modb::URI, RDConfigState> rdc_map_t;

    std::mutex ec_mutex;

    /**
     * Map domain URIs to routing domain config
     */
    rdc_map_t rdc_map;

    /**
     * Notify listeners for packet drop log config
     *
     * @param dropLogCfgURI  Drop log cfg URI
     */
    void notifyPacketDropLogConfigListeners(const opflex::modb::URI &dropLogCfgURI);

    /**
     * Notify listeners for drop flow config object
     *
     * @param dropFlowCfgURI Drop flow spec URI
     */
    void notifyPacketDropFlowConfigListeners(const opflex::modb::URI &dropFlowCfgURI);

    /**
     * Add or update a packet drop log config object
     *
     * @param dropCfg Drop log enable and mode
     */
    void packetDropLogConfigUpdated(PacketDropLogConfig &dropCfg);

    /**
     * Add or update a packet drop flow config object
     *
     * @param dropFlow Drop flow file path and spec
     */
    void packetDropFlowConfigUpdated(PacketDropFlowConfig &dropFlow);

    /**
     * The extraConfig listeners that have been registered
     */
    std::list<ExtraConfigListener*> extraConfigListeners;
    std::mutex listener_mutex;

    void notifyListeners(const opflex::modb::URI& uuid);

    friend class FSRDConfigSource;
    friend class FSPacketDropLogConfigSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_EXTRACONFIGMANAGER_H */
