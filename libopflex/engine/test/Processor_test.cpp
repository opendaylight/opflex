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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <vector>
#include <unistd.h>

#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/unordered_map.hpp>
#include <rapidjson/stringbuffer.h>

#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/modb/MAC.h"
#include "opflex/engine/internal/MOSerializer.h"
#include "opflex/engine/Processor.h"
#include "opflex/logging/StdOutLogHandler.h"
#include "opflex/engine/internal/MockOpflexServerImpl.h"

#include "BaseFixture.h"
#include "TestListener.h"

using namespace opflex::engine;
using namespace opflex::engine::internal;
using namespace opflex::modb;
using namespace opflex::modb::mointernal;
using namespace opflex::logging;
using namespace rapidjson;

using boost::assign::list_of;
using boost::shared_ptr;
using boost::make_shared;
using boost::unordered_map;
using mointernal::ObjectInstance;
using std::out_of_range;
using std::make_pair;
using std::vector;
using opflex::ofcore::OFConstants;
using opflex::ofcore::PeerStatusListener;
using opflex::test::MockOpflexServer;

#define SERVER_ROLES \
        (OFConstants::POLICY_REPOSITORY |     \
         OFConstants::ENDPOINT_REGISTRY |     \
         OFConstants::OBSERVER)
#define LOCALHOST "127.0.0.1"

BOOST_AUTO_TEST_SUITE(Processor_test)

class TestPeerStatusListener : public PeerStatusListener {
public:
    void peerStatusUpdated(const std::string& peerHostname,
                           int peerPort,
                           PeerStatus peerStatus) {
        statusMap[peerPort] = peerStatus;
    }

    void healthUpdated(Health health) {
        latestHealth = health;
    }
    
    unordered_map<int, PeerStatus> statusMap;
    Health latestHealth;
};

class BasePFixture : public BaseFixture {
public:
    BasePFixture() : processor(&db) {
        processor.setDelay(5);
        processor.setOpflexIdentity("testelement", "testdomain");
        processor.registerPeerStatusListener(&peerStatus);
    }

    ~BasePFixture() {
        processor.stop();
    }

    void testBootstrap(bool ssl);

    Processor processor;
    TestPeerStatusListener peerStatus;
};

class Fixture : public BasePFixture {
public:
    Fixture() {
        processor.start();
    }
};

class SSLFixture : public BasePFixture {
public:
    SSLFixture() {
        processor.enableSSL(SRCDIR"/comms/test/ca.pem", true);
        processor.start();
    }
};

class ServerFixture : public Fixture {
public:
    ServerFixture()
        : mockServer(8009, SERVER_ROLES,
                     list_of(make_pair(SERVER_ROLES, LOCALHOST":8009")),
                     md) {
        mockServer.start();
        WAIT_FOR(mockServer.getListener().isListening(), 1000);
    }

    ~ServerFixture() {
        mockServer.stop();
    }

    void startClient() {
        processor.addPeer(LOCALHOST, 8009);
    }

    MockOpflexServerImpl mockServer;
};

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
    URI c4u("/class4/test/");
    URI c5u("/class5/test/");
    URI c6u("/class4/test/class6/test2/");
    shared_ptr<ObjectInstance> oi5 = make_shared<ObjectInstance>(5);
    oi5->setString(10, "test");
    oi5->addReference(11, 4, c4u);

    shared_ptr<ObjectInstance> oi4 = make_shared<ObjectInstance>(4);
    oi4->setString(9, "test");
    shared_ptr<ObjectInstance> oi6 = make_shared<ObjectInstance>(6);
    oi6->setString(13, "test2");

    client2->put(5, c5u, oi5);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    BOOST_CHECK(itemPresent(client2, 5, c5u));
    WAIT_FOR(processor.getRefCount(c4u) > 0, 1000);

    client2->put(4, c4u, oi4);
    client2->put(6, c6u, oi6);
    client2->addChild(4, c4u, 12, 6, c6u);

    processor.remoteObjectUpdated(4, c4u);
    processor.remoteObjectUpdated(6, c6u);

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

static bool connReady(OpflexPool& pool, const char* host, int port) {
    OpflexConnection* conn = pool.getPeer(host, port);
    return (conn != NULL && conn->isReady());
}

static void initServerSSL(MockOpflexServerImpl& server) {
    server.enableSSL(SRCDIR"/comms/test/ca.pem",
                     SRCDIR"/comms/test/server.pem",
                     "password123", true);
}

