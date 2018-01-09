/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for endpoint listener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_ENDPOINTLISTENER_H
#define OPFLEXAGENT_ENDPOINTLISTENER_H

#include <opflex/modb/URI.h>

#include <string>

namespace opflexagent {

/**
 * An abstract interface for classes interested in updates related to
 * the endpoints
 */
class EndpointListener {
public:
    /**
     * Instantiate a new endpoint listener
     */
    EndpointListener() {};

    /**
     * Destroy the endpoint listener and clean up all state
     */
    virtual ~EndpointListener() {};

    /**
     * Called when an endpoint is added, updated, or removed.
     *
     * @param uuid the UUID for the endpoint
     */
    virtual void endpointUpdated(const std::string& uuid) = 0;

    /**
     * A set of URIs
     */
    typedef std::set<opflex::modb::URI> uri_set_t;

    /**
     * Called when a set of security groups is added or removed
     *
     * @param secGroups the set of security groups that has been
     * modified
     */
    virtual void secGroupSetUpdated(const uri_set_t& secGroups) {}
};

} /* namespace opflexagent */

namespace std {
/**
 * Template specialization for std::hash<opflexagent::EndpointListener::uri_set_t>
 */
template<> struct hash<opflexagent::EndpointListener::uri_set_t> {
    /**
     * Hash the opflexagent::EndpointListener::uri_set_t
     */
    std::size_t operator()(const opflexagent::EndpointListener::uri_set_t& u) const {
        return boost::hash_value(u);
    }
};
} /* namespace std */


#endif /* OPFLEXAGENT_ENDPOINTMANAGER_H */
