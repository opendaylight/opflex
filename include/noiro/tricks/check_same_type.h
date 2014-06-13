/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _NOIRO__TRICKS__CHECK_SAME_TYPE_H
#define _NOIRO__TRICKS__CHECK_SAME_TYPE_H

#define CHECK_SAME_TYPE(a, b) do { \
    typeof(a) _a; \
    typeof(b) _b; \
    (void) (&_a == &_b); \
    } while (0)

#endif/*_NOIRO__TRICKS__CHECK_SAME_TYPE_H*/
