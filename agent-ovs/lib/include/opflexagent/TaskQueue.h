/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_TASK_QUEUE_H_
#define OPFLEXAGENT_TASK_QUEUE_H_

#include <boost/asio/io_service.hpp>
#include <boost/asio.hpp>

#include <unordered_set>
#include <string>
#include <mutex>
#include <functional>

namespace opflexagent {
using namespace std;

/**
 * Queue tasks using a boost::asio::io_service so that the same task
 * is not queued multiple times
 */
class TaskQueue {
public:
    /**
     * Initialize a task queue using the specified io_service
     * @param io_service the io service to use
     */
    TaskQueue(boost::asio::io_service& io_service);

    /**
     * Dispatch the given task with the specified task ID.  If a task
     * with the given task ID has already been queued and not been
     * executed, the task will not be queued again.  The task can be
     * queued again once it has begun executing.
     *
     * @param taskId a unique ID for the task
     * @param task a function to execute for the task.  This will be
     * copied onto the task queue
     */

    void dispatch(const string& taskId,
                  const function<void ()>& task);
    /**
     * Dispatch the task immediately if delay is false.
     * Otherwise, if a timer is active, ignore dispatch request
     * and return.
     * If timer is not active and this is the first time since startup,
     * then start timer and queue task.
     * If timer is not active and this is not the forst time then
     * dispatch task.
     *
     * @param taskID id of the task assigned by user.
     * @param task task a function to execute for the task.
     * @param delay boolean to indicate if a delay is required.
     */
    void dispatch(const string& taskId,
                      const function<void ()>& task, bool delay);

private:
    void run_task(const string& taskId,
                  const function<void ()>& task);
    void dispatchCb(const boost::system::error_code& ec,
            const string& taskId, const function<void ()>& task);
    boost::asio::io_service& io_service;
    recursive_mutex queueMutex;
    unordered_set<string> queuedItems;
    bool first_time = true;
    bool timer_started = false;
    shared_ptr<boost::asio::deadline_timer> delay_timer;
    const unsigned long DELAY = 5;
    map<const string, const function<void ()>> taskMap;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TASK_QUEUE_H_ */
