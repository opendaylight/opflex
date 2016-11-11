/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ClassIndex.h
 * @brief Interface definition file for ClassIndex
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_CLASSINDEX_H
#define MODB_CLASSINDEX_H

#include <utility>

#include "opflex/modb/URI.h"
#include "opflex/modb/ClassInfo.h"

namespace opflex {
namespace modb {

/**
 * @brief A class index represents state for instances of a given class in
 * the managed object database.
 *
 * Note that because any given class can belong to only one region,
 * and a region has only one owner, access to this index can have at
 * most one writer, but can have multiple concurrent readers.
 */
class ClassIndex {
public:
    /**
     * Default constructor
     */
    ClassIndex();

    /**
     * Destroy the class index
     */
    ~ClassIndex();

    /**
     * Add a new object instance
     * @param uri the URI of the managed object instance
     */
    void addInstance(const URI& uri);

    /**
     * Delete an instance.  Note that this must not be called before
     * all parent/child links have been removed to this instance, both
     * in the current and other regions.
     *
     * @param uri the URI of the instance to delete
     */
    void delInstance(const URI& uri);

    /**
     * Add the referenced child instance as a child for the given
     * parent.
     * @param parent the URI of the parent object
     * @param parent_prop The property ID of the parent property to
     * set
     * @param child the URI of the child object
     * @return true if the child relationship was not already present
     */
    bool addChild(const URI& parent, prop_id_t parent_prop,
                  const URI& child);

    /**
     * Removed the referenced child instance as a child for the given
     * parent.
     * @param parent the URI of the parent object
     * @param parent_prop The property ID of the parent property
     * @param child the URI of the child object
     * @return whether the value was deleted
     */
    bool delChild(const URI& parent, prop_id_t parent_prop,
                  const URI& child);

    /**
     * Get the children of the parent URI and property and put the
     * result into the supplied vector.
     *
     * @param parent the URI of the parent object
     * @param parent_prop The property ID of the parent property
     * @param output the output array that will get the output
     */
    void getChildren(const URI& parent, prop_id_t parent_prop,
                     /* out */ std::vector<URI>& output) const ;

    /**
     * Get the parent for the given child URI.
     *
     * @param child the URI of the child object
     * @return a (URI, prop_id_t) pair which is the URI of the parent
     * and the property that represents the relation.
     * @throws std::out_of_range of the child URI is not found or has
     * no parent
     */
    const std::pair<URI, prop_id_t>& getParent(const URI& child) const;

    /**
     * Get the parent for the given child URI.
     *
     * @param child the URI of the child object
     * @param parent if parent is found, a (URI, prop_id_t) pair which
     * is the URI of the parent and the property that represents the relation.
     * @return true if parent is found, false otherwise
     */
    bool getParent(const URI& child,
                   /* out */ std::pair<URI, prop_id_t>& parent) const;

    /*
     * Check if the given URI has a parent
     *
     * @param child the URI of the child object
     * @return true if the child URI has a parent
     */
    bool hasParent(const URI& child) const;

    /**
     * Get all URIs for the class
     *
     * @param output an unordered_set to receive the output
     */
    void getAll(OF_UNORDERED_SET<URI>& output) const;

private:
    typedef OF_UNORDERED_SET<URI> uri_set_t;
    typedef OF_UNORDERED_MAP<prop_id_t, uri_set_t> prop_uri_map_t;
    typedef OF_UNORDERED_MAP<URI, prop_uri_map_t> uri_prop_uri_map_t;
    typedef OF_UNORDERED_MAP<URI, std::pair<URI, prop_id_t> > uri_prop_map_t;

    /**
     * The child map allows us to look up all the children of this
     * class index's type for a given parent URI
     */
    uri_prop_uri_map_t child_map;

    /**
     * Maps child URIs to their parents.
     */
    uri_prop_map_t parent_map;

    /**
     * The instance map gives us a list of all managed objects of this
     * class index's type.
     */
    OF_UNORDERED_SET<URI> instance_map;

    void doDelCMap(const URI& parent, prop_id_t parent_prop,
                   const URI& child);

};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_CLASSINDEX_H */
