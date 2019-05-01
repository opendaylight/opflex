/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MO class.
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

#include <stdexcept>

#include "opflex/modb/mo-internal/MO.h"
#include "opflex/modb/internal/ObjectStore.h"

namespace opflex {
namespace modb {
namespace mointernal {

using ofcore::OFFramework;

class MO::MOImpl {
public:
    MOImpl(OFFramework& framework_,
           class_id_t class_id_,
           const URI& uri_,
           const OF_SHARED_PTR<const ObjectInstance>& oi_)
        : framework(framework_),
          class_id(class_id_), uri(uri_), oi(oi_){ }

    ~MOImpl() { }

    /**
     * The opflex framework object associated with this MO
     */
    OFFramework& framework;

    /**
     * The unique class ID for this object.
     *
     * @see MO::getClassId()
     */
    modb::class_id_t class_id;

    /**
     * URI for this managed object
     */
    modb::URI uri;

    /**
     * Object instance for this managed object
     */
    OF_SHARED_PTR<const modb::mointernal::ObjectInstance> oi;

    friend bool operator==(const MO& lhs, const MO& rhs);
};

MO::MO(OFFramework& framework,
       class_id_t class_id,
       const URI& uri,
       const OF_SHARED_PTR<const ObjectInstance>& oi)
    : pimpl(new MOImpl(framework, class_id, uri, oi)) {

}

MO::~MO() {
    delete pimpl;
}

modb::class_id_t MO::getClassId() const {
    return pimpl->class_id;
}

const modb::URI& MO::getURI() const {
    return pimpl->uri;
}

const ObjectInstance& MO::getObjectInstance() const {
    return *pimpl->oi;
}

OFFramework& MO::getFramework() const {
    return pimpl->framework;
}

StoreClient& MO::getStoreClient(OFFramework& framework) {
    return framework.getStore().getReadOnlyStoreClient();
}

OF_SHARED_PTR<const ObjectInstance> MO::resolveOI(OFFramework& framework,
                                                  class_id_t class_id,
                                                  const URI& uri) {
    return MO::getStoreClient(framework).get(class_id, uri);
}

modb::Mutator& MO::getTLMutator() {
    return getFramework().getTLMutator();
}

void MO::registerListener(ofcore::OFFramework& framework,
                          ObjectListener* listener,
                          class_id_t class_id) {
    framework.getStore().registerListener(class_id,
                                          listener);
}

void MO::unregisterListener(ofcore::OFFramework& framework,
                            ObjectListener* listener,
                            class_id_t class_id) {
    framework.getStore().unregisterListener(class_id,
                                            listener);
}

bool operator==(const MO& lhs, const MO& rhs) {
    if (lhs.pimpl->uri != rhs.pimpl->uri) return false;
    if (lhs.pimpl->class_id != rhs.pimpl->class_id) return false;
    if (*lhs.pimpl->oi != *rhs.pimpl->oi) return false;
    return true;
}

bool operator!=(const MO& lhs, const MO& rhs) {
    return !operator==(lhs,rhs);
}

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */
