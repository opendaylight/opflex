/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file Processor.h
 * @brief Interface definition file for Processor
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_ENGINE_PROCESSOR_H
#define OPFLEX_ENGINE_PROCESSOR_H

#include <vector>
#include <utility>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/unordered_set.hpp>
#include <uv.h>

#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/modb/mo-internal/StoreClient.h"

#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/engine/internal/MOSerializer.h"
#include "opflex/engine/internal/AbstractObjectListener.h"

namespace opflex {
namespace engine {

/**
 * @brief The processor synchronizes the state of the local MODB with
 * the Opflex server as needed.
 *
 * The processor also maintains an index containing protocol state
 * information for each of the managed objects in the MODB.
 */
class Processor : public internal::AbstractObjectListener,
                  public internal::HandlerFactory {
public:
    /**
     * Construct a processor associated with the given object store
     * @param store the modb::ObjectStore
     */
    Processor(modb::ObjectStore* store);

    /**
     * Destroy the processor
     */
    ~Processor();

    /**
     * Set the opflex identity information for this framework
     * instance.
     *
     * @param name the unique name for this opflex component within
     * the policy domain
     * @param domain the globally unique name for this policy domain
     * @see opflex::ofcore::OFFramework::addOpflexIdentity
     */
    void setOpflexIdentity(const std::string& name,
                           const std::string& domain);

    /**
     * Add an OpFlex peer.
     *
     * @param hostname the hostname or IP address to connect to
     * @param port the TCP port to connect on
     * @see opflex::ofcore::OFFramework::addPeer
     */
    virtual void addPeer(const std::string& hostname,
                         int port);

    /**
     * Start the processor thread.  Should call only after the
     * underlying object store is started.
     */
    void start();

    /**
     * Stop the processor thread.  Should call before stopping the
     * underlying object store.
     */
    void stop();
    
    /**
     * Get the parent store
     */
    modb::ObjectStore* getStore() { return store; }

    /**
     * Get the MO serializer
     */
    internal::MOSerializer& getSerializer() { return serializer; }

    /**
     * Get the MO serializer
     */
    modb::mointernal::StoreClient* getSystemClient() { return client; }

    // See AbstractObjectListener::objectUpdated
    virtual void objectUpdated(modb::class_id_t class_id, 
                               const modb::URI& uri);

    /**
     * Get the reference count for an object
     */
    size_t getRefCount(const modb::URI& uri);

    /**
     * Set the processing delay for unit tests
     */
    void setDelay(uint64_t delay) { processingDelay = delay; }

    // See HandlerFactory::newHandler
    virtual 
    internal::OpflexHandler* newHandler(internal::OpflexConnection* conn);

    /**
     * Get the opflex connection pool for the processor
     */
    internal::OpflexPool& getPool() { return pool; }

private:
    /**
     * The system store client
     */
    modb::mointernal::StoreClient* client;

    /**
     * an MO serializer
     */
    internal::MOSerializer serializer;

    /**
     * The pool of Opflex connections
     */
    internal::OpflexPool pool;

    /**
     * The status of items in the MODB with respect to the opflex
     * protocol
     */
    enum ItemState {
        NEW,
        UPDATED,
        UNRESOLVED,
        IN_SYNC,
        PENDING_DELETE,
        DELETED
    };

    class item_details {
    public:
        /**
         * The class ID of the MO
         */
        modb::class_id_t class_id;
        /**
         * The update interval that controls expiration of the object
         * in milliseconds.
         */
        uint64_t refresh_rate;
        /**
         * State of the managed object
         */
        ItemState state;
        
        /**
         * The number of times the object has been referenced in the
         * system.
         */
        size_t refcount;

        /**
         * Outgoing URI references
         */
        boost::unordered_set<modb::reference_t> urirefs;
    };

    /**
     * The data stored in the object state index
     */
    class item {
    public:
        item() : uri(""), expiration(0), details(NULL) {}
        item(const item& i) : uri(i.uri), expiration(i.expiration) {
            details = new item_details(*i.details);
        }
        item(const modb::URI& uri_, modb::class_id_t class_id_,
             uint64_t expiration_, int64_t refresh_rate_,
             ItemState state_)
            : uri(uri_), expiration(expiration_) {
            details = new item_details();
            details->class_id = class_id_;
            details->refresh_rate = refresh_rate_;
            details->state = state_;
            details->refcount = 0;
        }
        ~item() { if (details) delete details; }
        item& operator=( const item& rhs ) {
            uri = rhs.uri;
            expiration = rhs.expiration;
            details = new item_details(*rhs.details);
            return *this;
        }

        /**
         * The URI of the MO
         */
        modb::URI uri;
        /**
         * The next expiration interval when an action needs to be
         * taken, such as refreshing the object resolution.  The value
         * is the time when the event should be processed.  A value of
         * 0 means that it should be processed immediately, while a
         * value of UINT64_MAX means that there is no pending event to
         * process.
         */
        uint64_t expiration;

        /**
         * Detailed item information
         */
        item_details* details;
    };

    // tag for expiration index
    struct expiration_tag{};
    // tag for expiration index
    struct uri_tag{};

    typedef boost::multi_index::multi_index_container<
        item,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<uri_tag>,
                boost::multi_index::member<item, 
                                           modb::URI, 
                                           &item::uri> > ,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<expiration_tag>,
                boost::multi_index::member<item, 
                                           uint64_t, 
                                           &item::expiration> >
            >
        > object_state_t;

    typedef object_state_t::index<expiration_tag>::type obj_state_by_exp;
    typedef object_state_t::index<uri_tag>::type obj_state_by_uri;

    /**
     * Functor for updating the expiration in the object state index
     */
    class change_expiration {
    public:
        change_expiration(uint64_t new_exp);
        
        void operator()(Processor::item& i);
    private:
        uint64_t new_exp;
    };

    /**
     * Store and index the state of managed objects
     */
    object_state_t obj_state;
    uv_mutex_t item_mutex;
    uv_cond_t item_cond;

    /**
     * Processing thread
     */
    uv_thread_t proc_thread;
    volatile bool proc_shouldRun;
    static void proc_thread_func(void* processor);

    /**
     * Processing delay to allow batching updates
     */
    uint64_t processingDelay;

    /**
     * Timer to wake up processing thread periodically
     */
    uv_loop_t timer_loop;
    uv_thread_t timer_loop_thread;
    uv_timer_t proc_timer;
    static void timer_thread_func(void* processor);
    static void timer_callback(uv_timer_t* handle);

    bool hasWork(/* out */ obj_state_by_exp::iterator& it);
    void addRef(obj_state_by_exp::iterator& it,
                const modb::reference_t& up);
    void removeRef(obj_state_by_exp::iterator& it,
                   const modb::reference_t& up);
    void processItem(obj_state_by_exp::iterator& it);
    void updateItemExpiration(obj_state_by_exp::iterator& it);
    bool isOrphan(const item& item);
};

} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_PROCESSOR_H */
