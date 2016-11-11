/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for ObjectStore class.
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


#include <boost/test/unit_test.hpp>
#include <vector>
#include <algorithm>
#include <unistd.h>

#include "opflex/modb/internal/ObjectStore.h"
#include "LockGuard.h"
#include "BaseFixture.h"
#include "TestListener.h"

using namespace opflex::modb;
using std::out_of_range;
using std::invalid_argument;
using std::vector;
using mointernal::ObjectInstance;

BOOST_AUTO_TEST_SUITE(ObjectStore_test)

// Just checks that basic metadata initialization works correctly
BOOST_FIXTURE_TEST_CASE( metadata_init, BaseFixture ) {
    // Check metadata
    BOOST_CHECK_EQUAL(10, md.getClasses().size());
    BOOST_CHECK_EQUAL("class1", md.getClasses()[0].getName());
    BOOST_CHECK_EQUAL(md.getClasses()[0].getProperties().size(), 7);
    BOOST_CHECK_EQUAL("prop1", md.getClasses()[0].getProperty("prop1").getName());
    BOOST_CHECK_EQUAL("prop2", md.getClasses()[0].getProperty("prop2").getName());
    BOOST_CHECK_EQUAL(PropertyInfo::COMPOSITE,
                      md.getClasses()[0].getProperties().at(3).getType());

    // Check class map
    BOOST_CHECK_EQUAL("class1", db.getClassInfo(1).getName());
    BOOST_CHECK_EQUAL("class2", db.getClassInfo(2).getName());
    BOOST_CHECK_THROW(db.getClassInfo(0), out_of_range);

    // Check region owner
    BOOST_CHECK(&db.getStoreClient("owner2") !=
                &db.getStoreClient("owner1"));
    BOOST_CHECK_THROW(db.getStoreClient("notanowner"), out_of_range);

    // Check region class_id
    BOOST_CHECK(db.getRegion(1) != NULL);
    BOOST_CHECK(db.getRegion(2) != NULL);
    BOOST_CHECK(db.getRegion(3) != NULL);
    BOOST_CHECK(db.getRegion(2) == db.getRegion(1));
    BOOST_CHECK(db.getRegion(3) != db.getRegion(1));
    BOOST_CHECK_THROW(db.getRegion(0), out_of_range);
}

BOOST_FIXTURE_TEST_CASE( region, BaseFixture ) {
    OF_SHARED_PTR<ObjectInstance> oi =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(1));
    oi->setUInt64(1, 42);
    oi->addString(2, "val1");
    oi->addString(2, "val2");

    URI uri("/");
    client1->put(1, uri, oi);
    BOOST_CHECK_THROW(client2->put(1, uri, oi), invalid_argument);

    OF_SHARED_PTR<const ObjectInstance> oi2 = client1->get(1, uri);
    BOOST_CHECK_EQUAL(42, oi2->getUInt64(1));

    OF_SHARED_PTR<ObjectInstance> oi3 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(*oi2));
    oi3->addString(2, "val3");
    client1->put(1, uri, oi3);

    oi2 = client1->get(1, uri);
    BOOST_CHECK_EQUAL("val3", oi2->getString(2, 2));
    BOOST_CHECK_THROW(oi->getString(2, 2), out_of_range);

    BOOST_CHECK_EQUAL(true, client1->remove(1, uri, true));
    BOOST_CHECK_EQUAL(false, client1->remove(1, uri, true));
    BOOST_CHECK_THROW(client2->remove(87, uri, true), out_of_range);
}

BOOST_FIXTURE_TEST_CASE( tree, BaseFixture ) {
    OF_UNORDERED_MAP<URI, class_id_t> notifs;

    TestListener listener;
    db.registerListener(1, &listener);
    db.registerListener(2, &listener);

    OF_SHARED_PTR<ObjectInstance> oi1 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(1));
    OF_SHARED_PTR<ObjectInstance> oi2 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(2));
    OF_SHARED_PTR<ObjectInstance> oi3 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(3));
    OF_SHARED_PTR<ObjectInstance> oi4 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(2));

    URI uri1("/");
    URI uri2("/prop3/42");
    URI uri3("/prop3/42/prop5/4242");
    URI uri4("/prop3/43");

    client1->put(1, uri1, oi1);
    client1->put(2, uri2, oi2);
    client2->put(3, uri3, oi3);
    client1->put(2, uri4, oi4);
    client1->queueNotification(1, uri1, notifs);
    client1->queueNotification(2, uri2, notifs);
    client1->queueNotification(3, uri3, notifs);
    client1->queueNotification(2, uri4, notifs);

    client1->addChild(1, uri1, 3, 2, uri2);
    client1->addChild(1, uri1, 3, 2, uri4);
    client2->addChild(2, uri2, 5, 3, uri3);
    client1->deliverNotifications(notifs);

    // we have registered for class 1 and 2, should have gotten
    // notified for 1,2,4
    WAIT_FOR(listener.contains(uri1), 500);
    WAIT_FOR(listener.contains(uri2), 500);
    WAIT_FOR(!listener.contains(uri3), 500);
    WAIT_FOR(listener.contains(uri4), 500);
    listener.notifs.clear();
    notifs.clear();

    client2->put(3, uri3, oi3);
    client2->queueNotification(3, uri3, notifs);
    client1->deliverNotifications(notifs);
    // we're not registered for uri3 but we should get notified for
    // its parents 1 and 2.  4 is not affected
    WAIT_FOR(listener.contains(uri1), 500);
    WAIT_FOR(listener.contains(uri2), 500);
    WAIT_FOR(!listener.contains(uri3), 500);
    WAIT_FOR(!listener.contains(uri4), 500);
    listener.notifs.clear();
    notifs.clear();

    // parent URI needs to be a prefix of child
    URI uri_bad("/prop3/43/prop5/4242");
    BOOST_CHECK_THROW(client1->addChild(2, uri2, 5, 3, uri_bad), invalid_argument);

    vector<URI> output;
    client1->getChildren(1, uri1, 3, 2, output);
    BOOST_CHECK_EQUAL(2, output.size());
    BOOST_CHECK(find(output.begin(), output.end(), uri2) != output.end());
    BOOST_CHECK(find(output.begin(), output.end(), uri4) != output.end());
    output.clear();

    client1->getChildren(2, uri2, 5, 3, output);
    BOOST_CHECK_EQUAL(1, output.size());
    BOOST_CHECK_EQUAL(uri3.toString(), output.at(0).toString());
    output.clear();

    client2->remove(3, uri3, true);
    client1->remove(1, uri1, true);

    // should remove the whole subtree
    BOOST_CHECK_THROW(client1->get(1, uri1), out_of_range);
    BOOST_CHECK_THROW(client1->get(2, uri2), out_of_range);
    BOOST_CHECK_THROW(client1->get(3, uri3), out_of_range);

    // also check that the links are removed
    client1->getChildren(1, uri1, 3, 2, output);
    BOOST_CHECK_EQUAL(0, output.size());
    output.clear();
    client1->getChildren(2, uri2, 5, 3, output);
    BOOST_CHECK_EQUAL(0, output.size());
    output.clear();
}

BOOST_AUTO_TEST_SUITE_END()
