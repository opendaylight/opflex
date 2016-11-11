/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for StoreClient class.
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

namespace opflex {
namespace modb {
namespace mointernal {

StoreClient::StoreClient(ObjectStore* store_, Region* region_, bool readOnly_)
    : store(store_), region(region_), readOnly(readOnly_) {

}

const std::string& StoreClient::getOwner() const {
    return region->getOwner();
}

static Region* checkOwner(ObjectStore* store, bool readOnly,
                          Region* region, class_id_t class_id) {
    if (readOnly)
        throw std::invalid_argument("Cannot write to a read-only store");
    if (region == NULL)
        return store->getRegion(class_id);
    if (store->getRegion(class_id) != region)
        throw std::invalid_argument("Invalid class ID for this owner");
    return region;
}

void StoreClient::queueNotification(class_id_t class_id, const URI& uri,
                                    notif_t& notifs) {
    if (notifs.find(uri) != notifs.end())
        return;
    // walk up the tree to the root and queue notifications along the
    // path.
    try {
        Region* r = store->getRegion(class_id);
        std::pair<URI, prop_id_t> parent(URI::ROOT, 0);
        if (r->getParent(class_id, uri, parent)) {
            queueNotification(store->prop_map.at(parent.second)->getId(),
                              parent.first, notifs);
        }
    } catch (const std::out_of_range&) {
        // region not found
    }
    notifs[uri] = class_id;
}

void StoreClient::deliverNotifications(const notif_t& notifs) {
    notif_t::const_iterator nit;
    for (nit = notifs.begin(); nit != notifs.end(); ++nit) {
        store->queueNotification(nit->second, nit->first);
    }
}

void StoreClient::put(class_id_t class_id,
                      const URI& uri,
                      const OF_SHARED_PTR<const ObjectInstance>& oi) {
    Region* r = checkOwner(store, readOnly, region, class_id);
    r->put(class_id, uri, oi);
}

bool StoreClient::putIfModified(class_id_t class_id,
                                const URI& uri,
                                const OF_SHARED_PTR<const ObjectInstance>& oi) {
    Region* r = checkOwner(store, readOnly, region, class_id);
    return r->putIfModified(class_id, uri, oi);
}

bool StoreClient::isPresent(class_id_t class_id, const URI& uri) const {
    Region* r = store->getRegion(class_id);
    return r->isPresent(uri);
}

OF_SHARED_PTR<const ObjectInstance> StoreClient::get(class_id_t class_id,
                                                     const URI& uri) const {
    Region* r = store->getRegion(class_id);
    return r->get(uri);
}

bool StoreClient::get(class_id_t class_id, const URI& uri,
                      /*out*/ OF_SHARED_PTR<const ObjectInstance>& oi) const {
    Region *r;
    try {
        r = store->getRegion(class_id);
    } catch (const std::out_of_range&) {
        return false;
    }
    return r->get(uri, oi);
}

void StoreClient::removeChildren(class_id_t class_id, const URI& uri,
                                 notif_t* notifs) {
    const ClassInfo& ci = store->getClassInfo(class_id);
    const ClassInfo::property_map_t& pmap = ci.getProperties();
    ClassInfo::property_map_t::const_iterator it;
    for (it = pmap.begin(); it != pmap.end(); ++it) {
        if (it->second.getType() == PropertyInfo::COMPOSITE) {
            class_id_t prop_class = it->second.getClassId();
            prop_id_t prop_id = it->second.getId();
            std::vector<URI> children;
            getChildren(class_id, uri, prop_id, prop_class, children);
            std::vector<URI>::iterator cit;
            for (cit = children.begin(); cit != children.end(); ++cit) {
                // unlink the parent/child
                delChild(class_id, uri, prop_id, prop_class, *cit);
                // remove the child object
                remove(prop_class, *cit, true, notifs);
                if (notifs)
                    (*notifs)[*cit] = prop_class;
            }
        }
    }
}

bool StoreClient::remove(class_id_t class_id, const URI& uri,
                         bool recursive, notif_t* notifs) {
    Region* r = checkOwner(store, readOnly, region, class_id);

    // Remove the object itself
    bool result = r->remove(class_id, uri);
    if (!result) return result;

    // remove the parent link
    try {
        std::pair<URI, prop_id_t> parent(URI::ROOT, 0);
        if (r->getParent(class_id, uri, parent)) {
            class_id_t parent_class = store->prop_map.at(parent.second)->getId();
            delChild(parent_class, parent.first, parent.second,
                     class_id, uri);
        }
    } catch (const std::out_of_range&) {
        // parent prop info not found
    }

    if (!recursive) return result;

    // Remove all the children if requested
    removeChildren(class_id, uri, notifs);

    return result;
}

bool StoreClient::addChild(class_id_t parent_class,
                           const URI& parent_uri,
                           prop_id_t parent_prop,
                           class_id_t child_class,
                           const URI& child_uri) {
    // verify that parent URI and class exists
    store->getRegion(parent_class)->get(parent_uri);

    // verify that the parent property exists for this class
    if (store->prop_map.at(parent_prop)->getId() != parent_class)
        throw std::invalid_argument("Parent class does not contain property");

    // verify that the parent URI is a prefix of child URI
    const std::string& puri = parent_uri.toString();
    const std::string& curi = child_uri.toString();
    if (puri.length() >= curi.length() ||
        0 != curi.compare(0, puri.length(), puri))
        throw std::invalid_argument("Parent URI must be a prefix of child URI");

    // add relationship to child's region.  Note that
    // it's OK if the child URI doesn't exist
    Region* r = checkOwner(store, readOnly, region, child_class);
    return r->addChild(parent_class, parent_uri, parent_prop,
                       child_class, child_uri);
}

void StoreClient::delChild(class_id_t parent_class,
                           const URI& parent_uri,
                           prop_id_t parent_prop,
                           class_id_t child_class,
                           const URI& child_uri) {
    Region* r = checkOwner(store, readOnly, region, child_class);
    r->delChild(parent_class, parent_uri, parent_prop,
                child_class, child_uri);
}

void StoreClient::getChildren(class_id_t parent_class,
                              const URI& parent_uri,
                              prop_id_t parent_prop,
                              class_id_t child_class,
                              std::vector<URI>& output) {
    Region* r = store->getRegion(child_class);
    r->getChildren(parent_class, parent_uri, parent_prop,
                   child_class, output);
}

std::pair<URI, prop_id_t>
StoreClient::getParent(class_id_t child_class,
                       const URI& child) {
    Region* r = store->getRegion(child_class);
    return r->getParent(child_class, child);
}

bool StoreClient::getParent(class_id_t child_class, const URI& child,
                            /* out */ std::pair<URI, prop_id_t>& parent) {
    Region *r;
    try {
        r = store->getRegion(child_class);
    } catch(const std::out_of_range&) {
        return false;
    }
    return r->getParent(child_class, child, parent);
}

void StoreClient::getObjectsForClass(class_id_t class_id,
                                     /* out */ OF_UNORDERED_SET<URI>& output) {
    Region* r = store->getRegion(class_id);
    return r->getObjectsForClass(class_id, output);
}

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */
