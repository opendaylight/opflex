/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for routing domain config
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_RDCONFIG_H
#define OPFLEXAGENT_RDCONFIG_H

#include <opflex/modb/URI.h>

#include <boost/optional.hpp>

#include <string>
#include <utility>
#include <unordered_set>

namespace opflexagent {

/**
 * A class for a routing domain config
 */
class RDConfig {
public:
    /**
     * Default constructor
     */
    RDConfig() : domainURI("/") {}

    /**
     * Construct a new RDConfig with the given uuid.
     *
     * @param domainURI_ the domain URI being configured
     */
    explicit RDConfig(const opflex::modb::URI& domainURI_)
        : domainURI(domainURI_) {}

    /**
     * The URI of the layer 3 forwarding domain relevant for this
     * service mapping.
     *
     * @return the domain name or boost::none if no domain
     * is set.
     */
    const opflex::modb::URI& getDomainURI() const {
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
     * Clear the subnets set
     */
    void clearInternalSubnets() {
        internalSubnets.clear();
    }

    /**
     * Add an internal subnet to the routing domain configuration
     *
     * @param subnet the subnet in CIDR notation
     */
    void addInternalSubnet(const std::string& subnet) {
        internalSubnets.insert(subnet);
    }

    /**
     * Get the list of internal subnets
     *
     * @return a set of internal subnets in CIDR notation
     */
    const std::unordered_set<std::string>& getInternalSubnets() const {
        return internalSubnets;
    }

private:
    opflex::modb::URI domainURI;
    std::unordered_set<std::string> internalSubnets;
};

/**
 * Print an to an ostream
 */
std::ostream & operator<<(std::ostream &os, const RDConfig& ep);

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_RDCONFIG_H */
