/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for Inspector.
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

#include <cstdio>
#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <sys/stat.h>

#include "BaseFixture.h"
#include "TestListener.h"

#include "opflex/engine/Inspector.h"
#include "opflex/engine/InspectorClientImpl.h"

namespace opflex {
namespace ofcore {

using std::string;
using boost::scoped_ptr;
using modb::BaseFixture;
using modb::URI;
using modb::mointernal::ObjectInstance;
using modb::mointernal::StoreClient;
using engine::Inspector;
using engine::InspectorClientImpl;

BOOST_AUTO_TEST_SUITE(Inspector_test)

static const string SOCK_NAME("/tmp/inspector_test.sock");

class InspectorFixture : public BaseFixture {
public:
    InspectorFixture()
        : BaseFixture(), inspector(&db), client(SOCK_NAME, md) {
        inspector.setSocketName(SOCK_NAME);
        inspector.start();
    }

    ~InspectorFixture() {
        inspector.stop();
        std::remove(SOCK_NAME.c_str());
    }

    Inspector inspector;
    InspectorClientImpl client;
};

static bool itemPresent(StoreClient* client,
                        modb::class_id_t class_id, const URI& uri) {
    try {
        client->get(class_id, uri);
        return true;
    } catch (std::out_of_range e) {
        return false;
    }
}

BOOST_FIXTURE_TEST_CASE( query, InspectorFixture ) {
    URI c4u("/class4/test/");
    URI c5u("/class5/test/");
    URI c5u_2("/class5/test2/");
    URI c6u("/class4/test/class6/test2/");
    OF_SHARED_PTR<ObjectInstance> oi5(new ObjectInstance(5));
    oi5->setString(10, "test");
    oi5->addReference(11, 4, c4u);
    OF_SHARED_PTR<ObjectInstance> oi5_2(new ObjectInstance(5));
    oi5_2->setString(10, "test2");
    oi5_2->addReference(11, 4, c4u);

    OF_SHARED_PTR<ObjectInstance> oi4(new ObjectInstance(4));
    oi4->setString(9, "test");
    OF_SHARED_PTR<ObjectInstance> oi6(new ObjectInstance(6));
    oi6->setString(13, "test2");

    client2->put(5, c5u, oi5);
    client2->put(5, c5u_2, oi5_2);
    client2->put(4, c4u, oi4);
    client2->put(6, c6u, oi6);
    client2->addChild(4, c4u, 12, 6, c6u);

    struct stat buffer;
    WAIT_FOR(stat(SOCK_NAME.c_str(), &buffer) == 0, 500);

    client.setRecursive(true);
    client.addQuery("class4", URI("/class4/test/"));
    client.addClassQuery("class5");
    client.execute();

    StoreClient& rosClient = client.getStore().getReadOnlyStoreClient();
    WAIT_FOR(itemPresent(&rosClient, 4, c4u), 1000);
    WAIT_FOR(itemPresent(&rosClient, 5, c5u), 1000);
    WAIT_FOR(itemPresent(&rosClient, 5, c5u_2), 1000);
    WAIT_FOR(itemPresent(&rosClient, 6, c6u), 1000);
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace ofcore */
} /* namespace opflex */
