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

#pragma once
#ifndef OPFLEXAGENT_ENDPOINT_H
#define OPFLEXAGENT_ENDPOINT_H

#include <opflex/modb/URI.h>
#include <opflex/modb/MAC.h>

#include <boost/optional.hpp>

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace opflexagent {

/**
 * A class for representing an endpoint discovered on the local
 * system.
 */
class Endpoint {
public:
    /**
     * Default constructor for containers
     */
    Endpoint() : promiscuousMode(false), discoveryProxyMode(false), natMode(false),
                 external(false) {}

    /**
     * Construct a new Endpoint with the given uuid.  Note that
     * endpoints in different groups can potentially have the same MAC
     * addresses, so the MAC may not be a unique key.
     *
     * @param uuid_ the unique ID for the endpoint.
     */
    explicit Endpoint(const std::string& uuid_)
        : uuid(uuid_), promiscuousMode(false), discoveryProxyMode(false), natMode(false),
          external(false) {}

    /**
     * Get the endpoint group URI associated with this endpoint.  Note
     * that this is just the URI as discovered locally.  If the
     * endpoint group is assigned through an EPG mapping, then this
     * field will not be set to the correct URI.  Instead, @see
     * EndpointManager::getComputedEPG.
     *
     * @return the endpoint group URI
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
     * Get the set of security group labels for this endpoint
     *
     * @return the list of security group labels
     */
    const std::set<opflex::modb::URI>& getSecurityGroups() const {
        return securityGroups;
    }

    /**
     * Set the security group labels for this endpoint.  This will
     * overwrite any existing labels
     *
     * @param securityGroups the set of security labels
     */
    void setSecurityGroups(const std::set<opflex::modb::URI>& securityGroups) {
        this->securityGroups = securityGroups;
    }

    /**
     * Add a security group label for this endpoint
     *
     * @param securityGroup the URI for the security group to add
     */
    void addSecurityGroup(const opflex::modb::URI& securityGroup) {
        this->securityGroups.insert(securityGroup);
    }

    /**
     * Get the list of IP addresses associated with this endpoint
     *
     * @return the list of IP addresses
     */
    const std::unordered_set<std::string>& getIPs() const {
        return ips;
    }

    /**
     * Set the IP addresses for this endpoint.  This will overwrite
     * any existing IP addresses
     *
     * @param ips the IP addresses
     */
    void setIPs(const std::unordered_set<std::string>& ips) {
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
     * Get the list of IP addresses that are valid sources for anycast
     * service addresses.
     *
     * @return the list of IP addresses
     */
    const std::unordered_set<std::string>& getAnycastReturnIPs() const {
        return anycastReturnIps;
    }

    /**
     * Set the list of IP addresses that are valid sources for anycast
     * service addresses.  This will overwrite any existing IP
     * addresses
     *
     * @param ips the IP addresses
     */
    void setAnycastReturnIPs(const std::unordered_set<std::string>& ips) {
        this->anycastReturnIps = ips;
    }

    /**
     * Add an IP address to the list of IPs that are valid sources for
     * anycast service addresses.
     *
     * @param ip the IP address to add
     */
    void addAnycastReturnIP(const std::string& ip) {
        this->anycastReturnIps.insert(ip);
    }

    /**
     * A MAC/IP address pair representing a virtual IP that can be
     * claimed by the endpoint by sending a gratuitous ARP.
     */
    typedef std::pair<opflex::modb::MAC, std::string> virt_ip_t;

    /**
     * Functor for storing an virt_ip_t as hash key
     */
    struct VirtIpTHash {
        /**
         * Hash the virt_ip_t
         */
        size_t operator()(const virt_ip_t& m) const noexcept;
    };

    /**
     * A set of virt_ip_t
     */
    typedef std::unordered_set<virt_ip_t, VirtIpTHash> virt_ip_set;

    /**
     * Get the list of virtual IP addresses associated with this endpoint
     *
     * @return the list of virtual IP addresses
     */
    const virt_ip_set& getVirtualIPs() const {
        return virtualIps;
    }

    /**
     * Set the virtual IP addresses for this endpoint.  This will overwrite
     * any existing virtual IP addresses
     *
     * @param virtualIps the virtual IP addresses
     */
    void setVirtualIPs(const virt_ip_set& virtualIps) {
        this->virtualIps = virtualIps;
    }

    /**
     * Add a virtual IP address to the list of IPs
     *
     * @param virtualIp the IP address to add
     */
    void addVirtualIP(const virt_ip_t& virtualIp) {
        this->virtualIps.insert(virtualIp);
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
     * attached on the integration bridge.  If the endpoint is remote
     * or unattached, this will be boost::none.
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
     * The name of the local interface to which this endpoint is
     * attached on the access bridge.
     *
     * @return the interface name or boost::none if no interface name
     * is set.
     */
    const boost::optional<std::string>& getAccessInterface() const {
        return accessInterface;
    }

    /**
     * Set the access interface name to the string specified.
     *
     * @param accessInterface the interface name to which the endpoint
     * is attached locally
     */
    void setAccessInterface(const std::string& accessInterface) {
        this->accessInterface = accessInterface;
    }

    /**
     * Unset the access interface name
     */
    void unsetAccessInterface() {
        accessInterface = boost::none;
    }

    /**
     * VLAN trunk tag for use on the access interface
     *
     * @return the vlan tag or boost::none if no vlan tag is set
     */
    const boost::optional<uint16_t>& getAccessIfaceVlan() const {
        return accessIfaceVlan;
    }

    /**
     * Set the VLAN trunk tag for use on the access interface
     *
     * @param accessIfaceVlan the vlan tag
     */
    void setAccessIfaceVlan(uint16_t accessIfaceVlan) {
        this->accessIfaceVlan = accessIfaceVlan;
    }

    /**
     * Unset the access interface vlan trunk tag.
     */
    void unsetAccessIfaceVlan() {
        accessIfaceVlan = boost::none;
    }

    /**
     * The name of the interface on the access bridge that should be
     * used for uplinking to the integration bridge.
     *
     * @return the interface name or boost::none if no interface name
     * is set.
     */
    const boost::optional<std::string>& getAccessUplinkInterface() const {
        return accessUplinkInterface;
    }

    /**
     * Set the access uplink interface name to the string specified.
     *
     * @param accessUplinkInterface the interface name to which the endpoint
     * is attached locally
     */
    void setAccessUplinkInterface(const std::string& accessUplinkInterface) {
        this->accessUplinkInterface = accessUplinkInterface;
    }

    /**
     * Unset the access uplink interface name
     */
    void unsetAccessUplinkInterface() {
        accessUplinkInterface = boost::none;
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
     * Set the discovery proxy mode flag.  If true, then the vswitch
     * will respond to any ARP or neighbor discovery requests for all
     * IP addresses configured for the endpoint
     *
     * @param discoveryProxyMode the new value for the promiscuous
     * mode
     */
    void setDiscoveryProxyMode(bool discoveryProxyMode) {
        this->discoveryProxyMode = discoveryProxyMode;
    }

    /**
     * Get the value of the discovery proxy mode flag
     *
     * @return true if discovery proxy mode is on
     */
    bool isDiscoveryProxyMode() const {
        return discoveryProxyMode;
    }

    /**
     * Set the Nat mode flag to the value specified
     *
     * @param natMode the new value for the NAT mode
     */
    void setNatMode(bool natMode) {
        this->natMode = natMode;
    }

    /**
     * Get the value of the Nat mode flag
     *
     * @return true if Nat mode is on
     */
    bool isNatMode() const {
        return natMode;
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
    typedef std::unordered_map<std::string, std::string> attr_map_t;

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
         * Get the IP address to return as the server IP address in
         * DHCP replies.
         *
         * @return the IP address
         */
        const boost::optional<std::string>& getServerIp() const {
            return serverIp;
        }

        /**
         * Get the IP address to return as the server IP address in
         * DHCP replies.
         *
         * @param serverIp the IP address
         */
        void setServerIp(const std::string& serverIp) {
            this->serverIp = serverIp;
        }

        /**
         * Get the MAC address to use for DHCP replies and ARP replies
         * for the DHCP server IP
         *
         * @return the MAC address
         */
        const boost::optional<opflex::modb::MAC>& getServerMac() const {
            return serverMac;
        }

        /**
         * Set the MAC address to use for DHCP replies and ARP replies
         * for the DHCP server IP
         *
         * @param serverMac the MAC address
         */
        void setServerMac(const opflex::modb::MAC& serverMac) {
            this->serverMac = serverMac;
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

        /**
         * Get the interface MTU to set for this endpoint
         *
         * @return the interface MTU
         */
        const boost::optional<uint16_t> getInterfaceMtu() const {
            return interfaceMtu;
        }

        /**
         * Set the interface MTU to set for this endpoint
         *
         * @param interfaceMtu the interface MTU
         */
        void setInterfaceMtu(uint16_t interfaceMtu) {
            this->interfaceMtu = interfaceMtu;
        }

        /**
         * Get the lease time for this endpoint
         *
         * @return the lease time
         */
        const boost::optional<uint32_t> getLeaseTime() const {
            return leaseTime;
        }

        /**
         * Set the DHCP lease time for this endpoint
         *
         * @param leaseTime the lease time
         */
        void setLeaseTime(uint32_t leaseTime) {
            this->leaseTime = leaseTime;
        }

    private:
        boost::optional<std::string> ipAddress;
        boost::optional<uint8_t> prefixLen;
        boost::optional<std::string> serverIp;
        boost::optional<opflex::modb::MAC> serverMac;
        std::vector<std::string> routers;
        boost::optional<std::string> domain;
        std::vector<static_route_t> staticRoutes;
        boost::optional<uint16_t> interfaceMtu;
        boost::optional<uint32_t> leaseTime;
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

        /**
         * Get the t1 time (the time before client should contact dhcp
         * server to renew)
         *
         * @return the t1 time
         */
        const boost::optional<uint32_t> getT1() const {
            return t1;
        }

        /**
         * Set the t1 time
         *
         * @param t1 the lease time
         */
        void setT1(uint32_t t1) {
            this->t1 = t1;
        }

        /**
         * Get the t2 time (the time before client should broadcast
         * renewal attempt)
         *
         * @return the t2 time
         */
        const boost::optional<uint32_t> getT2() const {
            return t2;
        }

        /**
         * Set the t2 time
         *
         * @param t2 the t2 time
         */
        void setT2(uint32_t t2) {
            this->t2 = t2;
        }

        /**
         * Get the valid lifetime for the IPv6 address
         *
         * @return the valid lifetime
         */
        const boost::optional<uint32_t> getValidLifetime() const {
            return validLifetime;
        }

        /**
         * Set the valid lifetime
         *
         * @param validLifetime the valid lifetime
         */
        void setValidLifetime(uint32_t validLifetime) {
            this->validLifetime = validLifetime;
        }

        /**
         * Get the preferred lifetime for the IPv6 address
         *
         * @return the preferred lifetime
         */
        const boost::optional<uint32_t> getPreferredLifetime() const {
            return preferredLifetime;
        }

        /**
         * Set the preferred lifetime
         *
         * @param preferredLifetime the preferred lifetime
         */
        void setPreferredLifetime(uint32_t preferredLifetime) {
            this->preferredLifetime = preferredLifetime;
        }

    private:
        std::vector<std::string> searchList;
        boost::optional<uint32_t> t1;
        boost::optional<uint32_t> t2;
        boost::optional<uint32_t> validLifetime;
        boost::optional<uint32_t> preferredLifetime;
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

    /**
     * IP address mapping for floating IP addresses and/or SNAT
     * mappings
     */
    class IPAddressMapping {
    public:
        /**
         * Construct a new address mapping
         *
         * @param uuid_ a unique ID for this address mapping
         */
        IPAddressMapping(const std::string& uuid_) : uuid(uuid_) { }

        /**
         * Get the UUID for this address mapping
         * @return the unique ID for the address mapping
         */
        const std::string& getUUID() const {
            return uuid;
        }

        /**
         * Set the UUID for the address mapping
         *
         * @param uuid the unique ID for the address mapping
         */
        void setUUID(const std::string& uuid) {
            this->uuid = uuid;
        }

        /**
         * Get the floating IP address for this address mapping
         *
         * @return the IP address
         */
        const boost::optional<std::string>& getFloatingIP() const {
            return floatingIp;
        }

        /**
         * Set the IP address for the address mapping
         *
         * @param floatingIp the IP address
         */
        void setFloatingIP(const std::string& floatingIp) {
            this->floatingIp = floatingIp;
        }

        /**
         * Unset the IP address for the address mapping
         */
        void unsetFloatingIP() {
            floatingIp = boost::none;
        }

        /**
         * Get the "real" IP address to which this address mapping is mapped
         *
         * @return the mapped IP address
         */
        const boost::optional<std::string>& getMappedIP() const {
            return mappedIp;
        }

        /**
         * Set the "real" IP address to which this address mapping is mapped
         *
         * @param mappedIp the mapped IP address
         */
        void setMappedIP(const std::string& mappedIp) {
            this->mappedIp = mappedIp;
        }

        /**
         * Unset the mapped IP address for the address mapping
         */
        void unsetMappedIP() {
            mappedIp = boost::none;
        }

        /**
         * Get the next hop interface for this address mapping.  If
         * this value is set, after performing the address mapping the
         * flow will be delivered to this interface rather than being
         * processed normally.
         *
         * @return the next hop interface
         */
        const boost::optional<std::string>& getNextHopIf() const {
            return nextHopIf;
        }

        /**
         * Set the next hop interface for the address mapping
         *
         * @param nextHopIf The name of the switch interface for the
         * next hop
         */
        void setNextHopIf(const std::string& nextHopIf) {
            this->nextHopIf = nextHopIf;
        }

        /**
         * Unset the next hop interface the address mapping
         */
        void unsetNextHopIf() {
            nextHopIf = boost::none;
        }

        /**
         * Get the next hop MAC address for this IP address mapping.
         * If set, use this router MAC address as the destination MAC
         * instead of the regular subnet router MAC address
         *
         * @return the hext hop MAC address
         */
        const boost::optional<opflex::modb::MAC>& getNextHopMAC() const {
            return nextHopMac;
        }

        /**
         * Set the next hop MAC address for the endpoint
         *
         * @param nextHopMac the MAC address
         */
        void setNextHopMAC(const opflex::modb::MAC& nextHopMac) {
            this->nextHopMac = nextHopMac;
        }

        /**
         * Unset the next hop MAC address for the endpoint
         */
        void unsetNextHopMAC() {
            nextHopMac = boost::none;
        }

        /**
         * Get the endpoint group associated with this address mapping.
         * This is the endpoint group into which the address mapping
         * address will be mapped.
         *
         * @return the endpoint group URI
         */
        const boost::optional<opflex::modb::URI>& getEgURI() const {
            return egURI;
        }

        /**
         * Set the endpoint group associated with this address mapping.
         * This is the endpoint group into which the address mapping
         * address will be mapped.
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

    private:
        std::string uuid;
        boost::optional<opflex::modb::MAC> nextHopMac;
        boost::optional<std::string> floatingIp;
        boost::optional<std::string> mappedIp;
        boost::optional<std::string> nextHopIf;
        boost::optional<opflex::modb::URI> egURI;
    };

    /**
     * Functor for storing an IPAddressMapping as hash key
     */
    struct IPAddressMappingHash {
        /**
         * Hash the address mapping
         */
        size_t operator()(const IPAddressMapping& m) const noexcept;
    };

    /**
     * A set of address mapping hash objects
     */
    typedef std::unordered_set<IPAddressMapping, IPAddressMappingHash> ipam_set;

    /**
     * Clear the list of address mappings
     */
    void clearIPAddressMappings() {
        ipAddressMappings.clear();
    }

    /**
     * Add a address mapping to the endpoint
     *
     * @param ipAddressMapping the address mapping object
     */
    void addIPAddressMapping(const IPAddressMapping& ipAddressMapping);

    /**
     * Get the set of address mappings for the endpoint
     *
     * @return a set of address mapping objects
     */
    const ipam_set& getIPAddressMappings() const {
        return ipAddressMappings;
    }

    /**
     * Get SNAT IP address
     *
     * @return the SNAT IP address
     */
    const boost::optional<std::string>& getSnatIP() const {
        return snatIp;
    }

    /**
     * Set SNAT IP address
     *
     * @param snatIp the SNAT IP address
     */
    void setSnatIP(const std::string& snatIp) {
        this->snatIp = snatIp;
    }

    /**
     * Unset the SNAT IP address
     */
    void unsetSnatIP() {
        snatIp = boost::none;
    }

    /**
     * Set the endpoint to be external
     *
     */
    void setExternal() {
        external = true;
    }

    /**
     * Get whether this endpoint is external
     *
     * @return whether the endpoint is external
     */
    bool isExternal() const {
        return external;
    }

    /**
     * Set the external interface URI associated with this endpoint.
     * The external interface URI is the path attachment of this external
     * endpoint to the fabric.
     *
     * @param extIntURI the interface URI to set
     */
    void setExtInterfaceURI(const opflex::modb::URI& extIntURI) {
        this->extInterfaceURI = extIntURI;
    }

    /**
     * Get the external interface URI associated with this endpoint.
     * The external interface URI is the path attachment of this external
     * endpoint to the fabric.
     *
     * @return the external interface URI associated with this endpoint
     */
    boost::optional<opflex::modb::URI> getExtInterfaceURI() const {
        return this->extInterfaceURI;
    }

    /**
     * Set the external node URI associated with this endpoint.
     * The external node URI is the node attachment of this external
     * endpoint to the fabric.
     *
     * @param extNodeURI the interface URI to set
     */
    void setExtNodeURI(const opflex::modb::URI& extNodeURI) {
        this->extNodeURI = extNodeURI;
    }

    /**
     * Get the external node URI associated with this endpoint.
     * The external node URI is the node attachment of this external
     * endpoint to the fabric.
     *
     * @return the external node URI associated with this endpoint
     */
    boost::optional<opflex::modb::URI> getExtNodeURI() const {
        return this->extNodeURI;
    }

private:
    std::string uuid;
    boost::optional<opflex::modb::MAC> mac;
    std::unordered_set<std::string> ips;
    std::unordered_set<std::string> anycastReturnIps;
    virt_ip_set virtualIps;
    boost::optional<std::string> egMappingAlias;
    boost::optional<opflex::modb::URI> egURI;
    /*Properties in this block are relevant for external
     endpoints only*/
    boost::optional<opflex::modb::URI> extInterfaceURI;
    boost::optional<opflex::modb::URI> extNodeURI;
    /*End external enpoint properties*/
    std::set<opflex::modb::URI> securityGroups;
    boost::optional<std::string> interfaceName;
    boost::optional<std::string> accessInterface;
    boost::optional<uint16_t> accessIfaceVlan;
    boost::optional<std::string> accessUplinkInterface;
    bool promiscuousMode;
    bool discoveryProxyMode;
    bool natMode;
    bool external;
    attr_map_t attributes;
    boost::optional<DHCPv4Config> dhcpv4Config;
    boost::optional<DHCPv6Config> dhcpv6Config;
    ipam_set ipAddressMappings;
    boost::optional<std::string> snatIp;

};

/**
 * Print an endpoint to an ostream
 */
std::ostream & operator<<(std::ostream &os, const Endpoint& ep);

/**
 * Compute a hash value for a address mapping
 */
size_t hash_value(Endpoint::IPAddressMapping const& ip);

/**
 * Check for address mapping equality.
 */
bool operator==(const Endpoint::IPAddressMapping& lhs,
                const Endpoint::IPAddressMapping& rhs);
/**
 * Check for address mapping inequality.
 */
bool operator!=(const Endpoint::IPAddressMapping& lhs,
                const Endpoint::IPAddressMapping& rhs);

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_ENDPOINT_H */
