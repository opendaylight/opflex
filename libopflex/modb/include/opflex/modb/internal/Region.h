/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file Region.h
 * @brief Interface definition file for Regions
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_REGION_H
#define MODB_REGION_H

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <string>
#include <uv.h>

#include "opflex/modb/mo-internal/ObjectInstance.h"
#include "opflex/modb/mo-internal/StoreClient.h"
#include "opflex/modb/internal/ClassIndex.h"

namespace opflex {
namespace modb {

/**
 * @brief A region is a storage area specific to a single "owner."
 *
 * The owner of the data stored in a region is the only writer allowed
 * to modify the data in the region, and must ensure that it does not
 * do so concurrently.
 */
class Region {
public:
    /**
     * Construct a new, empty region associated with the given owner
     * identifier.
     */
    Region(ObjectStore* parent, std::string owner);

    /**
     * Destroy the Region
     */
    virtual ~Region();

    /**
     * Get the store client associated with this region
     * @return the store client
     */
    mointernal::StoreClient& getStoreClient() { return client; }

    /**
     * Get the owner associated with this region
     * 
     * @return the owner string
     */
    const std::string& getOwner() const { return owner; }

    /**
     * Add the given class to the region
     *
     * @param class_info the class info associated with the class
     */
    void addClass(const ClassInfo& class_info);

    /**
     * Get the object instance associated with the specified URI
     *
     * @param uri the URI to look up
     * @return a shared ptr to an object instance that must not be
     * modified.
     * @throws std::out_of_range if no such element is present.
     */
    boost::shared_ptr<const mointernal::ObjectInstance> get(const URI& uri);

    /**
     * Set the specified URI to the provided object instance,
     * replacing any existing value
     *
     * @param class_id the class ID for the object being inserted
     * @param uri the URI for the object instance
     * @param oi the object instance to set
     */
    void put(class_id_t class_id, const URI& uri, 
             const boost::shared_ptr<const mointernal::ObjectInstance>& oi);

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
                       const boost::shared_ptr<const mointernal::ObjectInstance>& oi);

    /**
     * Remove the given URI from the region
     *
     * @param class_id the class ID for the object being inserted
     * @param uri the URI to remove
     * @return true if an object was removed
     */
    bool remove(class_id_t class_id, const URI& uri);

    /**
     * Add a parent/child relationship between a parent URI (from any
     * region) to a child URI (in this region).
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID to set for the parent
     * @param child_class the class ID of the child
     * @param child_uri the URI of the child
     * @return true if the child relationship was not already present
     * @throws std::out_of_range if either or both class IDs are not
     * registered
     */
    bool addChild(class_id_t parent_class,
                  const URI& parent_uri, 
                  prop_id_t parent_prop,
                  class_id_t child_class,
                  const URI& child_uri);

    /**
     * Remove a parent/child relationship between a parent URI and a
     * child URI.
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class the class ID of the child
     * @param child_uri the URI of the child
     * @return true if anything was deleted
     * @throws std::out_of_range if either or both class IDs are not
     * registered
     */
    bool delChild(class_id_t parent_class,
                  const URI& parent_uri, 
                  prop_id_t parent_prop,
                  class_id_t child_class,
                  const URI& child_uri);

    /**
     * Clear all children for a parent property of a particular type
     * 
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class the class ID of the children
     */
    void clearChildren(class_id_t parent_class,
                       const URI& parent_uri, 
                       prop_id_t parent_prop,
                       class_id_t child_class);

    /**
     * Get the children of the parent URI and property and put the
     * result into the supplied vector.
     *
     * @param parent_class the class ID of the parent
     * @param parent_uri the URI of the parent object
     * @param parent_prop the property ID in the parent object
     * @param child_class The class of the children to retrieve
     * @param output the output array that will get the output
     * @throws std::out_of_range if either or both class IDs are not
     * registered
     */
    void getChildren(class_id_t parent_class,
                     const URI& parent_uri, 
                     prop_id_t parent_prop,
                     class_id_t child_class,
                     /* out */ std::vector<URI>& output);

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
    const std::pair<URI, prop_id_t>& getParent(class_id_t child_class,
                                               const URI& child);

private:
    /**
     * The store client associated with this region
     */
    mointernal::StoreClient client;

    /**
     * The owner identifier for this region
     */
    std::string owner;

    /**
     * Mutex for serializing access to the region
     */
    uv_mutex_t region_mutex;

    typedef boost::unordered_map<int, ClassIndex> class_map_t;
    typedef boost::unordered_map<URI,
                                 boost::shared_ptr<const mointernal::ObjectInstance> > uri_map_t;

    class_map_t class_map;
    uri_map_t uri_map;
};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_REGION_H */
