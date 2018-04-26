/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for an anycast or load balanced service
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <utility>
#include <set>

#include <boost/optional.hpp>

#pragma once
#ifndef OPFLEXAGENT_LEARNINGBRIDGEIFACE_H
#define OPFLEXAGENT_LEARNINGBRIDGEIFACE_H

namespace opflexagent {

/**
 * A class for a learning bridge interface.  A learning bridge
 * interface supercedes the normal policy functionality for any
 * interfaces and VLANs for which it is configured.
 */
class LearningBridgeIface {
public:
    /**
     * Default constructor
     */
    LearningBridgeIface() {}

    /**
     * Construct a new LearningBridgeIface with the given uuid.
     *
     * @param uuid_ the unique ID for the service.
     */
    explicit LearningBridgeIface(const std::string& uuid_)
        : uuid(uuid_) {}

    /**
     * Get the UUID for this service
     * @return the unique ID for the service.
     */
    const std::string& getUUID() const {
        return uuid;
    }

    /**
     * Set the UUID for the service
     *
     * @param uuid the unique ID for the service
     */
    void setUUID(const std::string& uuid) {
        this->uuid = uuid;
    }

    /**
     * The name of the on the integration bridge.
     *
     * @return the interface name or boost::none if no interface name
     * is set.
     */
    const boost::optional<std::string>& getInterfaceName() const {
        return interfaceName;
    }

    /**
     * Set the integration bridge interface name to the string
     * specified.
     *
     * @param interfaceName the interface name to which the endpoint
     * is attached locally
     */
    void setInterfaceName(const std::string& interfaceName) {
        this->interfaceName = interfaceName;
    }

    /**
     * Unset the integration bridge interface name
     */
    void unsetInterfaceName() {
        interfaceName = boost::none;
    }

    /**
     * A range of VLANs
     */
    typedef std::pair<uint16_t, uint16_t> vlan_range_t;

    /**
     * Get the VLAN trunk ranges for this interface.  The trunked
     * VLANs will bypass normal policy processing to be handled by the
     * learning switch, while other VLANs will use normal policy
     * processing.
     *
     * @return the set of VLAN trunk ranges
     */
    const std::set<vlan_range_t>& getTrunkVlans() const {
        return trunkVlans;
    }

    /**
     * Set the VLAN trunk ranges for this interface
     *
     * @param trunkVlans the VLAN ranges
     */
    void setTrunkVlans(const std::set<vlan_range_t>& trunkVlans) {
        this->trunkVlans = trunkVlans;
    }

    /**
     * Add a range of VLANS to the trunk ranges for this interface
     *
     * @param trunkVlans the VLAN range to add
     */
    void addTrunkVlans(const vlan_range_t& trunkVlans) {
        this->trunkVlans.insert(trunkVlans);
    }

private:
    std::string uuid;

    boost::optional<std::string> interfaceName;
    std::set<vlan_range_t> trunkVlans;
};

constexpr bool operator<(const LearningBridgeIface::vlan_range_t& lhs,
                         const LearningBridgeIface::vlan_range_t& rhs) {
    return (lhs.first < rhs.first ||
            (lhs.first == rhs.first && lhs.second < rhs.second));
}

/**
 * Print a LearningBridgeIface to an ostream
 */
std::ostream& operator<<(std::ostream&, const LearningBridgeIface&);

} /* namespace opflexagent */

namespace std {

/**
 * Print a LearningBridgeIface::vlan_range_t to an ostream
 */
std::ostream&
operator<<(std::ostream&,
           const opflexagent::LearningBridgeIface::vlan_range_t&);

} /* namespace std */

#endif /* OPFLEXAGENT_SERVICE_H */