// test bootstrapping of Opflex connection
void BasePFixture::testBootstrap(bool ssl) {
    MockOpflexServer::peer_t p1 =
        make_pair(SERVER_ROLES, "127.0.0.1:8009");
    MockOpflexServer::peer_t p2 =
        make_pair(SERVER_ROLES, "127.0.0.1:8010");

    MockOpflexServerImpl anycastServer(8011, 0, list_of(p1)(p2), md);
    MockOpflexServerImpl peer1(8009, SERVER_ROLES, list_of(p1)(p2), md);
    MockOpflexServerImpl peer2(8010, SERVER_ROLES, list_of(p1)(p2), md);

    if (ssl) {
        initServerSSL(anycastServer);
        initServerSSL(peer1);
        initServerSSL(peer2);
    }

    anycastServer.start();
    peer1.start();
    peer2.start();
    WAIT_FOR(anycastServer.getListener().isListening(), 1000);
    WAIT_FOR(peer1.getListener().isListening(), 1000);
    WAIT_FOR(peer2.getListener().isListening(), 1000);

    processor.addPeer(LOCALHOST, 8011);

    // client should connect to anycast server, get a list of peers
    // and connect to those, then disconnect from the anycast server

    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8010), 1000);
    WAIT_FOR(processor.getPool().getPeer(LOCALHOST, 8011) == NULL, 1000);

    BOOST_CHECK_EQUAL(PeerStatusListener::CLOSING, peerStatus.statusMap[8011]);
    BOOST_CHECK_EQUAL(PeerStatusListener::READY, peerStatus.statusMap[8009]);
    BOOST_CHECK_EQUAL(PeerStatusListener::READY, peerStatus.statusMap[8010]);
    BOOST_CHECK_EQUAL(PeerStatusListener::HEALTHY, peerStatus.latestHealth);

    BOOST_CHECK_EQUAL(std::string("location_string"),
                      processor.getPool().getLocation());

    anycastServer.stop();
    peer1.stop();
    peer2.stop();
}

BOOST_FIXTURE_TEST_CASE( bootstrap, Fixture ) {
    testBootstrap(false);
}

BOOST_FIXTURE_TEST_CASE( bootstrap_ssl, SSLFixture ) {
    testBootstrap(true);
}

class EndpointDeclFixture : public ServerFixture {
public:
    EndpointDeclFixture() 
        : ServerFixture(), 
          u1("/"),
          u2("/class2/42/"),
          u3("/class2/42/class3/12/test/") {

    }

    void setup() {
        rclient = mockServer.getSystemClient();
        oi1 = make_shared<ObjectInstance>(1);
        oi2 = make_shared<ObjectInstance>(2);
        oi3 = make_shared<ObjectInstance>(3);

        client1->put(1, u1, oi1);

        oi2->setInt64(4, 42);
        client1->put(2, u2, oi2);

        client1->queueNotification(1, u1, notifs);
        client1->queueNotification(2, u2, notifs);
        client1->deliverNotifications(notifs);
        notifs.clear();

        oi3->setInt64(6, 12);
        oi3->setString(7, "test");
        client2->put(3, u3, oi3);
        client2->queueNotification(3, u3, notifs);
        client2->deliverNotifications(notifs);
        notifs.clear();
    }

    StoreClient::notif_t notifs;
    URI u1;
    URI u2;
    URI u3;
    StoreClient* rclient;
    shared_ptr<ObjectInstance> oi1;
    shared_ptr<ObjectInstance> oi2;
    shared_ptr<ObjectInstance> oi3;
};

