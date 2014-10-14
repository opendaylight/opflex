/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file LockGuard.h
 * @brief Interface definition file for LockGuard
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_UTIL_LOCKGUARD_H
#define OPFLEX_UTIL_LOCKGUARD_H

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
    LockGuard(uv_mutex_t* mutex);

    /**
     * Release the lock
     */
    ~LockGuard();

 private:
    uv_mutex_t* mutex;

};

} /* namespace util */
} /* namespace opflex */

#endif /* OPFLEX_UTIL_LOCKGUARD_H */
