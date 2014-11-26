/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file RecursiveLockGuard.h
 * @brief Interface definition file for LockGuard
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_UTIL_RECURSIVE_LOCKGUARD_H
#define OPFLEX_UTIL_RECURSIVE_LOCKGUARD_H

#include <LockGuard.h>

namespace opflex {
namespace util {

/**
 * A version of LockGuard that allows the same mutex to be acquired
 * multiple times by the same thread
 */
class RecursiveLockGuard {
public:
    /**
     * Acquire the lock
     */
    RecursiveLockGuard(uv_mutex_t* mutex, uv_key_t* mutex_key);

    /**
     * Release the lock
     */
    ~RecursiveLockGuard();

    /**
     * Release the lock
     */
    virtual void release();

 private:
    uv_mutex_t* mutex;
    uv_key_t* mutex_key;
    bool locked;
};

} /* namespace util */
} /* namespace opflex */

#endif /* OPFLEX_UTIL_RECURSIVE_LOCKGUARD_H */