// test endpoint_declare and endpoint_undeclare
BOOST_FIXTURE_TEST_CASE( endpoint_declare, ServerFixture ) {
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);

    StoreClient::notif_t notifs;

    URI u1("/");
    shared_ptr<ObjectInstance> oi1 = make_shared<ObjectInstance>(1);
    client1->put(1, u1, oi1);

    // check add
    URI u2_1("/class2/42/");
    shared_ptr<ObjectInstance> oi2_1 = make_shared<ObjectInstance>(2);
    oi2_1->setInt64(4, 42);
    client1->put(2, u2_1, oi2_1);

    URI u2_2("/class2/43/");
    shared_ptr<ObjectInstance> oi2_2 = make_shared<ObjectInstance>(2);
    oi2_2->setInt64(4, 43);
    client1->put(2, u2_2, oi2_2);

    client1->queueNotification(1, u1, notifs);
    client1->queueNotification(2, u2_1, notifs);
    client1->queueNotification(2, u2_2, notifs);
    client1->deliverNotifications(notifs);
    notifs.clear();

    StoreClient* rclient = mockServer.getSystemClient();
    WAIT_FOR(itemPresent(rclient, 2, u2_1), 1000);
    WAIT_FOR(itemPresent(rclient, 2, u2_2), 1000);
    shared_ptr<const ObjectInstance> roi2_2 = rclient->get(2, u2_2);
    BOOST_CHECK_EQUAL(43, roi2_2->getInt64(4));

    // check update
    MAC mac("aa:bb:cc:dd:ee:ff");
    oi2_1->setMAC(15, mac);
    client1->put(2, u2_1, oi2_1);
    client1->queueNotification(2, u2_1, notifs);
    client1->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(rclient->get(2, u2_1)->isSet(15, PropertyInfo::MAC), 1000);
    BOOST_CHECK_EQUAL(mac, rclient->get(2, u2_1)->getMAC(15));
    
    // check delete
    client1->remove(2, u2_2, true, &notifs);
    client1->queueNotification(2, u2_2, notifs);
    client1->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(!itemPresent(rclient, 2, u2_2), 1000);
    BOOST_CHECK(itemPresent(rclient, 2, u2_1));
    
}

static bool resolutions_pred(OpflexServerConnection* conn, void* user) {
    MockServerHandler* handler = (MockServerHandler*)conn->getHandler();
    return handler->hasResolutions();
}

class PolicyFixture : public ServerFixture {
public:
    PolicyFixture() 
        : ServerFixture(), 
          c4u("/class4/test/"),
          c5u("/class5/test/"),
          c6u("/class4/test/class6/test2/") {

    }

    void setup() {
        rclient = mockServer.getSystemClient();
        root = make_shared<ObjectInstance>(1);
        oi4 = make_shared<ObjectInstance>(4);
        oi5 = make_shared<ObjectInstance>(5);
        oi6 = make_shared<ObjectInstance>(6);
        
        // set up the server-side store
        oi4->setString(9, "test");
        oi6->setString(13, "test2");
        
        rclient->put(1, URI::ROOT, root);
        rclient->put(4, c4u, oi4);
        rclient->put(6, c6u, oi6);
        rclient->addChild(1, URI::ROOT, 8, 4, c4u);
        rclient->addChild(4, c4u, 12, 6, c6u);
        
        // create a local reference to the remote policy object
        oi5->setString(10, "test");
        oi5->addReference(11, 4, c4u);
        client2->put(5, c5u, oi5);
        
        client2->queueNotification(5, c5u, notifs);
        client2->deliverNotifications(notifs);
        notifs.clear();
    }

    StoreClient::notif_t notifs;
    URI c4u;
    URI c5u;
    URI c6u;
    StoreClient* rclient;
    shared_ptr<ObjectInstance> root;
    shared_ptr<ObjectInstance> oi4;
    shared_ptr<ObjectInstance> oi5;
    shared_ptr<ObjectInstance> oi6;
};

// test policy_resolve, policy_unresolve, policy_update
BOOST_FIXTURE_TEST_CASE( policy_resolve, PolicyFixture ) {
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);
    setup();

    // verify that the object is synced to the client
    BOOST_CHECK(itemPresent(client2, 5, c5u));
    WAIT_FOR(processor.getRefCount(c4u) > 0, 1000);
    WAIT_FOR(itemPresent(client2, 4, c4u), 1000);
    WAIT_FOR(itemPresent(client2, 6, c6u), 1000);
    BOOST_CHECK_EQUAL("test", client2->get(4, c4u)->getString(9));
    BOOST_CHECK_EQUAL("test2", client2->get(6, c6u)->getString(13));

    WAIT_FOR(mockServer.getListener().applyConnPred(resolutions_pred, NULL), 1000);

    // perform object updates
    vector<reference_t> replace;
    vector<reference_t> merge;
    vector<reference_t> del;
    
    oi4->setString(9, "moretesting");
    oi6->setString(13, "moretesting2");
    rclient->put(4, c4u, oi4);
    rclient->put(6, c6u, oi6);
    
    merge.push_back(make_pair(4, c4u));
    mockServer.policyUpdate(replace, merge, del);
    WAIT_FOR("moretesting" == client2->get(4, c4u)->getString(9), 1000);
    BOOST_CHECK_EQUAL("test2", client2->get(6, c6u)->getString(13));

    oi4->setString(9, "evenmore");
    rclient->put(4, c4u, oi4);
    rclient->remove(6, c6u, false);
    mockServer.policyUpdate(replace, merge, del);
    WAIT_FOR("evenmore" == client2->get(4, c4u)->getString(9), 1000);
    BOOST_CHECK_EQUAL("test2", client2->get(6, c6u)->getString(13));

    // replace
    merge.clear();
    replace.push_back(make_pair(4, c4u));
    mockServer.policyUpdate(replace, merge, del);
    WAIT_FOR(!itemPresent(client2, 6, c6u), 1000);

    // delete
    replace.clear();
    del.push_back(make_pair(4, c4u));
    mockServer.policyUpdate(replace, merge, del);
    WAIT_FOR(!itemPresent(client2, 4, c4u), 1000);

    // unresolve
    client2->remove(5, c5u, false, &notifs);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(!mockServer.getListener().applyConnPred(resolutions_pred, NULL), 1000);

    client2->put(5, c5u, oi5);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();
    WAIT_FOR(itemPresent(client2, 4, c4u), 1000);

    client2->remove(5, c5u, false, &notifs);
    client2->queueNotification(5, c5u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(!mockServer.getListener().applyConnPred(resolutions_pred, NULL), 1000);
}

