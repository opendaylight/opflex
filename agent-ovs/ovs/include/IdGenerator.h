/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Definition of IdGenerator class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_IDGENERATOR_H_
#define OPFLEXAGENT_IDGENERATOR_H_

#include <opflex/ofcore/OFFramework.h>

#include <boost/optional.hpp>

#include <string>
#include <set>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>

namespace opflexagent {

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
    IdGenerator(std::chrono::milliseconds cleanupInterval);

    /**
     * Initialize an ID namespace for generating IDs. If an ID file for
     * for the namespace is found, loads the assignments from the file.
     *
     * @param nmspc ID namespace to initialize
     * @param minId the minimum ID allowed for the namespace
     * @param maxId the maximum ID allowed for the namespace
     */
    void initNamespace(const std::string& nmspc,
                       uint32_t minId = 1,
                       uint32_t maxId = (1 << 31));

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
     * Get the number of IDs remaining for a namespace
     * @param nmspc the namespace to check
     * @return the number of remaining IDs
     */
    uint32_t getRemainingIds(const std::string& nmspc);

    /**
     * Get the number of sparse ranges for a namespace.  This is only
     * useful for testing purposes.
     * @param nmspc the namespace to chec
     * @return the number of remaining free ranges
     */
    uint32_t getFreeRangeCount(const std::string& nmspc);

    /**
     * Purge erased entries that are sufficiently old
     */
    void cleanup();

    /**
     * Function that can be registered as a hook for allocation of an
     * ID.  A false return value indicates allocation should be
     * canceled.
     */
    typedef std::function<bool(const std::string&, uint32_t)> alloc_hook_t;

    /**
     * Set an allocation callback hook called when a new ID is
     * allocated
     *
     * @param nmspc the namespace to register the hook for
     * @param allocHook the callback to register
     */
    void setAllocHook(const std::string& nmspc, alloc_hook_t& allocHook);

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
    typedef std::function<bool(const std::string&,
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
        return (bool)MO::resolve(framework, opflex::modb::URI(str));
    }

    /**
     * Get the string for the given ID in the given namespace.
     *
     * @param nmspc Namespace to get an ID from
     * @param id to get an str for
     * @return The string associated with id, or boost::none if not
     * found
     */
    boost::optional<std::string> getStringForId(const std::string& nmspc,
                                                uint32_t id);

private:
    typedef std::chrono::steady_clock::time_point time_point;
    typedef std::chrono::milliseconds duration;

    struct id_range {
        id_range(uint32_t start_, uint32_t end_) :
            start(start_), end(end_) { }

        uint32_t start;
        uint32_t end;
    };

    friend bool operator<(const id_range& lhs, const id_range& rhs);
    friend bool operator==(const id_range& lhs, const id_range& rhs);
    friend bool operator!=(const id_range& lhs, const id_range& rhs);

    /**
     * Keeps track of IDs assignments in a namespace.
     */
    struct IdMap : private boost::noncopyable {
        /**
         * Map of strings to the IDs assigned to them.
         */
        typedef std::unordered_map<std::string, uint32_t> Str2IdMap;
        Str2IdMap ids;

        std::set<id_range> freeIds;

        typedef std::unordered_map<std::string, time_point> Str2EIdMap;
        Str2EIdMap erasedIds;

        typedef std::unordered_map<uint32_t, std::string> Id2StrMap;
        Id2StrMap  reverseMap;

        boost::optional<alloc_hook_t> allocHook;
    };

    /**
     * Save ID assignment to file (which determined from the namespace).
     *
     * @param nmspc Namespace to save
     * @param idmap Assignments to save
     */
    void persist(const std::string& nmspc, IdMap& idmap);
    uint32_t getRemainingIdsLocked(const std::string& nmspc);

    std::mutex id_mutex;

    /**
     * Map of ID namespaces to the assignment within that namespace.
     */
    typedef std::unordered_map<std::string, IdMap> NamespaceMap;
    std::unordered_map<std::string, IdMap> namespaces;

    std::string persistDir;
    duration cleanupInterval;
};


} // ovsagent

#endif // OPFLEXAGENT_IDGENERATOR_H_
