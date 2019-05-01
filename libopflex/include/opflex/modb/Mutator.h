/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file Mutator.h
 * @brief Interface definition file for Mutators
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_MUTATOR_H
#define MODB_MUTATOR_H

#include <string>

#include "opflex/modb/URI.h"
#include "opflex/modb/mo-internal/ObjectInstance.h"
#include "opflex/ofcore/OFTypes.h"

namespace opflex {

namespace ofcore {
class OFFramework;
}

namespace modb {

/**
 * @addtogroup cpp
 * @{
 */

/**
 * @defgroup modb Managed Objects
 * Types for accessing and working with managed objects.
 * @{
 */

/**
 * @brief A mutator represents a set of changes to apply to the data
 * store.
 *
 * When modifying data through the managed object accessor classes,
 * changes are not actually written into the store, but rather are
 * written into the Mutator registered using the framework in a
 * thread-local variable.  These changes will not become visible until
 * the mutator is committed by calling commit()
 *
 * The changes are applied to the data store all at once but not
 * necessarily guaranteed to be atomic.  That is, a read in another
 * thread could see some objects updated but not others. Each
 * individual object update <i>is</i> applied atomically, however, so
 * the fields within that object will either all be modified or none.
 *
 * This is most similar to a database transaction with an isolation
 * level set to read uncommitted.
 */
class Mutator {
public:

    /**
     * Create a mutator that will work with the provided framework
     * instance and owner.
     * @param framework the framework instance that will be modified
     * @param owner the owner string that will control which fields
     * can be modified.
     */
    Mutator(ofcore::OFFramework& framework,
            const std::string& owner);

    /**
     * Destroy the Mutator.  Any uncommitted changes will be lost.
     */
    ~Mutator();

    /**
     * Commit the changes stored in the mutator to the object store.
     * If this method is not called, these changes will simply be
     * lost.  After you call commit(), you can continue to make
     * changes using the same mutator which will be effectively a new
     * "transaction".
     */
    void commit();

    /**
     * Create a new mutable object with the given URI which is a copy
     * of any existing object with the specified URI.
     *
     * @param class_id the class ID for the object
     * @param uri The URI for the object
     */
    OF_SHARED_PTR<mointernal::ObjectInstance>& modify(class_id_t class_id,
                                                      const URI& uri);

    /**
     * Delete the child object specified along with its link to its
     * parents.  If the child object has any children that are not
     * removed, then they will be garbage-collected at some later
     * time.
     *
     * @param class_id the class ID for the object
     * @param uri the URI to remove
     */
    void remove(class_id_t class_id, const URI& uri);

    /**
     * Create a new child object with the specified class and URI, and
     * make it a child of the given parent.  If the object already
     * exists, get a mutable copy of that object.
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class the class ID of the child
     * @param child_uri the URI of the child
     */
    OF_SHARED_PTR<mointernal
                  ::ObjectInstance>& addChild(class_id_t parent_class,
                                              const URI& parent_uri,
                                              prop_id_t parent_prop,
                                              class_id_t child_class,
                                              const URI& child_uri);

private:
    class MutatorImpl;
    friend class MutatorImpl;
    MutatorImpl* pimpl;
};

/* @} modb */
/* @} cpp */

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_MUTATOR_H */
