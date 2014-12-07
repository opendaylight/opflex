/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>

#include <boost/unordered_set.hpp>
#include <boost/optional.hpp>
#include <opflex/modb/URI.h>
#include <opflex/modb/MAC.h>

#pragma once
#ifndef OVSAGENT_ENDPOINT_H
#define OVSAGENT_ENDPOINT_H

namespace ovsagent {

/**
 * A class for representing an endpoint discovered on the local
 * system.
 */
class Endpoint {
public:
    /**
     * Default constructor for containers
     */
    Endpoint() {}

    /**
     * Construct a new Endpoint with the given uuid.  Note that
     * endpoints in different groups can potentially have the same MAC
     * addresses, so the MAC may not be a unique key.
     *
     * @param uuid_ the unique ID for the endpoint.
     */
    explicit Endpoint(const std::string& uuid_) 
        : uuid(uuid_) {}

    /**
     * Get the endpoint group URI associated with this endpoint.
     *
     * @return the endpoint URI
     */
    const boost::optional<opflex::modb::URI>& getEgURI() const {
        return egURI;
    }

    /**
     * Set the endpoint group URI associated with this endpoint.  The
     * endpoint group URI controls the policies that are applied to
     * the endpoint.
     *
     * @param egURI the URI to set
     */
    void setEgURI(const opflex::modb::URI& egURI) {
        this->egURI = egURI;
    }

    /**
     * Unset the endpoint group URI
     */
    void unsetEgURI() {
        egURI = boost::none;
    }

    /**
     * Get the list of IP addresses associated with this endpoint
     *
     * @return the list of IP addresses
     */
    const boost::unordered_set<std::string>& getIPs() const {
        return ips;
    }

    /**
     * Set the IP addresses for this endpoint.  This will overwrite
     * any existing IP addresses
     *
     * @param ips the IP addresses
     */
    void setIPs(const boost::unordered_set<std::string>& ips) {
        this->ips = ips;
    }

    /**
     * Add an IP address to the list of IPs
     *
     * @param ip the IP address to add
     */
    void addIP(const std::string& ip) {
        this->ips.insert(ip);
    }

    /**
     * Get the MAC address for this endpoint
     *
     * @return the MAC address
     */
    const boost::optional<opflex::modb::MAC>& getMAC() const {
        return mac;
    }

    /**
     * Set the MAC address for the endpoint
     *
     * @param mac the MAC address
     */
    void setMAC(const opflex::modb::MAC& mac) {
        this->mac = mac;
    }

    /**
     * Unset the MAC address for the endpoint
     */
    void unsetMAC() {
        mac = boost::none;
    }

    /**
     * Get the UUID for this endpoint
     * @return the unique ID for the endpoint.
     */
    const std::string& getUUID() const {
        return uuid;
    }

    /**
     * Set the UUID for the endpoint
     *
     * @param uuid the unique ID for the endpoint
     */
    void setUUID(const std::string& uuid) {
        this->uuid = uuid;
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
     * @param interfaceName the interface name to which the endpoint
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
     * Set the promomiscuous mode flag to the value specified
     *
     * @param promisciousMode the new value for the promiscuous mode
     */
    void setPromiscuousMode(bool promiscuousMode) {
        this->promiscuousMode = promiscuousMode;
    }

    /**
     * Get the value of the promiscuous mode flag
     * @param promiscu
     */
    bool isPromiscuousMode() const {
        return promiscuousMode;
    }

private:
    std::string uuid;
    boost::optional<opflex::modb::MAC> mac;
    boost::unordered_set<std::string> ips;
    boost::optional<opflex::modb::URI> egURI;
    boost::optional<std::string> interfaceName;
    bool promiscuousMode;
};

/**
 * Print an endpoint to an ostream
 */
std::ostream & operator<<(std::ostream &os, const Endpoint& ep);

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINT_H */
