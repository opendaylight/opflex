/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Definition of IdGenerator class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_IDGENERATOR_H_
#define OVSAGENT_IDGENERATOR_H_

#include <string>
#include <boost/unordered_map.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/chrono/system_clocks.hpp>

#include "opflex/modb/URI.h"

namespace ovsagent {

/**
 * Class to generate unique numeric IDs for URIs. Also supports
 * persisting the assignments so that they can restored upon restart.
 */
class IdGenerator {
public:
    /**
     * Initialize a new id generator using the default cleanup
     * interval
     **/
    IdGenerator();

    /**
     * Initialize a new id generator using the specified cleanup
     * interval
     *
     * @param cleanupInterval the amount of time to keep ids before
     * purging them on cleanup
     **/
    IdGenerator(boost::chrono::milliseconds cleanupInterval);

    /**
     * Initialize an ID namespace for generating IDs. If an ID file for
     * for the namespace is found, loads the assignments from the file.
     *
     * @param nmspc ID namespace to initialize
     */
    void initNamespace(const std::string& nmspc);

    /**
     * Get a unique ID for the given URI in the given namespace.
     *
     * @param nmspc Namespace to get an ID from
     * @param uri URI to get an ID for
     * @return 0 if arguments are invalid, -1 if no more IDs can be
     * generated (for example due to overflow), else ID for the URI
     */
    uint32_t getId(const std::string& nmspc, const opflex::modb::URI& uri);

    /**
     * Remove the ID assignment for a given URI, and move it to the
     * erased list
     *
     * @param nmspc Namespace to remove from
     * @param uri URI to remove
     */
    void erase(const std::string& nmspc, const opflex::modb::URI& uri);

    /**
     * Purge erased entries that are sufficiently old
     */
    void cleanup();

    /**
     * Gets the name of the file used for persisting IDs.
     *
     * @param nmspc Namespace to get file for
     * @return Name of ID file
     */
    std::string getNamespaceFile(const std::string& nmspc);

    /**
     * Set the directory location where ID files should be persisted.
     *
     * @param dir Filesystem path to a directory
     */
    void setPersistLocation(const std::string& dir) {
        persistDir = dir;
    }

private:
    typedef boost::chrono::steady_clock::time_point time_point;
    typedef boost::chrono::milliseconds duration;

    /**
     * Keeps track of IDs assignments in a namespace.
     */
    struct IdMap {
        IdMap() : lastUsedId(0) {}

        /**
         * Map of URIs to the IDs assigned to them.
         */
        typedef boost::unordered_map<opflex::modb::URI, uint32_t> Uri2IdMap;
        Uri2IdMap ids;
        uint32_t lastUsedId;

        typedef boost::unordered_map<opflex::modb::URI, time_point> Uri2EIdMap;
        Uri2EIdMap erasedIds;
    };

    /**
     * Save ID assignment to file (which determined from the namespace).
     *
     * @param nmspc Namespace to save
     * @param idmap Assignments to save
     */
    void persist(const std::string& nmspc, IdMap& idmap);

    /**
     * Map of ID namespaces to the assignment within that namespace.
     */
    typedef boost::unordered_map<std::string, IdMap> NamespaceMap;
    boost::unordered_map<std::string, IdMap> namespaces;
    std::string persistDir;
    duration cleanupInterval;
};

} // ovsagent

#endif // OVSAGENT_IDGENERATOR_H_
