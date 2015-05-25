/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for TestListener
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_TEST_TESTLISTENER_H
#define MODB_TEST_TESTLISTENER_H

#include <uv.h>
#include <boost/unordered_set.hpp>

#include "opflex/modb/ObjectListener.h"
#include <LockGuard.hpp>

namespace opflex {
namespace modb {

using boost::unordered_set;

class TestListener : public ObjectListener {
public:
    TestListener() {
        uv_mutex_init(&mutex);
    }

    virtual void objectUpdated(class_id_t class_id, const URI& uri) {
        opflex::util::LockGuard guard(&mutex);
        notifs.insert(uri);
    }
    bool contains(const URI& uri) {
        opflex::util::LockGuard guard(&mutex);
        return notifs.find(uri) != notifs.end();
    }

    uv_mutex_t mutex;
    unordered_set<URI> notifs;
};

// wait for a condition to become true because of an event in another
// thread
#define WAIT_FOR(condition, count)                             \
    {                                                          \
        int _c = 0;                                            \
        while (_c < count) {                                   \
           if (condition) break;                               \
           _c += 1;                                            \
           usleep(1000);                                       \
        }                                                      \
        BOOST_CHECK((condition));                              \
    }

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_TEST_TESTLISTENER_H */