// test policy resolve after connection ready
BOOST_FIXTURE_TEST_CASE( policy_resolve_reconnect, PolicyFixture ) {
    setup();
    WAIT_FOR(processor.getRefCount(c4u) > 0, 1000);
    WAIT_FOR(!processor.isObjNew(c5u), 1000);
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);

    WAIT_FOR(itemPresent(client2, 4, c4u), 1000);
    WAIT_FOR(itemPresent(client2, 6, c6u), 1000);
    BOOST_CHECK_EQUAL("test", client2->get(4, c4u)->getString(9));
    BOOST_CHECK_EQUAL("test2", client2->get(6, c6u)->getString(13));
}

class StateFixture : public ServerFixture {
public:
    StateFixture() 
        : ServerFixture(), 
          u1("/"),
          u2("/class2/42/"),
          u3("/class2/42/class3/12/test/") {

    }

    void setup() {
        rclient = mockServer.getSystemClient();
        oi1 = make_shared<ObjectInstance>(1);
        oi2 = make_shared<ObjectInstance>(2);
        oi3 = make_shared<ObjectInstance>(3);

        client1->put(1, u1, oi1);

        oi2->setInt64(4, 42);
        client1->put(2, u2, oi2);

        client1->queueNotification(1, u1, notifs);
        client1->queueNotification(2, u2, notifs);
        client1->deliverNotifications(notifs);
        notifs.clear();

        oi3->setInt64(6, 12);
        oi3->setString(7, "test");
        client2->put(3, u3, oi3);
        client2->queueNotification(3, u3, notifs);
        client2->deliverNotifications(notifs);
        notifs.clear();
    }

    StoreClient::notif_t notifs;
    URI u1;
    URI u2;
    URI u3;
    StoreClient* rclient;
    shared_ptr<ObjectInstance> oi1;
    shared_ptr<ObjectInstance> oi2;
    shared_ptr<ObjectInstance> oi3;
};

// test state_report
BOOST_FIXTURE_TEST_CASE( state_report, StateFixture ) {
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);
    setup();

    // check add
    WAIT_FOR(itemPresent(rclient, 3, u3), 1000);
    BOOST_CHECK_EQUAL(12, rclient->get(3, u3)->getInt64(6));

    // check update
    oi3->setString(16, "update");
    client2->put(3, u3, oi3);
    client2->queueNotification(3, u3, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(rclient->get(3, u3)->isSet(16, PropertyInfo::STRING), 1000);
    BOOST_CHECK_EQUAL("update", rclient->get(3, u3)->getString(16));
}

// test state_report after connection ready
BOOST_FIXTURE_TEST_CASE( state_report_reconnect, StateFixture ) {
    setup();
    WAIT_FOR(!processor.isObjNew(u3), 1000);
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);

    // check add
    WAIT_FOR(itemPresent(rclient, 3, u3), 1000);
    BOOST_CHECK_EQUAL(12, rclient->get(3, u3)->getInt64(6));
}

class EndpointResFixture : public ServerFixture {
public:
    EndpointResFixture() 
        : ServerFixture(), 
          c8u("/class8/test/"),
          c9u("/class9/test/"),
          c10u("/class8/test/class10/test2/") {

    }

