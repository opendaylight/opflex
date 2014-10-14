/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ObjectListener.h
 * @brief Interface definition file for ObjectListeners
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_OBJECTLISTENER_H
#define MODB_OBJECTLISTENER_H

#include <set>
#include "ClassInfo.h"
#include "URI.h"

namespace opflex {
namespace modb {

class ObjectStore;

/**
 * \addtogroup cpp
 * @{
 * \addtogroup modb
 * @{
 */

/**
 * @brief Interface for an object interested in updates to objects in
 * the data store.
 *
 * Object listeners are registered for a particular class will be
 * triggered for any modifications for objects of that class or any
 * transitive children of objects of that class.
 */
class ObjectListener {
public:
    /**
     * Destroy the object listener
     */
    virtual ~ObjectListener() {}

    /**
     * The specified URI has been added, updated, or deleted.  The
     * listener should queue a task to read the new state and perform
     * appropriate processing.  If this function blocks or peforms a
     * long-running operation, then the dispatching of update
     * notifications will be stalled, but there will not be any other
     * deleterious effects.
     *
     * If multiple changes happen to the same URI, then at least one
     * notification will be delivered but some events may be
     * consolidated.
     *
     * @param class_id the class ID for the type associated with the
     * updated object.
     * @param uri the URI for the updated object
     */
    virtual void objectUpdated(class_id_t class_id, const URI& uri) = 0;
};

/* @} modb */
/* @} cpp */

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_OBJECTLISTENER_H */
