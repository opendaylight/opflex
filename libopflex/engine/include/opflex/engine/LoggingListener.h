/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file LoggingListener.h
 * @brief Interface definition file for Processor
 */
/*
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_LOGGINGLISTENER_H
#define OPFLEX_LOGGINGLISTENER_H

#include <engine/include/opflex/engine/internal/AbstractObjectListener.h>
#include <uv.h>
#include <util/include/ThreadManager.h>

namespace opflex {
namespace engine {

/**
 * Logs details of any policy changes in MODB
 */
class LoggingListener : public internal::AbstractObjectListener {

private:

//    /**
//     * Thread manager
//     */
//    util::ThreadManager& threadManager;
//
//    /**
//     * Processing thread
//     */
//    uv_loop_t* proc_loop;
//    uv_async_t cleanup_async;
//    uv_async_t proc_async;
//    uv_async_t connect_async;
//    uv_timer_t proc_timer;
//
//    /**
//     * Processing delay to allow batching updates
//     */
//    uint64_t processingDelay;
//
//    /**
//     * Amount of time to wait before retrying message sends
//     */
//    uint64_t retryDelay;
//
//    static void timer_callback(uv_timer_t* handle);
//    static void cleanup_async_cb(uv_async_t *handle);
//    static void proc_async_cb(uv_async_t *handle);
//    static void connect_async_cb(uv_async_t *handle);

public:
    /**
     * Construct a logging listener associated with the given object store
     * @param store the modb::ObjectStore
     */
    LoggingListener(modb::ObjectStore* store);

    /**
     * Destroy the logging listener
     */
    ~LoggingListener();

    /**
     * Start the logging listener thread.  Should call only after the
     * underlying object store is started.
     */
    void start();

    /**
     * Stop the logging listener thread.  Should call before stopping the
     * underlying object store.
     */
    void stop();

    /**
     * Get the parent store
     */
    modb::ObjectStore* getStore() { return store; }

    // See AbstractObjectListener::objectUpdated
    virtual void objectUpdated(modb::class_id_t class_id,
                               const modb::URI& uri);

//    /**
//     * Set the processing delay for unit tests
//     */
//    void setProcDelay(uint64_t delay) { processingDelay = delay; }
//
//    /**
//     * Set the message retry delay for unit tests
//     */
//    void setRetryDelay(uint64_t delay) { retryDelay = delay; }
};

} /* namespace engine */
} /* namespace opflex */
#endif //OPFLEX_LOGGINGLISTENER_H
