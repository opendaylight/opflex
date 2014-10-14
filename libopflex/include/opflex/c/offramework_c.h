/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file offramework_c.h
 * @brief C wrapper for OFFramework
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ofcore_c.h"

#ifndef OPFLEX_C_OFFRAMEWORK_H
#define OPFLEX_C_OFFRAMEWORK_H

/**
 * @addtogroup cwrapper 
 * @{
 */

/**
 * @defgroup cmetadata Model Metadata
 * Wrapper for generated model metadata.
 *
 * These types are used to define the basic parameters of the model.
 * An actual instance of the metadata would be defined in the
 * generated model code.
 * @{
 */

/**
 * A pointer to a generated model metadata object.  This object is
 * entirely opaque and appears in the wrapper interface as just a
 * pointer.
 */
typedef ofobj_p ofmetadata_p;

/* @} cmetadata */

/**
 * @addtogroup ccore
 * @{
 * @defgroup cofframework Opflex Framework Object
 * Main interface to the OpFlex framework.
 * @{
 */

/**
 * A pointer to an OF framework object
 */
typedef ofobj_p offramework_p;

extern "C" {

    /**
     * Create a new OF framework instance.  You must eventually call
     * @ref offramework_destroy() on the returned object.
     * 
     * @param framework a pointer to memory that will receive the
     * pointer to the newly-allocated object.
     * @return a status code
     */
    ofstatus offramework_create(/* out */ offramework_p* framework);

    /**
     * Destroy a OF Framework instance, and zero the pointer.
     *
     * @param framework a pointer to memory containing the OF
     * framework pointer.
     * @return a status code
     */
    ofstatus offramework_destroy(/* out */ offramework_p* framework);

    /**
     * Add the given model metadata to the managed object database.  Must
     * be called before @ref offramework_start().
     *
     * @param framework the framework
     * @param metadata the model metadata to set
     * @return a status code
     */
    ofstatus offramework_set_model(offramework_p framework,
                                   ofmetadata_p metadata);

    /**
     * Start the framework.  This will start all the framework threads
     * and attempt to connect to configured OpFlex peers.
     *
     * @param framework the framework
     * @return a status code
     */
    ofstatus offramework_start(offramework_p framework);

    /**
     * Cleanly stop the framework.
     *
     * @param framework the framework
     * @return a status code
     */
    ofstatus offramework_stop(offramework_p framework);

} /* extern "C" */

/** @} offramework */
/** @} ccore */
/** @} cwrapper */

#endif /* OPFLEX_C_OFFRAMEWORK_H */
