/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file StoreClient.h
 * @brief Interface definition file for MODB
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_STORECLIENT_H
#define MODB_STORECLIENT_H

#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>

#include "opflex/modb/URI.h"
#include "opflex/modb/mo-internal/ObjectInstance.h"

namespace opflex {
namespace modb {

class Region;
class ObjectStore;

namespace mointernal {

/**
 * @brief A client for accessing the object store scoped to an owner.
 *
 * A store client can read any data but can write only to data that is
 * owned by the corresponding owner.
 */
class StoreClient : private boost::noncopyable {
public:
    /**
     * Get the owner of this store client
     *
     * @return the string ID for the owner of this client
     */
    const std::string& getOwner() const;

    /**
     * Set the specified URI to the provided object instance,
     * replacing any existing value
     *
     * @param class_id the class ID for the object being inserted
     * @param uri the URI for the object instance
     * @param oi the object instance to set
     * @throws std::out_of_range if there is no such class ID
     * registered
     */
    void put(class_id_t class_id,
             const URI& uri, 
             const boost::shared_ptr<const ObjectInstance>& oi);

    /**
     * Set the specified URI to the provided object instance if it has
     * been modified, atomically.  Return true if any change was made.
     *
     * @param class_id the class ID for the object being inserted
     * @param uri the URI for the object instance
     * @param oi the object instance to set
     * @return true if the object was updated, false if the new and
     * the old value are the same.
     * @throws std::out_of_range if there is no such class ID
     * registered
     */
    bool putIfModified(class_id_t class_id,
                       const URI& uri, 
                       const boost::shared_ptr<const ObjectInstance>& oi);

    /**
     * Get the object instance associated with the given class ID and
     * URI.
     *
     * @param class_id the class ID for the object being retrieved
     * @param uri the URI for the object instance
     * @return a shared ptr to an object instance that must not be
     * modified.
     * @throws std::out_of_range if no such element is present or
     * there is no such class ID registered
     */
    boost::shared_ptr<const ObjectInstance> get(class_id_t class_id, 
                                                const URI& uri) const;

    /**
     * A map to store queued notifications
     */
    typedef boost::unordered_map<URI, class_id_t> notif_t;

    /**
     * Remove the specified URI, if present
     *
     * @param class_id the class ID for the object being inserted
     * @param uri the URI to remove

     * @param recursive remove all the children of the object as well.
     * Setting this to true is equivalent to calling remove
     * nonrecursively followed by calling removeChildren.
     * @param notifs an optional notification map that will get added
     * to for any URI that is recursively removed, though not the
     * top-level node.
     * @return true if an object was removed
     * @throws std::out_of_range If no such class ID is registered
     */
    bool remove(class_id_t class_id, const URI& uri,
                bool recursive, 
                /* out */ notif_t* notifs = NULL);

    /**
     * Add a parent/child relationship between a parent URI (from any
     * region) to a child URI (in this region).  Note that only the
     * owner of the child object can change the parent/child
     * relationship.
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class the class ID of the child
     * @param child_uri the URI of the child
     * @return true if the child relationship was not already present
     * @throws std::out_of_range If no such class ID is registered
     * @throws std::invalid_argument If the parent URI is not a
     * prefix of the child URI
     */
    bool addChild(class_id_t parent_class,
                  const URI& parent_uri, 
                  prop_id_t parent_prop,
                  class_id_t child_class,
                  const URI& child_uri);

    /**
     * Remove a parent/child relationship between a parent URI and a
     * child URI.  Note that only the owner of the child object can
     * change the parent/child relationship.
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class the class ID of the child
     * @param child_uri the URI of the child
     * @throws std::out_of_range If no such class ID is registered
     */
    void delChild(class_id_t parent_class,
                  const URI& parent_uri, 
                  prop_id_t parent_prop,
                  class_id_t child_class,
                  const URI& child_uri);

    /**
     * Get the parent for the given child URI.
     *
     * @param child_class the class of the child object
     * @param child the URI of the child object
     * @return a (URI, prop_id_t) pair which is the URI of the parent
     * and the property that represents the relation.
     * @throws std::out_of_range of the child URI is not found or has
     * no parent
     */
    std::pair<URI, prop_id_t> getParent(class_id_t child_class,
                                        const URI& child);

    /**
     * Get the children of the parent URI and property and put the
     * result into the supplied vector.
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class the class ID of the child
     * @param output the output array that will get the output
     * @throws std::out_of_range If no such class ID is registered
     */
    void getChildren(class_id_t parent_class, 
                     const URI& parent_uri, 
                     prop_id_t parent_prop,
                     class_id_t child_class,
                     /* out */ std::vector<URI>& output);

    /**
     * Remove all the children of the given object, exluding the
     * object itself.
     *
     * @param class_id the class ID for the object being inserted
     * @param uri the URI to remove
     * @param notifs an optional notification map that will get added
     * to for any URI that is recursively removed, though not the
     * top-level node.
     */
    void removeChildren(class_id_t class_id,
                        const URI& uri,
                        notif_t* notifs);

    /**
     * Queue notifications for dispatch to the given URI and its
     * parents.  These will not be delivered until a call to
     * deliverNotification().
     */
    void queueNotification(class_id_t class_id, const URI& uri,
                           /* out */ notif_t& notifs);

    /**
     * Deliver the notifications to the object store notification
     * queue.
     *
     * @param notifs the notifications to deliver
     */
    void deliverNotifications(const notif_t& notifs);

    /**
     * Get a set of all objects with the given class ID
     *
     * @param class_id the class_id to look up
     * @param output An unordered set that will get the output
     * @throws std::out_of_range if the class is not found
     */
    void getObjectsForClass(class_id_t class_id,
                            /* out */ boost::unordered_set<URI>& output);

private:

    friend class opflex::modb::Region;
    friend class opflex::modb::ObjectStore;

    StoreClient(ObjectStore* store, Region* region,
                bool readOnly = false);

    ObjectStore* store;
    Region* region;
    bool readOnly;
};

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_STORECLIENT_H */
