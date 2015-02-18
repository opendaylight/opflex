/* -*- C -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ofobjectlistener_c.h
 * @brief C wrapper for object listener
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ofcore_c.h"
#include "ofuri_c.h"

#pragma once
#ifndef OPFLEX_C_OFOBJECTLISTENER_H
#define OPFLEX_C_OFOBJECTLISTENER_H

/**
 * @addtogroup cwrapper
 * @{
 * @addtogroup cmodb
 * @{
 */

/**
 * @defgroup cofobjectlistener Object Listener
 * An object that monitors for updates to objects in the data
 * store. Object listeners are registered for a particular class will
 * be triggered for any modifications for objects of that class or any
 * transitive children of objects of that class.
 * @{
 */

/**
 * A pointer to an object listener object.  
 */
typedef ofobj_p ofobjectlistener_p;

/**
 * A function pointer to a function to receive notificiations.
 * @param user_data a pointer to an opaque user data structure
 * @param class_id the class ID of the affected class
 * @param uri the affected URI
 */
typedef void (*ofnotify_p)(void* user_data, class_id_t class_id, ofuri_p uri);

extern "C" {
    /**
     * Create a new object listener.  You must eventually call
     * @ref ofobjectlistener_destroy() on the returned object.
     * 
     * @param user_data an opaque data blob that will be passed to
     * your handler
     * @param callback a function pointer to your handler function to be
     * invoked when the listener is notified.
     * @param listener a pointer to memory that will receive the
     * pointer to the newly-allocated object.
     * @return a status code
     */
    ofstatus ofobjectlistener_create(void* user_data,
                                     ofnotify_p callback,
                                     /* out */ ofobjectlistener_p* listener);

    /**
     * Destroy an object listener, and zero the pointer.  You must
     * ensure that the listener has been unregistered from all
     * listeners before destroying it.
     *
     * @param listener a pointer to memory containing the object pointer.
     * @return a status code
     */
    ofstatus ofobjectlistener_destroy(/* out */ ofobjectlistener_p* listener);

} /* extern "C" */

/** @} cofobjectlistener */
/** @} cmodb */
/** @} cwrapper */

#endif /* OPFLEX_C_OFOBJECTLISTENER_H */
