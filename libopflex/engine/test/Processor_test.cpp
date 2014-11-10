/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for Processor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <unistd.h>

#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/shared_ptr.hpp>
#include <rapidjson/stringbuffer.h>

#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/engine/internal/MOSerializer.h"
#include "opflex/engine/Processor.h"
#include "opflex/logging/StdOutLogHandler.h"

#include "BaseFixture.h"
#include "TestListener.h"
#include "MockOpflexServer.h"

using namespace opflex::engine;
using namespace opflex::engine::internal;
using namespace opflex::modb;
using namespace opflex::modb::mointernal;
using namespace opflex::logging;
using namespace rapidjson;

using boost::assign::list_of;
using boost::shared_ptr;
using mointernal::ObjectInstance;
using std::out_of_range;
using std::make_pair;

#define SERVER_ROLES \
        (OpflexHandler::POLICY_REPOSITORY |     \
         OpflexHandler::ENDPOINT_REGISTRY |     \
         OpflexHandler::OBSERVER)

class Fixture : public BaseFixture {
public:
    Fixture() : processor(&db) {
        processor.setDelay(50);
        processor.setOpflexIdentity("testelement", "testdomain");
        processor.start();
    }

    ~Fixture() {
        processor.stop();
    }

    Processor processor;
};

class ServerFixture : public Fixture {
public:
    ServerFixture()
        : mockServer(8009, SERVER_ROLES,
                     list_of(make_pair(SERVER_ROLES, "127.0.0.1:8009")),
                     md) {
        processor.addPeer("127.0.0.1", 8009);

    }

    ~ServerFixture() {

    }

    MockOpflexServer mockServer;
};

BOOST_AUTO_TEST_SUITE(Processor_test)

static bool itemPresent(StoreClient* client,
                        class_id_t class_id, const URI& uri) {
    try {
        client->get(class_id, uri);
        return true;
    } catch (std::out_of_range e) {
        return false;
    }
}

// Test garbage collection after removing references
BOOST_FIXTURE_TEST_CASE( dereference, Fixture ) {
    StoreClient::notif_t notifs;
    URI c4u("/class4/test");
    URI c5u("/class5/test");
    URI c6u("/class4/test/class6/test2");
    shared_ptr<ObjectInstance> oi5 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(5));
    oi5->setString(10, "test");
    oi5->addReference(11, 4, c4u);

    shared_ptr<ObjectInstance> oi4 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(4));
    oi4->setString(9, "test");
    shared_ptr<ObjectInstance> oi6 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(6));
    oi6->setString(12, "test2");

    // put in both in one operation, so the metadata object will be
    // already present
    client2->put(5, c5u, oi5);
    client2->put(4, c4u, oi4);
    client2->put(6, c6u, oi6);
    client2->addChild(4, c4u, 12, 6, c6u);

    client2->queueNotification(5, c5u, notifs);
    client2->queueNotification(4, c4u, notifs);
    client2->queueNotification(6, c6u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    BOOST_CHECK(itemPresent(client2, 4, c4u));
    BOOST_CHECK(itemPresent(client2, 5, c5u));
    BOOST_CHECK(itemPresent(client2, 6, c6u));
    WAIT_FOR(processor.getRefCount(c4u) > 0, 1000);
    BOOST_CHECK(itemPresent(client2, 6, c6u));

    client2->remove(5, c5u, false, &notifs);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    BOOST_CHECK(!itemPresent(client2, 5, c5u));
    WAIT_FOR(!itemPresent(client2, 4, c4u), 1000);
    BOOST_CHECK_EQUAL(0, processor.getRefCount(c4u));
    WAIT_FOR(!itemPresent(client2, 6, c6u), 1000);

    // add the reference and then the referent so the metadata object
    // is not present
    client2->put(5, c5u, oi5);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    BOOST_CHECK(itemPresent(client2, 5, c5u));
    WAIT_FOR(processor.getRefCount(c4u) > 0, 1000);

    client2->put(4, c4u, oi4);
    client2->put(6, c6u, oi6);
    client2->addChild(4, c4u, 12, 6, c6u);
    client2->queueNotification(4, c4u, notifs);
    client2->queueNotification(6, c6u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();
 
    BOOST_CHECK(itemPresent(client2, 4, c4u));
    BOOST_CHECK(itemPresent(client2, 6, c6u));

    client2->remove(5, c5u, false, &notifs);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    BOOST_CHECK(!itemPresent(client2, 5, c5u));
    WAIT_FOR(!itemPresent(client2, 4, c4u), 1000);
    BOOST_CHECK_EQUAL(0, processor.getRefCount(c4u));
    WAIT_FOR(!itemPresent(client2, 6, c6u), 1000);
}

// test bootstrapping of Opflex connection
BOOST_FIXTURE_TEST_CASE( bootstrap, Fixture ) {
    MockOpflexServer::peer_t p1 =
        make_pair(SERVER_ROLES, "127.0.0.1:8009");
    MockOpflexServer::peer_t p2 =
        make_pair(SERVER_ROLES, "127.0.0.1:8010");

    MockOpflexServer anycastServer(8011, 0, list_of(p1)(p2), md);
    MockOpflexServer peer1(8009, SERVER_ROLES, list_of(p1)(p2), md);
    MockOpflexServer peer2(8010, SERVER_ROLES, list_of(p1)(p2), md);

    processor.addPeer("127.0.0.1", 8011);

    // client should connect to anycast server, get a list of peers
    // and connect to those, then disconnect from the anycast server

    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8009) != NULL, 1000);
    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8010) != NULL, 1000);
    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8009)->isReady(), 1000);
    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8010)->isReady(), 1000);
    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8011) == NULL, 1000);
}

// test endpoint declarations and undeclarations
BOOST_FIXTURE_TEST_CASE( endpoint_declare, ServerFixture ) {
    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8009) != NULL, 1000);
    WAIT_FOR(processor.getPool().getPeer("127.0.0.1", 8009)->isReady(), 1000);

    StoreClient::notif_t notifs;

    URI u1("/");
    shared_ptr<ObjectInstance> oi1 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(1));
    client1->put(1, u1, oi1);

    URI u2_1("/class2/42");
    shared_ptr<ObjectInstance> oi2_1 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(2));
    oi2_1->setInt64(4, 42);
    client1->put(2, u2_1, oi2_1);

    URI u2_2("/class2/43");
    shared_ptr<ObjectInstance> oi2_2 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(3));
    oi2_2->setInt64(4, 42);
    client1->put(2, u2_2, oi2_1);

    client1->queueNotification(1, u1, notifs);
    client1->queueNotification(2, u2_1, notifs);
    client1->queueNotification(2, u2_2, notifs);
    client1->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(itemPresent(mockServer.getSystemClient(), 2, u2_1), 1000);
    WAIT_FOR(itemPresent(mockServer.getSystemClient(), 2, u2_2), 1000);

}

BOOST_AUTO_TEST_SUITE_END()
