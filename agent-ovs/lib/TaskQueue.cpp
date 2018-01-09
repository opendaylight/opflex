/*
 * Copyright (c) 2014-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/TaskQueue.h>
#include <opflexagent/logging.h>

#include <functional>

namespace opflexagent {

TaskQueue::TaskQueue(boost::asio::io_service& io_service_)
    : io_service(io_service_) {

}

void TaskQueue::run_task(const std::string& taskId,
                         const std::function<void ()>& task) {
    {
        std::unique_lock<std::mutex> guard(queueMutex);
        queuedItems.erase(taskId);
    }
    try {
        task();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception while executing task " << taskId
                   << ": " << e.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while executing task " << taskId;
    }
}

void TaskQueue::dispatch(const std::string& taskId,
                         const std::function<void ()>& task) {
    {
        std::unique_lock<std::mutex> guard(queueMutex);
        if (!queuedItems.insert(taskId).second) return;
    }
    io_service.post(std::bind(&TaskQueue::run_task, this, taskId, task));
}

} // namespace opflexagent
