/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ofcore_c.h
 * @brief C wrapper core defininitions
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEX_C_OFCORE_H
#define OPFLEX_C_OFCORE_H

#include <stdint.h>

/**
 * @defgroup cwrapper C Wrapper Interface
 *
 * The C interface wraps the native C++ interface and allows using the
 * OpFlex framework from C code.
 *
 * @addtogroup cwrapper 
 * @{
 */

/**
 * @defgroup ccore Core
 * Defines basic library types and definitions
 * @{
 */

/**
 * @defgroup cdefs Definitions
 * Defines status codes and other basic definitions
 * @{
 */

/**
 * A successful error return
 */
#define OF_ESUCCESS 0
/**
 * A generic failure code
 */
#define OF_EFAILED 1
/**
 * A failure caused by problems related to memory allocation
 */
#define OF_EMEMORY 2
/**
 * A generic logic error caused by errors in the user logic
 */
#define OF_ELOGIC 10
/**
 * You have requested data that is not available or is out of range
 * for the requested type.
 */
#define OF_EOUTOFRANGE 11
/**
 * An argument to the function was invalid
 */
#define OF_EINVALID_ARG 12
/**
 * A generic runtime error
 */
#define OF_ERUNTIME 10

/**
 * Check whether a status code is successful
 */
#define OF_IS_SUCCESS(statusc) ((statusc) == OF_ESUCCESS)

/**
 * Check whether a status code is failed
 */
#define OF_IS_FAILURE(statusc) (!OF_IS_SUCCESS(statusc))

/**
 * An opflex status code
 */
typedef int ofstatus;

/**
 * Base type for all OpFlex object pointers
 */
typedef void* ofobj_p;

/**
 * A unique class ID
 */
typedef uint64_t class_id_t;

/** @} defs */
/** @} ccore */
/** @} cwrapper */

#endif /* OPFLEX_C_OFCORE_H */
