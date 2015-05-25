/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file LockGuard.hpp
 * @brief Interface definition file for LockGuard
 */
/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_UTIL_INLINELOCKGUARD_HPP
#define OPFLEX_UTIL_INLINELOCKGUARD_HPP

#include <uv.h>

namespace opflex {
namespace util {

/**
 * @brief Lock guard will aquire a libuv mutex on construction and
 * release it on destruction.
 */
class LockGuard {
public:
    /**
     * Acquire the lock
     */
    LockGuard(uv_mutex_t* mutex)
        :
            mutex_(mutex),
            locked(true)
        {
            uv_mutex_lock(mutex);
        }

    /**
     * Release the lock
     */
    ~LockGuard() {
        release();
    }

    /**
     * Release the lock
     */
    void release() {
        if (locked) 
            uv_mutex_unlock(mutex_);
        locked = false;
    }

 private:
    uv_mutex_t* mutex_;
    bool locked;
};

} /* namespace util */
} /* namespace opflex */

#endif /* OPFLEX_UTIL_INLINELOCKGUARD_HPP */
