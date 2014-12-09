/*
 * Implementation of WorkQueue class
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>
#include <boost/thread/lock_types.hpp>
#include "logging.h"
#include "WorkQueue.h"

using namespace std;
using namespace boost;
using namespace ovsagent;

typedef unique_lock<mutex> mutex_guard;

namespace ovsagent {

void WorkQueue::enqueue(const WorkItem& w) {
    if (isStopping) {
        return;
    }
    mutex_guard lock(queueMtx);
    workQ.push_back(w);
    queueCondVar.notify_one();
}

void WorkQueue::start() {
    procThread = new boost::thread(boost::ref(*this));
}

void WorkQueue::stop() {
    isStopping = true;
    if (procThread) {
        mutex_guard lock(queueMtx);
        queueCondVar.notify_one();
        lock.unlock();

        procThread->join();
        procThread = NULL;
    }
}

void WorkQueue::operator()() {
    while (!isStopping) {
        mutex_guard lock(queueMtx);
        while (!isStopping && workQ.empty()) {
            queueCondVar.wait(lock);
        }
        if (isStopping) {
            return;
        }
        WorkItemList wl;
        wl.swap(workQ);
        lock.unlock();

        BOOST_FOREACH (WorkItem& w, wl) {
            w();
        }
    }
}

} // namespace ovsagent
