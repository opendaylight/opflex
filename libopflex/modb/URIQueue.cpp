/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for URIQueue class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <boost/foreach.hpp>
#include "opflex/modb/internal/URIQueue.h"

#include "LockGuard.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace modb {

URIQueue::URIQueue(QProcessor* processor_, util::ThreadManager& threadManager_)
    : processor(processor_), threadManager(threadManager_),
      proc_shouldRun(false) {
    uv_mutex_init(&item_mutex);
}

URIQueue::~URIQueue() {
    stop();
    uv_mutex_destroy(&item_mutex);
}

// listen on the item queue and dispatch events where required
void URIQueue::proc_async_func(uv_async_t* handle) {
    URIQueue* queue = static_cast<URIQueue*>(handle->data);

    if (queue->proc_shouldRun) {
        item_queue_t toProcess;
        {
            util::LockGuard guard(&queue->item_mutex);
            toProcess.swap(queue->item_queue);
        }

        BOOST_FOREACH (const URIQueue::item& d, toProcess) {
            if (!queue->proc_shouldRun) return;
            try {
                queue->processor->processItem(d.uri, d.data);
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Exception while processing notification queue: "
                           << ex.what();
            } catch (...) {
                LOG(ERROR) << "Unknown error processing notification queue";
            }
        }
    }
}

void URIQueue::cleanup_async_func(uv_async_t* handle) {
    URIQueue* queue = static_cast<URIQueue*>(handle->data);
    uv_close((uv_handle_t*)&queue->item_async, NULL);
    uv_close((uv_handle_t*)handle, NULL);
}

void URIQueue::start() {
    proc_shouldRun = true;
    item_loop = threadManager.initTask(processor->taskName());
    uv_async_init(item_loop, &item_async, proc_async_func);
    uv_async_init(item_loop, &cleanup_async, cleanup_async_func);
    item_async.data = this;
    cleanup_async.data = this;

    threadManager.startTask(processor->taskName());
}

void URIQueue::stop() {
    if (proc_shouldRun) {
        proc_shouldRun = false;
        uv_async_send(&cleanup_async);
        threadManager.stopTask(processor->taskName());
    }
}

void URIQueue::queueItem(const URI& uri, const boost::any& data) {
    {
        util::LockGuard guard(&item_mutex);
        item_queue.push_back(item(uri, data));
        uv_async_send(&item_async);
    }
}

} /* namespace modb */
} /* namespace opflex */
