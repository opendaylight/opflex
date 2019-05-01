/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MO.h
 * @brief Interface definition file for managed objects
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_CORE_MO_H
#define OPFLEX_CORE_MO_H

#include <string>

#include <boost/ref.hpp>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>

#include "opflex/ofcore/OFFramework.h"
#include "opflex/modb/ObjectListener.h"
#include "opflex/modb/mo-internal/StoreClient.h"
#include "opflex/ofcore/OFTypes.h"

namespace opflex {
namespace modb {
namespace mointernal {

/**
 * @brief This is the base class for all managed objects, which are
 * the primary point of interface with data stored in the managedb
 * object store.
 *
 * A managed object is an object that can be stored in the managed
 * object store, and supports operations for serialization and
 * deserialization for use with the OpFlex protocol.  Managed objects
 * are the primary interface point for the opflex framework.
 *
 * Children of MO are temporary typed objects that reference the
 * "real" objects stored in the MO.  The properties of the MO will not
 * change once resolved, so if the object changes in the store, you
 * must respond to the update notification by resolving the affected
 * MOs again.
 */
class MO : private boost::noncopyable {
public:
    /**
     * Destroy an MO
     */
    virtual ~MO();

    /**
     * Get the class ID associated with this managed object.  The
     * class ID is a unique ID for the type.  This ID must be unique
     * for a given managed object database, but need not be globally
     * unique.
     *
     * @return The unique class ID
     */
    class_id_t getClassId() const;

    /**
     * Get the URI associated with this managed object.  The URI shows
     * the location of the managed object in the tree.
     *
     * @return the URI for the object
     */
    const URI& getURI() const;

protected:

    /**
     * Construct an MO associated with a non-default framework instance
     *
     * @param framework the framework instance
     * @param class_id the class ID for the object
     * @param uri the URI for the object
     * @param oi the object instance for this object
     */
    MO(ofcore::OFFramework& framework,
       class_id_t class_id,
       const URI& uri,
       const OF_SHARED_PTR<const ObjectInstance>& oi);

    /**
     * Get the framework instance associated with this managed object
     *
     * @return the framework instance
     */
    ofcore::OFFramework& getFramework() const;

    /**
     * Get the raw object instance associated with this managed object
     *
     * @return the raw object instance
     */
    const ObjectInstance& getObjectInstance() const;

    /**
     * Resolve the specified URI to the underlying object instance, if
     * it exists.
     *
     * @param framework the framework instance
     * @param class_id the class ID for the corresponding object
     * @param uri the URI to resolve
     * @return a shared pointer to the object instance
     * @throws std::out_of_range if the provided URI does not exist
     * locally
     */
    static OF_SHARED_PTR<const ObjectInstance>
    resolveOI(ofcore::OFFramework& framework,
              class_id_t class_id,
              const URI& uri);

    /**
     * Get a read-only store client for the framework instance
     */
    static StoreClient& getStoreClient(ofcore::OFFramework& framework);

    /**
     * Get the currently-active mutator.
     *
     * @throws std::logic_error if there is no active mutator
     */
    Mutator& getTLMutator();

    /**
     * Register the listener for the specified class ID
     */
    static void registerListener(ofcore::OFFramework& framework,
                                 ObjectListener* listener,
                                 class_id_t class_id);

    /**
     * Unregister the listener for the specified class ID
     */
    static void unregisterListener(ofcore::OFFramework& framework,
                                   ObjectListener* listener,
                                   class_id_t class_id);

    /**
     * Resolve the specified URI to its managed object wrapper class,
     * if it exists.
     *
     * @param framework the framework instance
     * @param class_id the class ID for the corresponding object
     * @param uri the URI to resolve
     * @return a shared pointer to the managed object.  Returns
     * boost::none if the object does not exist
     */
    template <class T> static
    boost::optional<OF_SHARED_PTR<T> >
    resolve(ofcore::OFFramework& framework,
            class_id_t class_id,
            const URI& uri) {
        try {
            return OF_MAKE_SHARED<T>(framework, uri,
                                     resolveOI(framework, class_id, uri));
        } catch (const std::out_of_range& e) {
            return boost::none;
        }
    }

    /**
     * Resolve any children of the specified parent object to their
     * managed object wrapper classes.
     */
    template <class T> static
    void resolveChildren(ofcore::OFFramework& framework,
                         class_id_t parent_class,
                         const URI& parent_uri,
                         prop_id_t parent_prop,
                         class_id_t child_class,
                         /* out */ std::vector<OF_SHARED_PTR<T> >& out) {
        std::vector<URI> childURIs;
        MO::getStoreClient(framework)
            .getChildren(parent_class, parent_uri, parent_prop,
                         child_class, childURIs);
        std::vector<URI>::const_iterator it;
        for (it = childURIs.begin(); it != childURIs.end(); ++it) {
            boost::optional<OF_SHARED_PTR<T> > child =
                resolve<T>(framework, child_class, *it);
            if (child) out.push_back(child.get());
        }
    }

    /**
     * Add a child of the specified type to the mutator and
     * instantiate the correct wrapper class
     */
    template <class T>
    OF_SHARED_PTR<T> addChild(class_id_t parent_class,
                              const URI& parent_uri,
                              prop_id_t parent_prop,
                              class_id_t child_class,
                              const URI& child_uri) {
        return OF_MAKE_SHARED<T>(boost::ref(getFramework()),
                                 child_uri,
                                 getTLMutator().addChild(parent_class,
                                                         parent_uri,
                                                         parent_prop,
                                                         child_class,
                                                         child_uri));
    }

    /**
     * Add a root element of the given type to the framework
     */
    template <class T>
    static OF_SHARED_PTR<T>
    createRootElement(ofcore::OFFramework& framework,
                      class_id_t class_id) {
        return OF_MAKE_SHARED<T>(boost::ref(framework),
                                 URI::ROOT,
                                 framework.getTLMutator()
                                 .modify(class_id, URI::ROOT));
    }

    /**
     * Remove the specified node
     */
    static void remove(ofcore::OFFramework& framework,
                       class_id_t class_id,
                       const modb::URI& uri) {
        framework.getTLMutator().remove(class_id, uri);
    }

private:
    class MOImpl;
    friend class MOImpl;
    MOImpl* pimpl;

    friend bool operator==(const MO& lhs, const MO& rhs);
    friend bool operator!=(const MO& lhs, const MO& rhs);
};

/**
 * Check for MO equality.
 */
bool operator==(const MO& lhs, const MO& rhs);
/**
 * Check for MO inequality.
 */
bool operator!=(const MO& lhs, const MO& rhs);

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */

#endif /* OPFLEX_CORE_MO_H */
