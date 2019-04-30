/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for policy listener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/modb/URI.h>
#include <opflex/modb/PropertyInfo.h>

#pragma once
#ifndef OPFLEXAGENT_POLICYLISTENER_H
#define OPFLEXAGENT_POLICYLISTENER_H

namespace opflexagent {

/**
 * An abstract interface for classes interested in updates related to
 * the policy and the indices.
 */
class PolicyListener {
public:
    /**
     * Destroy the policy listener and clean up all state
     */
    virtual ~PolicyListener() {};

    /**
     * Called when the forwarding domains for an endpoint group have
     * been updated.
     */
    virtual void egDomainUpdated(const opflex::modb::URI&) {};

    /**
     * Called when an external interface has been updated.
     */
    virtual void externalInterfaceUpdated(const opflex::modb::URI&) {};

    /**
     * Called when a static route has been updated.
     */
    virtual void staticRouteUpdated(const opflex::modb::URI&) {};

    /**
     * Called when a remote route has been updated.
     */
    virtual void remoteRouteUpdated(const opflex::modb::URI&) {};

    /**
     * Called when a local route has been updated.
     */
    virtual void localRouteUpdated(const opflex::modb::URI&) {};

    /**
     * Called when a forwarding domain object is updated.
     */
    virtual void domainUpdated(opflex::modb::class_id_t,
                               const opflex::modb::URI&) {}

    /**
     * Called when a policy contract is updated. These include changes
     * to the rules that compose the contract as well as changes to
     * the list of provider/consumer endpoint groups for the contract.
     */
    virtual void contractUpdated(const opflex::modb::URI&) {}

    /**
     * Called when a security group is updated, including changes to
     * the rules that compose the security group.
     */
    virtual void secGroupUpdated(const opflex::modb::URI&) {}

    /**
     * Called when the platform config object is updated
     */
    virtual void configUpdated(const opflex::modb::URI&) {}

    /**
     * Called when span opjects are updated.
     */
     virtual void spanUpdated(const opflex::modb::URI&) {}
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_POLICYMANAGER_H */
