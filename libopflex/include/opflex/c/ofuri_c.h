/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ofuri_c.h
 * @brief C wrapper for URI
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ofcore_c.h"

#ifndef OPFLEX_C_OFURI_H
#define OPFLEX_C_OFURI_H

/**
 * @addtogroup cwrapper
 * @{
 * @addtogroup cmodb
 * @{
 */

/**
 * @defgroup curi URI
 * A URI is used to identify managed objects in the MODB.
 *
 * It takes the form of a series of names and value of naming
 * properties such as "/childname1/5/childname2/8/value2" that
 * represents a unique path from the root of the tree to the specific
 * child.
 * @{
 */

/**
 * A pointer to a URI object
 */
typedef ofobj_p ofuri_p;

extern "C" {

    /**
     * Get a C-style string representation of the URI.  The result is
     * returned in the provided out pointer.  The resulting memory is
     * owned by the object and must not be freed by the caller.
     * @param uri the URI
     * @param str the pointer that will recieve the output
     * @return a status code
     */
    ofstatus ofuri_get_str(ofuri_p uri, /* out */ const char** str);

    /**
     * Compute a hash value for the URI that can be used with a hash
     * table.
     * @param uri the URI
     * @param hash_value a size_t pointer that will recieve the hash
     * value
     * @return a status code
     */
    ofstatus ofuri_hash(ofuri_p uri, /* out */ size_t* hash_value);

} /* extern "C" */

/** @} curi */
/** @} cmodb */
/** @} cwrapper */

#endif /* OPFLEX_C_OFURI_H */
