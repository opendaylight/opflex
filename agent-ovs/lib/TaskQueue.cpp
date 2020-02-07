/*
 * Copyright (c) 2014-2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/TaskQueue.h>
#include <opflexagent/logging.h>
#include <boost/bind.hpp>

#include <functional>

namespace opflexagent {
using namespace std;

TaskQueue::TaskQueue(boost::asio::io_service& io_service_)
    : io_service(io_service_) {

}

void TaskQueue::run_task(const std::string& taskId,
                         const std::function<void ()>& task) {
    {
        lock_guard<std::recursive_mutex> guard(queueMutex);
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
        lock_guard<std::recursive_mutex> guard(queueMutex);
        if (!queuedItems.insert(taskId).second) return;
    }
    io_service.post([=]() { TaskQueue::run_task(taskId, task); });
}

void TaskQueue::dispatchCb(const boost::system::error_code& ec,
        const std::string& taskId, const std::function<void ()>& task) {
    if (ec) {
        string cat = string(ec.category().name());
        if (!(cat.compare("system") == 0 &&
            ec.value() == 125)) {
            delay_timer->cancel();
            timer_started = false;
        }
        return;
    }
    dispatch(taskId, task);
    lock_guard<recursive_mutex> guard(queueMutex);
    timer_started = false;
}

void TaskQueue::dispatch(const std::string& taskId,
                         const std::function<void ()>& task, bool delay) {
    if (!delay) {
        dispatch(taskId, task);
        return;
    }
    lock_guard<std::recursive_mutex> guard(queueMutex);
    if (timer_started) {
        // nothing to do but wait till timer fires.
        return;
    }
    if (first_time) {
        first_time = false;
        delay_timer.reset(new boost::asio::deadline_timer(io_service,
                            boost::posix_time::seconds(DELAY)));
        delay_timer->async_wait(boost::bind(&TaskQueue::dispatchCb, this,
                            boost::asio::placeholders::error, taskId, task));
        timer_started = true;
        return;
    }
    // this is not the first time and timer has expired.
    // follow regular execution flow.
    dispatch(taskId, task);
}

} // namespace opflexagent
