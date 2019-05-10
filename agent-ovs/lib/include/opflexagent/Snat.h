/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for an anycast or load balanced snat
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <utility>
#include <unordered_set>
#include <set>

#include <boost/optional.hpp>
#include <opflex/modb/URI.h>
#include <opflex/modb/MAC.h>

#pragma once
#ifndef OPFLEXAGENT_SNAT_H
#define OPFLEXAGENT_SNAT_H

namespace opflexagent {

/**
 * A class for an anycast or load balanced snat hosted on the local
 * system
 */
class Snat {
public:
    /**
     * Construct a new Snat with the given snatIp
     *
     * @param snatIp for this mapping
     */
    Snat()
    : destIp(boost::none), destPrefix(0), zone(0) {}

    /**
     * Get SNAT IP address
     *
     * @return the SNAT IP address
     */
    const std::string& getSnatIP() const {
        return snatIp;
    }

    /**
     * Set SNAT IP address
     *
     * @param snatIp the mapped IP address
     */
    void setSnatIP(const std::string& snatIp) {
        this->snatIp = snatIp;
    }

    /**
     * The name of the local interface to which this endpoint is
     * attached.  If the endpoint is remote or unattached, this will
     * be boost::none.
     *
     * @return the interface name or boost::none if no interface name
     * is set.
     */
    const boost::optional<std::string>& getInterfaceName() const {
        return interfaceName;
    }

    /**
     * Set the local interface name to the string specified.
     *
     * @param interfaceName the interface name to which the snat
     * is attached locally
     */
    void setInterfaceName(const std::string& interfaceName) {
        this->interfaceName = interfaceName;
    }

    /**
     * Unset the local interface name
     */
    void unsetInterfaceName() {
        interfaceName = boost::none;
    }

    /**
     * VLAN trunk tag for use on the local interface
     *
     * @return the vlan tag or boost::none if no vlan tag is set
     */
    const boost::optional<uint16_t>& getIfaceVlan() const {
        return ifaceVlan;
    }

    /**
     * Set the VLAN trunk tag for use on the interface
     *
     * @param ifaceVlan the vlan tag
     */
    void setIfaceVlan(uint16_t ifaceVlan) {
        this->ifaceVlan = ifaceVlan;
    }

    /**
     * Unset the interface vlan trunk tag.
     */
    void unsetIfaceVlan() {
        ifaceVlan = boost::none;
    }

    /**
     * Get destination network that should be SNATTED
     *
     * @return the destination network for which SNAT
     * action should be performed
     */
    const boost::optional<std::string>& getDest() const {
        return destIp;
    }

    /**
     * Set destination network that should be SNATTED
     */
    void setDest(const std::string&  destIp) {
        this->destIp = destIp;
    }

    /**
     * Unset the destination network
     */
    void unsetDest() {
        destIp = boost::none;
    }

    /**
     * Get destination prefix for destination network
     *
     * @return prefix
     */
    const boost::optional<uint8_t> getDestPrefix() const {
         return destPrefix;
    }

    /**
     * Set destination prefix for destination network
     */
    void setDestPrefix(uint8_t destPrefix) {
        this->destPrefix = destPrefix;
    }

    /**
     * Unset destination prefix
     */
    void unsetDestPrefix() {
        destPrefix = 0;
    }

    /**
     * Get SNAT zone
     *
     * @return zone id
     */
    const boost::optional<uint16_t> getZone() const {
         return zone;
    }

    /**
     * Set SNAT zone
     */
    void setZone(uint16_t zone) {
        this->zone = zone;
    }

    /**
     * Unset SNAT zone
     */
    void unsetZone() {
        zone = 0;
    }

    /**
     * An SNAT IP can have multiple port ranges
     */
    class port_range_t {
    public:
        /**
         * Construct a new port range
         */
        port_range_t(uint16_t start_, uint16_t end_)
        : start(start_), end(end_) {}

        /**
         * Starting port
         */
        uint16_t start;

        /**
         * Last port
         */
        uint16_t end;
    };

    /**
     * Add range of ports for translation
     */
    void addPortRange(uint16_t start, uint16_t end) {
        portRanges.push_back(port_range_t(start, end));
    }

    /**
     * Get the list of port ranges
     *
     * @return a vector of port ranges
     */
    const std::vector<port_range_t>& getPortRanges() const {
        return portRanges;
    }

    /**
     * Clear the list of port ranges
     */
    void clearPortRanges() {
        portRanges.clear();
    }

private:
    std::string snatIp;
    boost::optional<std::string> interfaceName;
    boost::optional<uint16_t> ifaceVlan;
    boost::optional<std::string> destIp;
    boost::optional<uint8_t> destPrefix;
    boost::optional<uint16_t> zone;
    std::vector<port_range_t> portRanges;
};

/**
 * Print an to an ostream
 */
std::ostream & operator<<(std::ostream &os, const Snat& snat);

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SNAT_H */
