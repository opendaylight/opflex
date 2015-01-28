/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ObjectListener class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include "opflex/modb/internal/ObjectStore.h"

#include "opflex/engine/internal/AbstractObjectListener.h"

namespace opflex {
namespace engine {
namespace internal {

using modb::ObjectStore;
using modb::class_id_t;

AbstractObjectListener::AbstractObjectListener(ObjectStore* store_) 
    : store(store_) {

}

AbstractObjectListener::~AbstractObjectListener() {
    unlisten();
}

void AbstractObjectListener::listen(class_id_t class_id) {
    listen_classes.insert(class_id);
    store->registerListener(class_id, this);
}

void AbstractObjectListener::unlisten() {
    std::set<class_id_t>::const_iterator it;
    for (it = listen_classes.begin();
         it != listen_classes.end();
         ++it) {
        store->unregisterListener(*it, this);
    }
    listen_classes.clear();
}

void AbstractObjectListener::unlisten(class_id_t class_id) {
    store->unregisterListener(class_id, this);
    listen_classes.erase(class_id);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
