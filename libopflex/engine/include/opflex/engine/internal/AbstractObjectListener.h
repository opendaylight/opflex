/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file AbstractObjectListener.h
 * @brief Interface definition file for AbstractObjectListener
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_ENGINE_ABSTRACTOBJECTLISTENER_H
#define OPFLEX_ENGINE_ABSTRACTOBJECTLISTENER_H

#include <set>
#include "opflex/modb/ObjectListener.h"

namespace opflex {
namespace engine {
namespace internal {

/**
 * @brief Abstract base class that implements ObjectListener.
 *
 * This class will handle registering and unregistering for events for
 * a class, and will ensure that it is unregistered on destruction.
 */
class AbstractObjectListener : public modb::ObjectListener {
public:
    /**
     * Allocate a new listener for the parent store
     */
    AbstractObjectListener(modb::ObjectStore* store);

    /**
     * Destroy the listener.  Implicitly calls unlisten() as well.
     */
    virtual ~AbstractObjectListener();
    
    // see ObjectListener::objectUpdated
    virtual void objectUpdated(modb::class_id_t class_id, 
                               const modb::URI& uri) = 0;

    /**
     * Register this listener to get updates for the specified class.
     * Note that this method is must not be called concurrently for a
     * particular object listener instance, but can be called in
     * multiple threads for multiple listener instances.
     * 
     * @param class_id the class ID to get updates for
     */
    virtual void listen(modb::class_id_t class_id);

    /**
     * Unregister for updates to a class. Note that this method is
     * must not be called concurrently for a particular object
     * listener instance, but can be called in multiple threads for
     * multiple listener instances.
     * 
     * @param class_id the class ID to get updates for
     */
    virtual void unlisten(modb::class_id_t class_id);

    /**
     * Unregister for all updates to all classes to which we're
     * currently listening. Note that this method is must not be
     * called concurrently for a particular object listener instance,
     * but can be called in multiple threads for multiple listener
     * instances.
     */
    virtual void unlisten();

protected:
    /**
     * The parent object store associated with this listener
     */
    modb::ObjectStore* store;

    /**
     * The set of classes to which we're currently listening
     */
    std::set<modb::class_id_t> listen_classes;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_ABSTRACTOBJECTLISTENER_H */
