/*
 * Mock implementation of MODB APIs.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/unordered_map.hpp>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <list>
#include <internal/modb.h>

#include <iostream>

using namespace std;

namespace opflex {
namespace modb {


namespace mock {
static boost::unordered_map<class_id_t, std::list<ObjectListener*> > listeners;
typedef boost::unordered_map<URI, MoBasePtr> ObjectMap;
static ObjectMap objects;
}   // mock


void RegisterListener(class_id_t c, ObjectListener& ol) {
    mock::listeners[c].push_back(&ol);
}

void UnregisterListener(class_id_t c, ObjectListener& ol) {

}

MoBasePtr Find(const URI& uri) {
    mock::ObjectMap::iterator itr = mock::objects.find(uri);
    return (itr == mock::objects.end()) ? MoBasePtr() : itr->second;
}

bool Add(MoBasePtr newObj) {
    const URI& uri = newObj->GetUri();
    mock::ObjectMap::iterator itr = mock::objects.find(uri);
    mock::objects[uri] = newObj;
    return itr == mock::objects.end();
}

bool Remove(const URI& uri) {
    return mock::objects.erase(uri) != 0;
}

}   // namespace modb
}   // namespace opflex




