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

#include <unordered_set>
#include <string>
#include <mutex>
#include <functional>

namespace opflexagent {

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
    void dispatch(const std::string& taskId,
                  const std::function<void ()>& task);

private:
    void run_task(const std::string& taskId,
                  const std::function<void ()>& task);

    boost::asio::io_service& io_service;
    std::mutex queueMutex;
    std::unordered_set<std::string> queuedItems;
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TASK_QUEUE_H_ */
