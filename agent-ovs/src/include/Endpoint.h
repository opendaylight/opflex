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
    Endpoint() : promiscuousMode(false), discoveryProxyMode(false) {}

    /**
     * Construct a new Endpoint with the given uuid.  Note that
     * endpoints in different groups can potentially have the same MAC
     * addresses, so the MAC may not be a unique key.
     *
     * @param uuid_ the unique ID for the endpoint.
     */
    explicit Endpoint(const std::string& uuid_)
        : uuid(uuid_), promiscuousMode(false), discoveryProxyMode(false) {}

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
     * Get the list of IP addresses that are valid sources for anycast
     * service addresses.
     *
     * @return the list of IP addresses
     */
    const boost::unordered_set<std::string>& getAnycastReturnIPs() const {
        return anycastReturnIps;
    }

    /**
     * Set the list of IP addresses that are valid sources for anycast
     * service addresses.  This will overwrite any existing IP
     * addresses
     *
     * @param ips the IP addresses
     */
    void setAnycastReturnIPs(const boost::unordered_set<std::string>& ips) {
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
     * Get the list of virtual IP addresses associated with this endpoint
     *
     * @return the list of virtual IP addresses
     */
    const boost::unordered_set<virt_ip_t>& getVirtualIPs() const {
        return virtualIps;
    }

    /**
     * Set the virtual IP addresses for this endpoint.  This will overwrite
     * any existing virtual IP addresses
     *
     * @param virtualIps the virtual IP addresses
     */
    void setVirtualIPs(const boost::unordered_set<virt_ip_t>& virtualIps) {
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

    private:
        boost::optional<std::string> ipAddress;
        boost::optional<uint8_t> prefixLen;
        boost::optional<std::string> serverIp;
        std::vector<std::string> routers;
        boost::optional<std::string> domain;
        std::vector<static_route_t> staticRoutes;
        boost::optional<uint16_t> interfaceMtu;
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
    const boost::unordered_set<IPAddressMapping>& getIPAddressMappings() const {
        return ipAddressMappings;
    }

    /**
     * An attestation can be used by the endpoint registry to confirm
     * the validity of a reported endpoint.
     */
    class Attestation {
    public:
        /**
         * Construct a new attestation
         *
         * @param name_ a name for this attestation unique for the
         * endpoint
         */
        Attestation(const std::string& name_) : name(name_) { }

        /**
         * Get the name for this attestation
         * @return the name
         */
        const std::string& getName() const {
            return name;
        }

        /**
         * Get the validator for this endpoint.  This is an encoded
         * string that provides validation for the information in the
         * endpoint.
         *
         * @return the validator
         */
        const boost::optional<std::string>& getValidator() const {
            return validator;
        }

        /**
         * Set the validator for the endpoint.  This is an encoded
         * string that provides validation for the information in the
         * endpoint.
         *
         * @param validator the validator string
         */
        void setValidator(const std::string& validator) {
            this->validator = validator;
        }

        /**
         * Unset the validator
         */
        void unsetValidator() {
            validator = boost::none;
        }

        /**
         * Get the validator MAC for this endpoint's validator.
         *
         * @return the validatorMac
         */
        const boost::optional<std::string>& getValidatorMac() const {
            return validatorMac;
        }

        /**
         * Set the validator MAC for this endpoint's validator.  The
         * validator MAC authenticates the validator.
         *
         * @param validatorMac the validator MAC encoded string
         */
        void setValidatorMac(const std::string& validatorMac) {
            this->validatorMac = validatorMac;
        }

        /**
         * Unset the validatorMac
         */
        void unsetValidatorMac() {
            validatorMac = boost::none;
        }

    private:
        std::string name;
        boost::optional<std::string> validator;
        boost::optional<std::string> validatorMac;
    };

    /**
     * Clear the list of attestations
     */
    void clearAttestations() {
        attestations.clear();
    }

    /**
     * Add a address mapping to the endpoint
     *
     * @param attestation the address mapping object
     */
    void addAttestation(const Attestation& attestation);

    /**
     * Get the set of address mappings for the endpoint
     *
     * @return a set of address mapping objects
     */
    const std::vector<Attestation>& getAttestations() const {
        return attestations;
    }

private:
    std::string uuid;
    boost::optional<opflex::modb::MAC> mac;
    boost::unordered_set<std::string> ips;
    boost::unordered_set<std::string> anycastReturnIps;
    boost::unordered_set<virt_ip_t> virtualIps;
    boost::optional<std::string> egMappingAlias;
    boost::optional<opflex::modb::URI> egURI;
    std::set<opflex::modb::URI> securityGroups;
    boost::optional<std::string> interfaceName;
    bool promiscuousMode;
    bool discoveryProxyMode;
    attr_map_t attributes;
    boost::optional<DHCPv4Config> dhcpv4Config;
    boost::optional<DHCPv6Config> dhcpv6Config;
    boost::unordered_set<IPAddressMapping> ipAddressMappings;
    std::vector<Attestation> attestations;
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

} /* namespace ovsagent */

#endif /* OVSAGENT_ENDPOINT_H */
