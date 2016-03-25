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
#include <boost/function.hpp>

#include <opflex/ofcore/OFFramework.h>

namespace ovsagent {

/**
 * Class to generate unique numeric IDs for strings. Also supports
 * persisting the assignments so that they can restored upon restart.
 */
class IdGenerator : private boost::noncopyable {
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
     * Get a unique ID for the given string in the given namespace.
     *
     * @param nmspc Namespace to get an ID from
     * @param str string to get an ID for
     * @return 0 if arguments are invalid, -1 if no more IDs can be
     * generated (for example due to overflow), else ID for the string
     */
    uint32_t getId(const std::string& nmspc, const std::string& str);

    /**
     * Remove the ID assignment for a given string, and move it to the
     * erased list
     *
     * @param nmspc Namespace to remove from
     * @param str string to remove
     */
    void erase(const std::string& nmspc, const std::string& str);

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

    /**
     * The garbage collection callback.  Arguments are the namespace
     * and the string to check.  Returns true if the string remains
     * valid.
     */
    typedef boost::function<bool(const std::string&,
                                 const std::string&)> garbage_cb_t;

    /**
     * Cleanup the string map by verifying that each entry in the map
     * for the given namespace is still a valid object
     *
     * @param ns the namespace to check
     * @param cb the callback to call to verify the namespace
     */
    void collectGarbage(const std::string& ns, garbage_cb_t cb);

    /**
     * A ready-made callback for IDs formed from a managed object URI
     * @param framework the framework object
     * @param ns the namespace from collectGarbage
     * @param str the string from collectGarbage
     */
    template <class MO>
    static bool uriIdGarbageCb(opflex::ofcore::OFFramework& framework,
                               const std::string& ns, const std::string& str) {
        return MO::resolve(framework, opflex::modb::URI(str));
    }

private:
    typedef boost::chrono::steady_clock::time_point time_point;
    typedef boost::chrono::milliseconds duration;

    /**
     * Keeps track of IDs assignments in a namespace.
     */
    struct IdMap : private boost::noncopyable {
        IdMap() : lastUsedId(0) {}

        /**
         * Map of strings to the IDs assigned to them.
         */
        typedef boost::unordered_map<std::string, uint32_t> Str2IdMap;
        Str2IdMap ids;
        uint32_t lastUsedId;

        typedef boost::unordered_map<std::string, time_point> Str2EIdMap;
        Str2EIdMap erasedIds;
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
