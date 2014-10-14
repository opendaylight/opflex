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
    // walk up the tree to the root and queue notifications along the
    // path.
    try {
        Region* r = store->getRegion(class_id);
        const std::pair<URI, prop_id_t>& parent = r->getParent(class_id, uri);
        queueNotification(store->prop_map.at(parent.second)->getId(), 
                          parent.first, notifs);
    } catch (std::out_of_range e) {
        // no parent
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
                      const boost::shared_ptr<const ObjectInstance>& oi) {
    Region* r = checkOwner(store, readOnly, region, class_id);
    r->put(class_id, uri, oi);
}

boost::shared_ptr<const ObjectInstance> StoreClient::get(class_id_t class_id,
                                                         const URI& uri) const {
    Region* r = store->getRegion(class_id);
    return r->get(uri);
}

bool StoreClient::remove(class_id_t class_id, const URI& uri,
                         bool recursive, notif_t* notifs) {
    Region* r = checkOwner(store, readOnly, region, class_id);

    // Remove the object itself
    bool result = r->remove(class_id, uri);
    if (!result) return result;

    // remove the parent link
    try {
        const std::pair<URI, prop_id_t>& parent = r->getParent(class_id, uri);
        std::vector<std::pair<URI, prop_id_t> > parents;
        std::vector<std::pair<URI, prop_id_t> >::iterator pit;
        class_id_t parent_class = store->prop_map.at(parent.second)->getId();
        delChild(parent_class, parent.first, parent.second,
                 class_id, uri);
    } catch (std::out_of_range e) {
        // no parent link found
    }

    if (!recursive) return result;

    // Remove all the children if requested
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
                remove(prop_class, *cit, true);
                if (notifs)
                    (*notifs)[*cit] = prop_class;
            }
        }
    }
    return result;
}

void StoreClient::addChild(class_id_t parent_class,
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
    r->addChild(parent_class, parent_uri, parent_prop, 
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

const std::pair<URI, prop_id_t>& 
StoreClient::getParent(class_id_t child_class,
                       const URI& child) {
    Region* r = store->getRegion(child_class);
    return r->getParent(child_class, child);
}

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */
