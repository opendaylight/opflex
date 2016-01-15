/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ThreadManager class.
 *
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ThreadManager.h"

namespace opflex {
namespace util {

uv_loop_t* ThreadManager::initTask(const std::string& name) {
    Task& task = task_map[name];

    uv_loop_t* loop = external_loop;
    if (!external_loop) {
        task.loop = new uv_loop_t;
        uv_loop_init(task.loop);
        loop = task.loop;
    }
    uv_async_init(loop, &task.cleanup, cleanup_func);
    task.cleanup.data = &task;

    return loop;
}

void ThreadManager::startTask(const std::string& name) {
    Task& task = task_map.at(name);
    if (!external_loop) {
        uv_thread_create(&task.thread, thread_func, &task);
    }
}

void ThreadManager::stopTask(const std::string& name) {
    task_map_t::iterator it = task_map.find(name);
    if (it != task_map.end()) {
        Task& task = it->second;
        uv_async_send(&task.cleanup);

        if (!external_loop) {
            uv_thread_join(&task.thread);
            uv_loop_close(task.loop);
            delete task.loop;
            task.loop = NULL;
        }
    }
}

void ThreadManager::cleanup_func(uv_async_t* handle) {
    Task* task = static_cast<Task*>(handle->data);
    uv_close((uv_handle_t*)handle, NULL);
}

void ThreadManager::thread_func(void* taskptr) {
    Task* task = static_cast<Task*>(taskptr);
    uv_run(task->loop, UV_RUN_DEFAULT);
}

} /* namespace util */
} /* namespace opflex */
