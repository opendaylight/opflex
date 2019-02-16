/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file URIQueue.h
 * @brief Interface definition file for MODB
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_URIQUEUE_H
#define MODB_URIQUEUE_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/any.hpp>
#include <uv.h>

#include "opflex/modb/URI.h"
#include "ThreadManager.h"

namespace opflex {
namespace modb {

/**
 * @brief A queue containing URIs that consolidates items and
 * processes them in order.
 *
 * Adding a URI to the queue that is already in the queue may not
 * change the queue.  This ensures the queue length is bounded by the
 * number of unique URIs
 */
class URIQueue {
public:
    /**
     * @brief An abstract base class for registering a processor
     * function
     *
     * The Processor is invoked while processing the items in the URI
     * queue
     */
    class QProcessor {
    public:
        /**
         * Get a name for the task associated with this queue
         */
        virtual const std::string& taskName() = 0;

        /**
         * Process the item in the queue.  If this function blocks or
         * does any costly processing then it will stall processing of
         * the URI queue.
         *
         * @param uri the URI for the item
         * @param data the data associated with the item
         */
        virtual void processItem(const URI& uri,
                                 const boost::any& data) = 0;
    };

    /**
     * Construct a new URI queue
     */
    URIQueue(QProcessor* processor, util::ThreadManager& threadManager);

    /**
     * Destroy the queue
     */
    ~URIQueue();

    /**
     * Start the processor thread
     */
    void start();

    /**
     * Stop the processor thread
     */
    void stop();

    /**
     * Queue an item to be processed by the processor thread.
     *
     * @param uri the URI of the item
     * @param data a data item associated with the item
     */
    void queueItem(const URI& uri, const boost::any& data);

private:
    /**
     * The processor that will handle queue items
     */
    QProcessor* processor;

    /**
     * Thread manager
     */
    util::ThreadManager& threadManager;

    /**
     * The data stored in a queue item
     */
    struct item {
        item() : uri("") {}
        item(const URI& uri_, const boost::any& data_)
            : uri(uri_), data(data_) { }

        URI uri;
        boost::any data;
    };

    typedef boost::multi_index::multi_index_container<
        item,
        boost::multi_index::indexed_by<
            boost::multi_index::sequenced<>,
            boost::multi_index::hashed_unique<
                boost::multi_index::member<item,
                                           URI,
                                           &item::uri> >
            >
        > item_queue_t;

    /**
     * A notification queue that will be have duplicates removed
     */
    item_queue_t item_queue;
    uv_loop_t* item_loop;
    uv_mutex_t item_mutex;
    uv_async_t item_async;
    uv_async_t cleanup_async;

    volatile bool proc_shouldRun;
    static void proc_async_func(uv_async_t* handle);
    static void cleanup_async_func(uv_async_t* handle);
};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_URIQUEUE_H */
