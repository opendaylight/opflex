/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for anycast service
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <utility>

#include <boost/unordered_set.hpp>
#include <boost/optional.hpp>
#include <opflex/modb/URI.h>
#include <opflex/modb/MAC.h>

#pragma once
#ifndef OVSAGENT_ANYCAST_SERVICE_H
#define OVSAGENT_ANYCAST_SERVICE_H

namespace ovsagent {

/**
 * A class for an anycast service hosted on the local system
 */
class AnycastService {
public:
    /**
     * Default constructor
     */
    AnycastService() {}

    /**
     * Construct a new AnycastService with the given uuid.
     *
     * @param uuid_ the unique ID for the service.
     */
    explicit AnycastService(const std::string& uuid_)
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
     * A mapping for this service from a forwarding domain scope to a
     * corresponding interface.
     */
    class ServiceMapping {
    public:

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

    private:
        boost::optional<std::string> serviceIp;
        boost::optional<std::string> gatewayIp;
    };

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
    const boost::unordered_set<ServiceMapping>& getServiceMappings() const {
        return serviceMappings;
    }

private:
    std::string uuid;
    boost::optional<opflex::modb::URI> domainURI;
    boost::optional<std::string> interfaceName;
    boost::optional<opflex::modb::MAC> serviceMac;
    boost::unordered_set<ServiceMapping> serviceMappings;
};

/**
 * Print an to an ostream
 */
std::ostream & operator<<(std::ostream &os, const AnycastService& ep);

/**
 * Compute a hash value for a service mapping
 */
size_t hash_value(AnycastService::ServiceMapping const& m);

/**
 * Check for service mapping equality.
 */
bool operator==(const AnycastService::ServiceMapping& lhs,
                const AnycastService::ServiceMapping& rhs);
/**
 * Check for service mapping inequality.
 */
bool operator!=(const AnycastService::ServiceMapping& lhs,
                const AnycastService::ServiceMapping& rhs);

} /* namespace ovsagent */

#endif /* OVSAGENT_ANYCAST_SERVICE_H */
