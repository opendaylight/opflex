/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for snat
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
     * Default constructor for Snat
     */
    Snat()
    : local(false), destIp(boost::none), destPrefix(0), zone(0) {}

    /**
     * Get the UUID for this snat
     * @return the unique ID for the snat
     */
    const std::string& getUUID() const {
        return uuid;
    }

    /**
     * Set the UUID for the snat
     *
     * @param uuid the unique ID for the snat
     */
    void setUUID(const std::string& uuid) {
        this->uuid = uuid;
    }

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
     * Set the local flag to the value specified
     *
     * @param local value true or false
     */
    void setLocal(bool local) {
        this->local = local;
    }

    /**
     * Is this snat local or remote
     *
     * @return true if snat is local
     */
    bool isLocal() const {
        return local;
    }

    /**
     * The name of the local interface to which this endpoint is
     * attached.  If the endpoint is remote or unattached, this will
     * be boost::none.
     *
     * @return the interface name or boost::none if no interface name
     * is set.
     */
    const std::string& getInterfaceName() const {
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
     * Get the MAC address for snat interface
     *
     * @return the snat interface mac address
     */
    const boost::optional<opflex::modb::MAC>& getInterfaceMAC() const {
        return interfaceMac;
    }

    /**
     * Set the interface MAC address for snat
     *
     * @param interfaceMac the MAC address
     */
    void setInterfaceMAC(const opflex::modb::MAC& interfaceMac) {
        this->interfaceMac = interfaceMac;
    }

    /**
     * Unset the interface MAC address for snat
     */
    void unsetInterfaceMAC() {
        interfaceMac = boost::none;
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
     * A vector of port ranges
     */
    typedef std::vector<port_range_t> PortRanges;

    /**
     * Map of mac address to port-ranges
     */
    typedef std::unordered_map<std::string, PortRanges> PortRangeMap;

    /**
     * Add range of ports for translation
     *
     * @param mac the mac address for remote node or "local"
     * @param start the start for the range
     * @param end the end for the range
     */
    void addPortRange(const std::string& mac, uint16_t start, uint16_t end) {
        portRangeMap[mac].push_back(port_range_t(start, end));
    }

    /**
     * Get the list of port ranges
     *
     * @return a vector of port ranges
     */
    boost::optional<PortRanges> getPortRanges(const std::string& mac) const {
        auto it = portRangeMap.find(mac);
        if (it != portRangeMap.end())
            return it->second;
        else
            return boost::none;
    }

    /**
     * Get port range map
     *
     * @return portRangeMap for this snat-ip
     */
    PortRangeMap getPortRangeMap() const {
        return portRangeMap;
    }

    /**
     * Clear the list of port ranges
     */
    void clearPortRanges() {
        portRangeMap.clear();
    }

private:
    std::string uuid;
    std::string snatIp;
    std::string interfaceName;
    bool local;
    boost::optional<opflex::modb::MAC> interfaceMac;
    boost::optional<uint16_t> ifaceVlan;
    boost::optional<std::string> destIp;
    boost::optional<uint8_t> destPrefix;
    boost::optional<uint16_t> zone;
    PortRangeMap portRangeMap;
};

/**
 * Print an to an ostream
 */
std::ostream & operator<<(std::ostream &os, const Snat& snat);

/**
 * Check if two port ranges are equal
 */
bool operator==(const Snat::port_range_t& lhs,
                const Snat::port_range_t& rhs);

/**
 * Check if two port ranges are not equal
 */
bool operator!=(const Snat::port_range_t& lhs,
                const Snat::port_range_t& rhs);

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SNAT_H */
