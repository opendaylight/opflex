/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for RecursiveLockGuard class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "RecursiveLockGuard.h"

namespace opflex {
namespace util {

RecursiveLockGuard::RecursiveLockGuard(uv_mutex_t* mutex_,
                                       uv_key_t* mutex_key_) 
    : mutex(mutex_), mutex_key(mutex_key_), locked(true) {
    void* lkey = uv_key_get(mutex_key);
    if (!lkey) {
        uv_mutex_lock(mutex);
        uv_key_set(mutex_key, (void*)1);
    }
}

RecursiveLockGuard::~RecursiveLockGuard() {
    release();
}

void RecursiveLockGuard::release() {
    void* lkey = uv_key_get(mutex_key);
    if (lkey) {
        uv_mutex_unlock(mutex);
        uv_key_set(mutex_key, 0);
    }
    locked = false;
}

} /* namespace util */
} /* namespace opflex */
