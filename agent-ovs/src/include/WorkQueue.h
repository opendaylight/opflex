/*
 * Definition of the WorkQueue class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_WORKQUEUE_H_
#define OVSAGENT_WORKQUEUE_H_

#include <list>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace ovsagent {

typedef boost::function<void ()>    WorkItem;
typedef std::list<WorkItem>         WorkItemList;

/**
 * Class that provides single-threaded execution of a
 * set of work-items. Creates a thread of its own
 * to execute the enqueued work-items.
 */
class WorkQueue {
public:
    /**
     * Default constructor
     */
    WorkQueue() : procThread(NULL), isStopping(false) {}

    /**
     * Start processing queued work items.
     */
    void start();

    /**
     * Stop accepting and processing work items.
     */
    void stop();

    /**
     * Enqueue a work-item for execution.
     *
     * @param w the work-item to execute
     */
    void enqueue(const WorkItem& w);

    /** Interface: Boost thread */
    void operator()();
private:
    WorkItemList workQ;

    boost::thread *procThread;
    boost::mutex queueMtx;
    boost::condition_variable queueCondVar;

    bool isStopping;
};

} //namespace ovsagent

#endif  // OVSAGENT_WORKQUEUE_H_
