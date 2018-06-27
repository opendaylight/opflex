/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for learning bridge manager
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_LEARNINGBRIDGEMANAGER_H
#define OPFLEXAGENT_LEARNINGBRIDGEMANAGER_H

#include <opflexagent/LearningBridgeIface.h>
#include <opflexagent/LearningBridgeListener.h>

#include <boost/noncopyable.hpp>

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <list>
#include <memory>
#include <map>

namespace opflexagent {

/**
 * Manage learning bridge endpoints
 */
class LearningBridgeManager : private boost::noncopyable {
public:
    /**
     * Instantiate a new learning bridge manager
     */
    LearningBridgeManager();

    /**
     * Register a listener for learning bridge change events
     *
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @see LearningBridgeListener
     */
    void registerListener(LearningBridgeListener* listener);

    /**
     * Unregister the specified listener
     *
     * @param listener the listener to unregister
     * @throws std::out_of_range if there is no such class
     */
    void unregisterListener(LearningBridgeListener* listener);

    /**
     * Get the detailed information for a learning bridge interface
     *
     * @param uuid the UUID for the interface
     * @return the object containing the detailed information, or a
     * NULL pointer if there is no such interface
     */
    std::shared_ptr<const LearningBridgeIface>
        getLBIface(const std::string& uuid);

    /**
     * Get the load balancer interfaces for a given integration bridge
     * interface.
     *
     * @param ifaceName the name of the integration bridge interface
     * @param lbi a set that will be filled with the UUIDs of
     * matching load balancer interfaces
     */
    void getLBIfaceByIface(const std::string& ifaceName,
                           /* out */ std::unordered_set<std::string>& lbi);

    /**
     * A range of VLANs
     */
    typedef LearningBridgeIface::vlan_range_t vlan_range_t;

    /**
     * Get the load balancer interfaces for a given VLAN range.  Note
     * that this is an exact range lookup, so 5-9 does not match
     * e.g. 5-10 or 4-10.
     *
     * @param range the vlan range
     * @param lbi a set that will be filled with the UUIDs of
     * matching load balancer interfaces
     */
    void getIfacesByVlanRange(vlan_range_t range,
                              /* out */ std::unordered_set<std::string>& lbi);

    /**
     * Get the set of active VLAN ranges for a given interface.  Note
     * that this could be different from the trunk VLANs for the
     * interface based on what other interfaces exist in the index.
     *
     * @param uuid the interface
     * @param ranges the set of relevent VLAN ranges
     */
    void getVlanRangesByIface(const std::string& uuid,
                              /* out */ std::set<vlan_range_t> ranges);

    /**
     * Callback function for forEachVlanRange
     */
    typedef std::function<void(LearningBridgeIface::vlan_range_t,
                               const std::unordered_set<std::string>&)> vlanCb;

    /**
     * Iterate over all VLAN ranges in order.  Note that the state
     * mutex is held during this call.
     *
     * @param func the callback function
     */
    void forEachVlanRange(const vlanCb& func);

private:
    /**
     * Add or update the learning bridge state with new information about an
     * learning bridge.
     */
    void updateLBIface(const LearningBridgeIface& learningBridge);

    /**
     * Remove the learning bridge with the specified UUID from the
     * learning bridge manager.
     *
     * @param uuid the UUID of the learning bridge that no longer exists
     */
    void removeLBIface(const std::string& uuid);

    std::mutex iface_mutex;

    class LBIfaceState {
    public:
        std::shared_ptr<const LearningBridgeIface> iface;
    };

    /**
     * Comparator for VLAN ranges that considers only the left range
     * edge.
     */
    struct VlanRangeComp {
        /**
         * true if lhs.first < rhs.first
         */
        constexpr bool operator()(const vlan_range_t& lhs,
                                  const vlan_range_t& rhs) {
            return lhs.first < rhs.first;
        }
    };

    typedef std::unordered_map<std::string, LBIfaceState> lbi_map_t;
    typedef std::unordered_map<std::string,
                               std::unordered_set<std::string>> str_lbi_map_t;
    typedef std::map<vlan_range_t, std::unordered_set<std::string>,
                     VlanRangeComp> range_lbi_map_t;
    typedef std::set<vlan_range_t> range_set_t;

    /**
     * Map learning bridge iface UUID to LBIfaceState
     */
    lbi_map_t lbi_map;

    /**
     * Map interface names to a set of learning bridge iface UUIDs
     */
    str_lbi_map_t iface_lbi_map;

    /**
     * Map vlan ranges to a set of interfaces that use those ranges.
     */
    range_lbi_map_t range_lbi_map;

    /**
     * The listeners that have been registered
     */
    std::list<LearningBridgeListener*> lbListeners;
    std::mutex listener_mutex;

    void notifyListeners(const std::string& uuid);
    void notifyListeners(const range_set_t& notify);
    void removeIfaces(const LearningBridgeIface& lbi);
    void addVlans(const LearningBridgeIface& lbi, range_set_t& notify);
    void removeVlans(const LearningBridgeIface& lbi, range_set_t& notify);

    friend class LearningBridgeSource;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_LEARNINGBRIDGEMANAGER_H */
