/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for LockGuard class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "LockGuard.h"

namespace opflex {
namespace util {

LockGuard::LockGuard(uv_mutex_t* mutex_) : mutex(mutex_) {
    uv_mutex_lock(mutex);
}

LockGuard::~LockGuard() {
    uv_mutex_unlock(mutex);
}

} /* namespace util */
} /* namespace opflex */