    void setup() {
        // set up the server-side store
        rclient = mockServer.getSystemClient();
        root = make_shared<ObjectInstance>(1);
        oi8 = make_shared<ObjectInstance>(8);
        oi10 = make_shared<ObjectInstance>(10);
        oi8->setString(17, "test");
        oi10->setString(21, "test2");
        
        rclient->put(1, URI::ROOT, root);
        rclient->put(8, c8u, oi8);
        rclient->put(10, c10u, oi10);
        rclient->addChild(1, URI::ROOT, 22, 8, c8u);
        rclient->addChild(8, c8u, 20, 10, c10u);
        
        // create a local reference to the remote policy object
        oi9 = make_shared<ObjectInstance>(9);
        oi9->setString(18, "test");
        oi9->setReference(19, 8, c8u);
        client2->put(9, c9u, oi9);
        
        client2->queueNotification(9, c9u, notifs);
        client2->deliverNotifications(notifs);
        notifs.clear();

    }

    StoreClient::notif_t notifs;
    URI c8u;
    URI c9u;
    URI c10u;
    StoreClient* rclient;
    shared_ptr<ObjectInstance> root;
    shared_ptr<ObjectInstance> oi8;
    shared_ptr<ObjectInstance> oi9;
    shared_ptr<ObjectInstance> oi10;
};

// test endpoint_resolve, endpoint_unresolve, endpoint_update
BOOST_FIXTURE_TEST_CASE( endpoint_resolve, EndpointResFixture ) {
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);
    setup();

    // verify that the object is synced to the client
    BOOST_CHECK(itemPresent(client2, 9, c9u));
    WAIT_FOR(processor.getRefCount(c8u) > 0, 1000);
    WAIT_FOR(itemPresent(client2, 8, c8u), 1000);
    WAIT_FOR(itemPresent(client2, 10, c10u), 1000);
    BOOST_CHECK_EQUAL("test", client2->get(8, c8u)->getString(17));
    BOOST_CHECK_EQUAL("test2", client2->get(10, c10u)->getString(21));
    WAIT_FOR(mockServer.getListener().applyConnPred(resolutions_pred, NULL), 1000);

    // perform object updates
    vector<reference_t> replace;
    vector<reference_t> merge;
    vector<reference_t> del;
    
    oi8->setString(17, "moretesting");
    oi10->setString(21, "moretesting2");
    rclient->put(8, c8u, oi8);
    rclient->put(10, c10u, oi10);
    
    // replace
    replace.push_back(make_pair(8, c8u));
    mockServer.endpointUpdate(replace, del);
    WAIT_FOR("moretesting" == client2->get(8, c8u)->getString(17), 1000);
    WAIT_FOR("moretesting2" == client2->get(10, c10u)->getString(21), 1000);

    oi8->setString(17, "evenmore");
    rclient->put(8, c8u, oi8);
    rclient->remove(10, c10u, false);
    mockServer.endpointUpdate(replace, del);
    WAIT_FOR("evenmore" == client2->get(8, c8u)->getString(17), 1000);
    WAIT_FOR(!itemPresent(client2, 10, c10u), 1000);

    // delete
    replace.clear();
    del.push_back(make_pair(8, c8u));
    mockServer.endpointUpdate(replace, del);
    WAIT_FOR(!itemPresent(client2, 8, c8u), 1000);

    // unresolve
    client2->remove(9, c9u, false, &notifs);
    client2->queueNotification(9, c9u, notifs);
    client2->deliverNotifications(notifs);
    notifs.clear();

    WAIT_FOR(!mockServer.getListener().applyConnPred(resolutions_pred, NULL), 1000);
}

// test endpoint_resolve after connection ready
BOOST_FIXTURE_TEST_CASE( endpoint_resolve_reconnect, EndpointResFixture ) {
    setup();
    WAIT_FOR(processor.getRefCount(c8u) > 0, 1000);
    WAIT_FOR(!processor.isObjNew(c9u), 1000);
    startClient();
    WAIT_FOR(connReady(processor.getPool(), LOCALHOST, 8009), 1000);

    // verify that the object is synced to the client
    BOOST_CHECK(itemPresent(client2, 9, c9u));
    WAIT_FOR(processor.getRefCount(c8u) > 0, 1000);
    WAIT_FOR(itemPresent(client2, 8, c8u), 1000);
    WAIT_FOR(itemPresent(client2, 10, c10u), 1000);
    BOOST_CHECK_EQUAL("test", client2->get(8, c8u)->getString(17));
    BOOST_CHECK_EQUAL("test2", client2->get(10, c10u)->getString(21));
    WAIT_FOR(mockServer.getListener().applyConnPred(resolutions_pred, NULL), 1000);
}

BOOST_AUTO_TEST_SUITE_END()
