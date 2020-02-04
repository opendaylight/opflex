/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for LoggingListener
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <engine/include/opflex/engine/LoggingListener.h>
#include <modb/include/opflex/modb/internal/ObjectStore.h>
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace engine {

LoggingListener::LoggingListener(modb::ObjectStore* store_)
    : AbstractObjectListener(store_) {
}

LoggingListener::~LoggingListener() {
}

static void register_listeners(void* listener, const modb::ClassInfo& ci) {
    if (ci.getType() == modb::ClassInfo::POLICY ||
        ci.getType() == modb::ClassInfo::RELATIONSHIP ||
        ci.getType() == modb::ClassInfo::REMOTE_ENDPOINT ||
        ci.getType() == modb::ClassInfo::LOCAL_ENDPOINT) {
        LOG(DEBUG) << "Log any changes to " << ci.getName();
        auto ll = (LoggingListener *) listener;
        ll->listen(ci.getId());
    }
}

void LoggingListener::start() {
    LOG(DEBUG) << "Starting MODB change logger";
    store->forEachClass(&register_listeners, this);

//     proc_loop = threadManager.initTask("changelogger");
//     uv_timer_init(proc_loop, &proc_timer);
//     cleanup_async.data = this;
//     uv_async_init(proc_loop, &cleanup_async, cleanup_async_cb);
//     proc_async.data = this;
//     uv_async_init(proc_loop, &proc_async, proc_async_cb);
//     connect_async.data = this;
//     uv_async_init(proc_loop, &connect_async, connect_async_cb);
//     proc_timer.data = this;
//     uv_timer_start(&proc_timer, &timer_callback,
//                    processingDelay, processingDelay);
//     threadManager.startTask("changelogger");
}

void LoggingListener::stop() {
        LOG(DEBUG) << "Stopping MODB change logger";
        unlisten();
//        uv_async_send(&cleanup_async);
//        threadManager.stopTask("changelogger");
}
//
//void LoggingListener::timer_callback(uv_timer_t* handle) {
//    LoggingListener* ll = (LoggingListener*)handle->data;
//     ll->doProcess();
//}
//
//void LoggingListener::cleanup_async_cb(uv_async_t* handle) {
//    LoggingListener* ll = (LoggingListener*)handle->data;
//    uv_timer_stop(&ll->proc_timer);
//    uv_close((uv_handle_t*)&ll->proc_timer, NULL);
//    uv_close((uv_handle_t*)&ll->proc_async, NULL);
//    uv_close((uv_handle_t*)&ll->connect_async, NULL);
//    uv_close((uv_handle_t*)handle, NULL);
//}

void LoggingListener::objectUpdated(modb::class_id_t class_id,
                                    const modb::URI& uri) {
    LOG(INFO) << "Change in MO " << uri;
}


} /* namespace engine */
} /* namespace opflex */