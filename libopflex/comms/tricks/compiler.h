/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__NOIRO__TRICKS__COMPILER_H
#define _INCLUDE__NOIRO__TRICKS__COMPILER_H

#define STATIC_ASSERT(COND,MSG) \
    typedef char static_assertion_##MSG[(COND)?1:-1] __attribute__((unused))

/* FIXME: TODO: 
 *
 * for gcc >= 4.3, use:

#define CTC(X) \
    ({ extern int __attribute__((error("assertion failure: '" #X "' not true")))
    compile_time_check(); ((X)?0:compile_time_check()),0; })

 * as it gives a nicer error message
 */

#endif /* _INCLUDE__NOIRO__TRICKS__COMPILER_H */
