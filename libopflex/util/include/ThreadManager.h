/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ThreadManager.h
 * @brief Interface definition file for thread manager
 */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_UTIL_THREADMANAGER_H
#define OPFLEX_UTIL_THREADMANAGER_H

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <uv.h>

#include "opflex/ofcore/MainLoopAdaptor.h"
#include "opflex/ofcore/OFTypes.h"

namespace opflex {
namespace util {

/**
 * Manage threads and event loops for the OpFlex framework
 */
class ThreadManager : private boost::noncopyable  {
public:
    /**
     * Initialize a task with the given name
     * @param name the name of the task
     * @return a uv_loop that can be used for the task
     */
    uv_loop_t* initTask(const std::string& name);

    /**
     * Start the task with the given name.  If running without an
     * adaptor, starts a thread for the task.
     */
    void startTask(const std::string& name);

    /**
     * Stop the task of the given name.  Call when all handles for the
     * task have been closed.  Will join the thread for the task if
     * running without an adaptor.
     */
    void stopTask(const std::string& name);

    /**
     * Allocate a main loop adaptor and use it for running tasks
     */
    ofcore::MainLoopAdaptor* getAdaptor();

    /**
     * Perform final cleanup
     */
    void stop();

private:
    class Task : private boost::noncopyable {
    public:
        Task() : loop(NULL) {}

        uv_loop_t* loop;
        uv_thread_t thread;
        uv_async_t cleanup;
    };

    class AdaptorImpl : public ofcore::MainLoopAdaptor {
    public:
        virtual void runOnce();
        virtual int getBackendFd();
        virtual int getBackendTimeout();

        uv_loop_t main_loop;
    };

    boost::scoped_ptr<AdaptorImpl> adaptor;

    typedef OF_UNORDERED_MAP<std::string, Task> task_map_t;
    task_map_t task_map;

    static void cleanup_func(uv_async_t* handle);
    static void thread_func(void* task);
};

} /* namespace util */
} /* namespace opflex */

#endif /* OPFLEX_UTIL_THREADMANAGER_H */
