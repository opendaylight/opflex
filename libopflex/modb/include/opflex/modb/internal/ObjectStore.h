/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ObjectStore.h
 * @brief Interface definition file for MODB
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_OBJECTSTORE_H
#define MODB_OBJECTSTORE_H

#include <boost/noncopyable.hpp>
#include <list>
#include <uv.h>

#include "opflex/modb/ModelMetadata.h"
#include "opflex/modb/ClassInfo.h"
#include "opflex/modb/ObjectListener.h"
#include "opflex/modb/mo-internal/StoreClient.h"
#include "opflex/modb/internal/Region.h"
#include "opflex/modb/internal/URIQueue.h"

namespace opflex {
namespace modb {

/**
 * @brief Main interface into the managed object database.
 *
 * The MODB allows you to store and look up managed objects, and get
 * notifications when objects change.
 */
class ObjectStore : private boost::noncopyable {
public:
    /**
     * Construct a new, empty managed object database.
     */
    ObjectStore(util::ThreadManager& threadManager);

    /**
     * Destroy the MODB
     */
    ~ObjectStore();

    /**
     * Add the given model metadata to the managed object database.
     * Must be called before start()
     *
     * @param model the model metadata to add to the object database
     */
    void init(const ModelMetadata& model);

    /**
     * Start the managed object database.  Must be called before
     * writing or reading is allowed on the database.
     */
    void start();

    /**
     * Stop the managed object database
     */
    void stop();

    /**
     * Get the class info object for the given class ID
     * @param class_id the class ID
     * @return a const reference to the class info object
     * @throws std::out_of_range if there is no such class registered
     */
    const ClassInfo& getClassInfo(class_id_t class_id) const;

    /**
     * Get the class info object for the given class name
     * @param class_name the class name
     * @return a const reference to the class info object
     * @throws std::out_of_range if there is no such class registered
     */
    const ClassInfo& getClassInfo(std::string class_name) const;

    /**
     * Get the class info object associated with the given property ID
     * @param prop_id The property ID
     * @return a const reference to the class info object
     * @throws std::out_of_range if there is no such property_id
     * registered
     */
    const ClassInfo& getPropClassInfo(prop_id_t prop_id) const;

    /**
     * Apply the given function to each registered class ID
     * @param apply the function to apply
     * @param data a data member to pass to each function invocation
     */
    void forEachClass(void (*apply)(void*, const ClassInfo&), void* data);

    /**
     * Register a listener for change events related to a particular
     * class.  This listener will be called for any modifications of a
     * particular class or any transitive children of that class.
     *
     * @param class_id the class ID to listen to
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     * @throws std::out_of_range if there is no such class
     * @see ObjectListener
     */
    void registerListener(class_id_t class_id, ObjectListener* listener);

    /**
     * Unregister the specified listener from the specified class ID.
     *
     * @param class_id the class ID
     * @param listener the listener to unregister
     */
    void unregisterListener(class_id_t class_id, ObjectListener* listener);

    /**
     * Get a store client for the specified owner.
     *
     * @param owner the owner field that governs which fields can be edited
     * by the store client
     * @return the store client.  This memory is owned by the object
     * store and must not be freed by the caller.
     * @see ObjectStore::Client
     * @throws std::out_of_range if there is no region with that owner
     */
    mointernal::StoreClient& getStoreClient(const std::string& owner);

    /**
     * Get a store client that can only read from the store, and not
     * write.  This client is not associated with any particular
     * owner.
     *
     * @return the store client
     */
    mointernal::StoreClient& getReadOnlyStoreClient();

    /**
     * Get a store client for the owner associated with the specified
     * class ID
     *
     * @param class_id The class ID.
     * @return the store client.  This memory is owned by the object
     * store and must not be freed by the caller.
     * @see ObjectStore::Client
     * @throws std::out_of_range if there is no region with that owner
     */
    mointernal::StoreClient& getStoreClient(class_id_t class_id);

    /**
     * Get the Region for the specified owner.  This region object is
     * where all the actual object data will be stored.  This is not
     * needed under ordinary circumstances.
     *
     * @param owner the owner ID for the region
     * @return a pointer to the region.  This is owned by the object
     * store and should not be freed by the caller.
     * @throws std::out_of_range if there is no region with that owner
     */
    Region* getRegion(const std::string& owner);

    /**
     * Get the Region for the specified class ID.  This region object
     * is where all the actual object data will be stored.  This is
     * not needed under ordinary circumstances.
     *
     * @param class_id The class ID
     * @return a pointer to the region.  This is owned by the object
     * store and should not be freed by the caller.
     * @throws std::out_of_range if there is no region for the class ID
     */
    Region* getRegion(class_id_t class_id);

    /**
     * Get all the region owners that exist in the MODB
     *
     * @param output a set to receive the owners
     */
    void getOwners(/* out */ OF_UNORDERED_SET<std::string>& output);

private:
    struct ClassContext {
        ClassInfo classInfo;
        std::list<ObjectListener*> listeners;
        Region* region;
    };

    typedef OF_UNORDERED_MAP<std::string, Region*> region_owner_map_t;
    typedef OF_UNORDERED_MAP<class_id_t, ClassContext> class_map_t;
    typedef OF_UNORDERED_MAP<std::string, ClassInfo*> class_name_map_t;
    typedef OF_UNORDERED_MAP<prop_id_t, ClassInfo*> prop_map_t;

    /**
     * Lookup region by owner
     */
    region_owner_map_t region_owner_map;
    /**
     * Look up the class context by the ID
     */
    class_map_t class_map;

    /**
     * Look up the class info by the name
     */
    class_name_map_t class_name_map;

    /**
     * Look up the class info for a property
     */
    prop_map_t prop_map;

    /**
     * A store client that can write anywhere
     */
    mointernal::StoreClient systemClient;

    /**
     * A store client that can only read.
     */
    mointernal::StoreClient readOnlyClient;

    /**
     * handle items from the notification queue
     */
    class NotifQueueProc : public URIQueue::QProcessor {
    public:
        NotifQueueProc(ObjectStore* store);

        // notify all the listeners
        virtual void processItem(const URI& uri,
                                 const boost::any& data);
        virtual const std::string& taskName();
    private:
        ObjectStore* store;
    };

    /**
     * A URI queue to hold notifications to be processed
     */
    NotifQueueProc notif_proc;
    URIQueue notif_queue;

    /**
     * Mutex for accessing listeners
     */
    uv_mutex_t listener_mutex;

    /**
     * Queue a notification to be delivered to the listeners
     */
    void queueNotification(class_id_t class_id, const URI& uri);

    friend class mointernal::StoreClient;
    friend class NotifQueueProc;
};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_OBJECTSTORE_H */
