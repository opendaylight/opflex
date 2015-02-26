/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ofmutator_c.h
 * @brief C wrapper for Mutator
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "offramework_c.h"

#ifndef OPFLEX_C_OFMUTATOR_H
#define OPFLEX_C_OFMUTATOR_H

/**
 * @addtogroup cwrapper
 * @{
 * @addtogroup cmodb
 * @{
 */

/**
 * @defgroup cmutator Mutator
 * A mutator represents a set of changes to apply to the data store.
 * When modifying data through the managed object accessor classes,
 * changes are not actually written into the store, but rather are
 * written into the Mutator registered using the framework in a
 * thread-local variable. These changes will not become visible until
 * the mutator is committed by calling @ref ofmutator_commit()
 *
 * The changes are applied to the data store all at once but not
 * necessarily guaranteed to be atomic. That is, a read in another
 * thread could see some objects updated but not others. Each
 * individual object update is applied atomically, however, so the
 * fields within that object will either all be modified or none.
 * 
 * This is most similar to a database transaction with an isolation
 * level set to read uncommitted.
 * @{
 */

/**
 * A pointer to a mutator object
 */
typedef ofobj_p ofmutator_p;

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Create a new mutator that will work with the provided framework
     * instance.
     *
     * @param framework the framework instance
     * @param owner the owner string that will control which fields can be
     * modified.
     * @param mutator a pointer to memory that will receive the
     * pointer to the newly-allocated object.
     * @return a status code
     */
    ofstatus ofmutator_create(offramework_p framework, 
                              const char* owner,
                              /* out */ ofmutator_p* mutator);

    /**
     * Destroy a mutator, and zero the pointer.  Any uncommitted
     * changes will be lost.
     *
     * @param mutator a pointer to memory containing the OF framework
     * pointer.
     * @return a status code
     */
    ofstatus ofmutator_destroy(/* out */ ofmutator_p* mutator);

    /**
     * Commit the changes stored in the mutator.  If this function is
     * not called, these changes will simply be lost.
     * 
     * @param mutator the mutator
     * @return a status code
     */
    ofstatus ofmutator_commit(ofmutator_p mutator);
    

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} mutator */
/** @} cmodb */
/** @} cwrapper */

#endif /* OPFLEX_C_OFMUTATOR_H */
