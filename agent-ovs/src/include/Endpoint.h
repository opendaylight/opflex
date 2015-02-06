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
#include <utility>
#include <vector>

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
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
    Endpoint() : promiscuousMode(false) {}

    /**
     * Construct a new Endpoint with the given uuid.  Note that
     * endpoints in different groups can potentially have the same MAC
     * addresses, so the MAC may not be a unique key.
     *
     * @param uuid_ the unique ID for the endpoint.
     */
    explicit Endpoint(const std::string& uuid_) 
        : uuid(uuid_), promiscuousMode(false) {}

    /**
     * Get the endpoint group URI associated with this endpoint.  Note
     * that this is just the URI as discovered locally.  If the
     * endpoint group is assigned through an EPG mapping, then this
     * field will not be set to the correct URI.  Instead, @see
     * EndpointManager::getComputedEPG.
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
     * Get the mapping alias for this endpoint
     *
     * @return the mapping alias
     */
    const boost::optional<std::string>& getEgMappingAlias() const {
        return egMappingAlias;
    }

    /**
     * Set the mapping alias for this endpoint.  The mapping alias can
     * be used to find the endpoint group mapping for the endpoint
     *
     * @param egMappingAlias name of the mapping alias
     */
    void setEgMappingAlias(const std::string& egMappingAlias) {
        this->egMappingAlias = egMappingAlias;
    }

    /**
     * Unset the eg mapping alias
     */
    void unsetEgMappingAlias() {
        egMappingAlias = boost::none;
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
     * @param promiscuousMode the new value for the promiscuous mode
     */
    void setPromiscuousMode(bool promiscuousMode) {
        this->promiscuousMode = promiscuousMode;
    }

    /**
     * Get the value of the promiscuous mode flag
     *
     * @return true if promiscuous mode is on
     */
    bool isPromiscuousMode() const {
        return promiscuousMode;
    }

    /**
     * Clear the attribute map
     */
    void clearAttributes() {
        attributes.clear();
    }

    /**
     * Add an attribute to the attribute map
     *
     * @param name the name of the attribute to set
     * @param value the new value for the attribute
     */
    void addAttribute(const std::string& name, const std::string& value) {
        attributes[name] = value;
    }

    /**
     * A string to string mapping
     */
    typedef boost::unordered_map<std::string, std::string> attr_map_t;

    /**
     * Get a reference to a map of name/value attributes
     *
     * @return a map of name/value attribute pairs
     */
    const attr_map_t& getAttributes() const {
        return attributes;
    }

    /**
     * Base class for DHCP configuration
     */
    class DHCPConfig {
    public:
        /**
         * Clear the list of dns servers
         */
        void clearDnsServers() {
            dnsServers.clear();
        }

        /**
         * Add a DNS server to be returned to the endpoint using DHCP
         *
         * @param dnsServerIp the IP address for the dns server
         */
        void addDnsServer(const std::string& dnsServerIp) {
            dnsServers.push_back(dnsServerIp);
        }

        /**
         * Get the list of DNS servers for this dhcp config
         *
         * @return a vector of DNS server IP addresses
         */
        const std::vector<std::string>& getDnsServers() const {
            return dnsServers;
        }
    private:
        std::vector<std::string> dnsServers;
    };

    /**
     * Represent a configuration to be passed to the endpoint through
     * the virtual distributed DHCPv4 server.
     */
    class DHCPv4Config : public DHCPConfig {
    public:
        /**
         * Get the IP address to return for DHCP requests from this
         * endpoint.  Must be one of the IP addresses listed in the IP
         * address section for the endpoint.
         *
         * @return the IP address
         */
        const boost::optional<std::string>& getIpAddress() const {
            return ipAddress;
        }

        /**
         * Set the IP address to return for DHCP requests from this
         * endpoint
         *
         * @param ipAddress the IP address
         */
        void setIpAddress(const std::string& ipAddress) {
            this->ipAddress = ipAddress;
        }

        /**
         * Get the subnet prefix length to return for DHCP requests
         * from this endpoint.
         *
         * @return the prefix length
         */
        const boost::optional<uint8_t> getPrefixLen() const {
            return prefixLen;
        }

        /**
         * Set the subnet prefix length to return for DHCP requests
         * from this endpoint
         *
         * @param prefixLen the prefix length
         */
        void setPrefixLen(uint8_t prefixLen) {
            this->prefixLen = prefixLen;
        }

        /**
         * Clear the list of routers
         */
        void clearRouters() {
            routers.clear();
        }

        /**
         * Add a router to be returned to the endpoint using DHCP
         *
         * @param routerIp the IP address for the router
         */
        void addRouter(const std::string& routerIp) {
            routers.push_back(routerIp);
        }

        /**
         * Get the list of routers for this dhcp config
         *
         * @return a vector of router IP addresses
         */
        const std::vector<std::string>& getRouters() const {
            return routers;
        }

        /**
         * Get the domain to return for DHCP requests from this
         * endpoint.
         *
         * @return the domain name
         */
        const boost::optional<std::string>& getDomain() const {
            return domain;
        }

        /**
         * Set the domain to return for DHCP requests from this
         * endpoint
         *
         * @param domain the domain name
         */
        void setDomain(const std::string& domain) {
            this->domain = domain;
        }

        /**
         * A static route to be returned to the endpoint using DHCP.
         * The first element of the pair is the IP address destination
         * and the second element is the next hop in the path.
         */
        class static_route_t {
        public:
            /**
             * Construct a new static route
             */
            static_route_t(const std::string& dest_,
                           uint8_t prefixLen_,
                           const std::string& nextHop_)
                : dest(dest_), prefixLen(prefixLen_), nextHop(nextHop_) {}

            /**
             * The destination IP or network address
             */
            std::string dest;

            /**
             * The prefix length for the subnet
             */
            uint8_t prefixLen;

            /**
             * The IP address for the next hop
             */
            std::string nextHop;
        };

        /**
         * Clear the list of static routes
         */
        void clearStaticRoutes() {
            staticRoutes.clear();
        }

        /**
         * Add a static route to return to the endpoint using DHCP
         *
         * @param dest the destination network
         * @param prefixLen the prefix length for the route
         * @param nextHop the next hop IP for the route
         */
        void addStaticRoute(const std::string& dest,
                            uint8_t prefixLen,
                            const std::string& nextHop) {
            staticRoutes.push_back(static_route_t(dest, prefixLen, nextHop));
        }

        /**
         * Get the list of static routes to return to the endpoint
         * using DHCP.
         *
         * @return a vector of static routes
         */
        const std::vector<static_route_t>& getStaticRoutes() const {
            return staticRoutes;
        }

    private:
        boost::optional<std::string> ipAddress;
        boost::optional<uint8_t> prefixLen;
        std::vector<std::string> routers;
        boost::optional<std::string> domain;
        std::vector<static_route_t> staticRoutes;
    };

    /**
     * Represent a configuration to be passed to the endpoint through
     * the virtual distributed DHCPv6 server.
     */
    class DHCPv6Config : public DHCPConfig {
    public:
        /**
         * Clear the DNS search list
         */
        void clearSearchList() {
            searchList.clear();
        }

        /**
         * Add a DNS search list entry to be returned to the endpoint
         * using DHCP
         *
         * @param searchListEntry the IP address for the dns server
         */
        void addSearchListEntry(const std::string& searchListEntry) {
            searchList.push_back(searchListEntry);
        }

        /**
         * Get the list of search domains
         *
         * @return a vector of hostnames
         */
        const std::vector<std::string>& getSearchList() const {
            return searchList;
        }

    private:
        std::vector<std::string> searchList;
    };

    /**
     * Add a DHCPv4 configuration object.
     *
     * @param dhcpConfig the DHCP config to add
     */
    void setDHCPv4Config(const DHCPv4Config& dhcpConfig) {
        this->dhcpv4Config = dhcpConfig;
    }

    /**
     * Get the DHCPv4 configuration
     *
     * @return the DHCPv4Config object
     */
    const boost::optional<DHCPv4Config>& getDHCPv4Config() const {
        return dhcpv4Config;
    }

    /**
     * Add a DHCPv6 configuration object.
     *
     * @param dhcpConfig the DHCP config to add
     */
    void setDHCPv6Config(const DHCPv6Config& dhcpConfig) {
        this->dhcpv6Config = dhcpConfig;
    }

    /**
     * Get the DHCPv6 configuration
     *
     * @return the DHCPv6Config object
     */
    const boost::optional<DHCPv6Config>& getDHCPv6Config() const {
        return dhcpv6Config;
    }

private:
    std::string uuid;
    boost::optional<opflex::modb::MAC> mac;
    boost::unordered_set<std::string> ips;
    boost::optional<std::string> egMappingAlias;
    boost::optional<opflex::modb::URI> egURI;
    boost::optional<std::string> interfaceName;
    bool promiscuousMode;
    attr_map_t attributes;
    boost::optional<DHCPv4Config> dhcpv4Config;
    boost::optional<DHCPv6Config> dhcpv6Config;
};

/**
 * Print an endpoint to an ostream
 */
std::ostream & operator<<(std::ostream &os, const Endpoint& ep);

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINT_H */
