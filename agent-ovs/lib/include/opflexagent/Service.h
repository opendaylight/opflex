/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for an anycast or load balanced service
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
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
#ifndef OPFLEXAGENT_SERVICE_H
#define OPFLEXAGENT_SERVICE_H

namespace opflexagent {

/**
 * A class for an anycast or load balanced service hosted on the local
 * system
 */
class Service {
public:
    /**
     * Default constructor
     */
    Service() : serviceMode(LOCAL_ANYCAST) {}

    /**
     * Construct a new Service with the given uuid.
     *
     * @param uuid_ the unique ID for the service.
     */
    explicit Service(const std::string& uuid_)
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
     * Get the MAC address for this service mapping.
     *
     * @return the service MAC address
     */
    const boost::optional<opflex::modb::MAC>& getServiceMAC() const {
        return serviceMac;
    }

    /**
     * Set the service MAC address for the service
     *
     * @param serviceMac the MAC address
     */
    void setServiceMAC(const opflex::modb::MAC& serviceMac) {
        this->serviceMac = serviceMac;
    }

    /**
     * Unset the service MAC address for the service
     */
    void unsetServiceMAC() {
        serviceMac = boost::none;
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
     * @param interfaceName the interface name to which the service
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
     * Get the IP address for the service interface.  ARP/ND requests
     * for this IP on the service interface will be answered with the
     * service MAC.
     *
     * @return the service interface IP address
     */
    const boost::optional<std::string>& getIfaceIP() const {
        return ifaceIP;
    }

    /**
     * Set the service interface IP address for the service
     *
     * @param ifaceIP the IP address
     */
    void setIfaceIP(const std::string& ifaceIP) {
        this->ifaceIP = ifaceIP;
    }

    /**
     * Unset the service Iface IP address for the service
     */
    void unsetIfaceIP() {
        ifaceIP = boost::none;
    }

    /**
     * The URI of the layer 3 forwarding domain relevant for this
     * service mapping.
     *
     * @return the domain name or boost::none if no domain
     * is set.
     */
    const boost::optional<opflex::modb::URI>& getDomainURI() const {
        return domainURI;
    }

    /**
     * Set the l3 domain URI to the URI specified.
     *
     * @param domainURI the domain uri to which the endpoint
     * is attached locally
     */
    void setDomainURI(const opflex::modb::URI& domainURI) {
        this->domainURI = domainURI;
    }

    /**
     * Unset the l3 domain uri
     */
    void unsetDomainURI() {
        domainURI = boost::none;
    }

    /**
     * Modes for the service
     */
    enum ServiceMode {
        /**
         * Provide the service to only the local endpoints.  The
         * service interface (if present) implements the service
         */
        LOCAL_ANYCAST,
        /**
         * Load balance a service across multiple endpoints, exposed
         * via a virtual service IP.  If a service interface is
         * present, then expose the service to remote endpoints using
         * the service interface.
         */
        LOADBALANCER
    };

    /**
     * Set the service mode
     *
     * @param serviceMode the new value for the service mode
     */
    void setServiceMode(ServiceMode serviceMode) {
        this->serviceMode = serviceMode;
    }

    /**
     * Get the service mode
     *
     * @return the service mode
     */
    ServiceMode getServiceMode() const {
        return serviceMode;
    }

    /**
     * A mapping for this service from a forwarding domain scope to a
     * corresponding interface.
     */
    class ServiceMapping {
    public:
        /**
         * Default constructor for service mapping
         */
        ServiceMapping() : ctMode(false) {}

        /**
         * Get the service IP address for this service mapping
         *
         * @return the IP address
         */
        const boost::optional<std::string>& getServiceIP() const {
            return serviceIp;
        }

        /**
         * Set the IP address for the service mapping
         *
         * @param serviceIp the IP address
         */
        void setServiceIP(const std::string& serviceIp) {
            this->serviceIp = serviceIp;
        }

        /**
         * Unset the IP address for the service mapping
         */
        void unsetServiceIP() {
            serviceIp = boost::none;
        }

        /**
         * Get the protocol (either tcp or udp) for the service.  If
         * unset all traffic to/from the service IP will match
         *
         * @return the protocol
         */
        const boost::optional<std::string>& getServiceProto() const {
            return serviceProto;
        }

        /**
         * Set the service protocol
         *
         * @param serviceProto the service protocol
         */
        void setServiceProto(const std::string& serviceProto) {
            this->serviceProto = serviceProto;
        }

        /**
         * Unset the service protocol for the service mapping
         */
        void unsetServiceProto() {
            serviceProto = boost::none;
        }

        /**
         * Port number on which the service will be accessed.  If
         * unspecified all traffic to/from the service IP will match
         *
         * @return the service port number
         */
        const boost::optional<uint16_t>& getServicePort() const {
            return servicePort;
        }

        /**
         * Set the service port for the service mapping
         *
         * @param servicePort the service port number
         */
        void setServicePort(uint16_t servicePort) {
            this->servicePort = servicePort;
        }

        /**
         * Unset the service port
         */
        void unsetServicePort() {
            servicePort = boost::none;
        }

        /**
         * Get the gateway IP address for this service mapping.  This
         * won't act as a "real" gateway but will reply to ARP or
         * neighbor discovery requests for the IP address specified.
         *
         * @return the IP address
         */
        const boost::optional<std::string>& getGatewayIP() const {
            return gatewayIp;
        }

        /**
         * Set the gateway IP address for the service mapping
         *
         * @param gatewayIp the IP address
         */
        void setGatewayIP(const std::string& gatewayIp) {
            this->gatewayIp = gatewayIp;
        }

        /**
         * Unset the gateway IP address for the service mapping
         */
        void unsetGatewayIP() {
            gatewayIp = boost::none;
        }

        /**
         * Get the next hop IP addresses for this service mapping.
         * When delivering to the service interface, rewrite the
         * destination IP to the specified next hop IP address.  If
         * there are multiple addreses, load balance across them
         *
         * @return the IP address
         */
        const std::set<std::string>& getNextHopIPs() const {
            return nextHopIps;
        }

        /**
         * Set the next hop IP addresses for the service mapping
         *
         * @param nextHopIps the IP addresses
         */
        void setNextHopIPs(const std::set<std::string>& nextHopIps) {
            this->nextHopIps = nextHopIps;
        }

        /**
         * Add a next hop IP address for the service mapping
         *
         * @param nextHopIp the IP address
         */
        void addNextHopIP(const std::string& nextHopIp) {
            this->nextHopIps.insert(nextHopIp);
        }

        /**
         * Port number when the service traffic is delivered to the
         * service endpoint.  If unspecified the next hop port is the
         * same as the service port
         *
         * @return the next hop port number
         */
        const boost::optional<uint16_t>& getNextHopPort() const {
            return nextHopPort;
        }

        /**
         * Set the next hop port for the service mapping
         *
         * @param nextHopPort the next hop port number
         */
        void setNextHopPort(uint16_t nextHopPort) {
            this->nextHopPort = nextHopPort;
        }

        /**
         * Unset the next hop port
         */
        void unsetNextHopPort() {
            nextHopPort = boost::none;
        }

        /**
         * Set the connection tracking mode flag to the value
         * specified.  If connection tracking is enabled, reverse flow
         * mapping requires a stateful connection
         *
         * @param ctMode the new value for the connection tracking
         * mode
         */
        void setConntrackMode(bool ctMode) {
            this->ctMode = ctMode;
        }

        /**
         * Get the value of the connection tracking mode flag
         *
         * @return true if connection tracking mode is on
         */
        bool isConntrackMode() const {
            return ctMode;
        }

    private:
        boost::optional<std::string> serviceIp;
        boost::optional<std::string> serviceProto;
        boost::optional<uint16_t> servicePort;
        boost::optional<std::string> gatewayIp;
        std::set<std::string> nextHopIps;
        boost::optional<uint16_t> nextHopPort;
        bool ctMode;
    };

    /**
     * Functor for storing a ServiceMapping as hash key
     */
    struct ServiceMappingHash {
        /**
         * Hash the service mapping
         */
        size_t operator()(const ServiceMapping& m) const noexcept;
    };

    /**
     * A set of service mapping hash objects
     */
    typedef std::unordered_set<ServiceMapping, ServiceMappingHash> sm_set;

    /**
     * Clear the list of service mappings
     */
    void clearServiceMappings() {
        serviceMappings.clear();
    }

    /**
     * Add a service mapping to the endpoint
     *
     * @param serviceMapping the service mapping object
     */
    void addServiceMapping(const ServiceMapping& serviceMapping);

    /**
     * Get the set of service mappings for the endpoint
     *
     * @return a set of service mapping objects
     */
    const sm_set& getServiceMappings() const {
        return serviceMappings;
    }

private:
    std::string uuid;
    boost::optional<opflex::modb::URI> domainURI;
    boost::optional<std::string> interfaceName;
    boost::optional<uint16_t> ifaceVlan;
    boost::optional<opflex::modb::MAC> serviceMac;
    boost::optional<std::string> ifaceIP;
    ServiceMode serviceMode;
    sm_set serviceMappings;
};

/**
 * Print an to an ostream
 */
std::ostream & operator<<(std::ostream &os, const Service& ep);

/**
 * Check for service mapping equality.
 */
bool operator==(const Service::ServiceMapping& lhs,
                const Service::ServiceMapping& rhs);
/**
 * Check for service mapping inequality.
 */
bool operator!=(const Service::ServiceMapping& lhs,
                const Service::ServiceMapping& rhs);

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_SERVICE_H */
