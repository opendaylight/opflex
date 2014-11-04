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

class Fixture : public BaseFixture {
public:
    Fixture() : processor(&db) {
        processor.setDelay(50);
        processor.start();
    }

    ~Fixture() {
        processor.stop();
    }

    Processor processor;
};

BOOST_AUTO_TEST_SUITE(Processor_test)

BOOST_FIXTURE_TEST_CASE( mo_serialize , BaseFixture ) {
    MOSerializer serializer(&db);
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);

    shared_ptr<ObjectInstance> oi = 
        shared_ptr<ObjectInstance>(new ObjectInstance(1));
    oi->setUInt64(1, 42);
    oi->addString(2, "test1");
    oi->addString(2, "test2");
    URI uri("/");
    client1->put(1, uri, oi);

    shared_ptr<ObjectInstance> oi2 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(2));
    oi2->setInt64(4, -42);
    URI uri2("/class2/-42");
    client1->put(2, uri2, oi2);
    client1->addChild(1, uri, 3, 2, uri2);

    shared_ptr<ObjectInstance> oi3 = 
        shared_ptr<ObjectInstance>(new ObjectInstance(2));
    oi3->setInt64(4, -84);
    URI uri3("/class2/-84");
    client1->put(2, uri3, oi3);
    client1->addChild(1, uri, 3, 2, uri3);

    writer.StartObject();

    writer.String("result");

    writer.StartObject();
    writer.String("policy");
    writer.StartArray();
    serializer.serialize<StringBuffer>(1, uri, *client1, writer);
    writer.EndArray();

    writer.String("prr");
    writer.Uint(3600);
    writer.EndObject();
    
    writer.String("id");
    writer.Uint(42);
    writer.EndObject();

    //std::cout << buffer.GetString() << std::endl;

    Document d;
    d.Parse(buffer.GetString());
    const Value& result = d["result"];
    BOOST_CHECK(result.IsObject());
    const Value& prr = result["prr"];
    BOOST_CHECK(prr.IsUint());
    BOOST_CHECK_EQUAL(3600, prr.GetInt());

    const Value& policy = result["policy"];
    BOOST_CHECK(policy.IsArray());
    BOOST_CHECK_EQUAL(3, policy.Size());
    
    const Value& mo1 = policy[SizeType(0)];
    BOOST_CHECK_EQUAL("class1", const_cast<char *>(mo1["subject"].GetString()));
    BOOST_CHECK_EQUAL("/", const_cast<char *>(mo1["name"].GetString()));
    const Value& mo1props = mo1["properties"];
    BOOST_CHECK(mo1props.IsArray());
    BOOST_CHECK_EQUAL(2, mo1props.Size());
    const Value& mo1children = mo1["children"];
    BOOST_CHECK(mo1children.IsArray());
    BOOST_CHECK_EQUAL(2, mo1children.Size());

    for (SizeType i = 0; i < mo1children.Size(); ++i) {
        const Value& child = mo1children[i];
        BOOST_CHECK(child.IsString());
        BOOST_CHECK(std::string("/class2/-42") == child.GetString() ||
                    std::string("/class2/-84") == child.GetString());

        const Value& mo2 = policy[SizeType(i+1)];
        BOOST_CHECK(std::string("/class2/-42") == mo2["name"].GetString() ||
                    std::string("/class2/-84") == mo2["name"].GetString());
        BOOST_CHECK_EQUAL("class2", const_cast<char *>(mo2["subject"].GetString()));
        BOOST_CHECK_EQUAL("class1", const_cast<char *>(mo2["parent_subject"].GetString()));
        BOOST_CHECK_EQUAL("/", const_cast<char *>(mo2["parent_name"].GetString()));
        BOOST_CHECK_EQUAL("class2", const_cast<char *>(mo2["parent_relation"].GetString()));
        const Value& mo2props = mo2["properties"];
        BOOST_CHECK(mo2props.IsArray());
        BOOST_CHECK_EQUAL(1, mo2props.Size());
        const Value& mo2children = mo2["children"];
        BOOST_CHECK(mo2children.IsArray());
        BOOST_CHECK_EQUAL(0, mo2children.Size());

    }


}

BOOST_FIXTURE_TEST_CASE( mo_deserialize , BaseFixture ) {
    StoreClient::notif_t notifs;

    static const char buffer[] = 
        "{\"result\":{\"policy\":[{\"subject\":\"class1\",\"name\""
        ":\"/\",\"properties\":[{\"name\":\"prop2\",\"data\":[\"te"
        "st1\",\"test2\"]},{\"name\":\"prop1\",\"data\":42}],\"chi"
        "ldren\":[\"/class2/-84\",\"/class2/-42\"]},{\"subject\":"
        "\"class2\",\"name\":\"/class2/-84\",\"properties\":[{\"na"
        "me\":\"prop4\",\"data\":-84}],\"children\":[],\"parent_su"
        "bject\":\"class1\",\"parent_name\":\"/\",\"parent_relatio"
        "n\":\"class2\"},{\"subject\":\"class2\",\"name\":\"/class"
        "2/-42\",\"properties\":[{\"name\":\"prop4\",\"data\":-42}"
        "],\"children\":[],\"parent_subject\":\"class1\",\"parent_"
        "name\":\"/\",\"parent_relation\":\"class2\"}],\"prr\":360"
        "0},\"id\":42}";

    MOSerializer serializer(&db);
    StoreClient& sysClient = db.getStoreClient("_SYSTEM_");
    Document d;
    d.Parse(buffer);
    const Value& result = d["result"];
    const Value& policy = result["policy"];
    BOOST_CHECK(policy.IsArray());
    for (SizeType i = 0; i < policy.Size(); ++i) {
        serializer.deserialize(policy[i], sysClient, false, &notifs);
    }
    
    URI uri("/");
    URI uri2("/class2/-42");
    URI uri3("/class2/-84");
    shared_ptr<const ObjectInstance> oi = sysClient.get(1, uri);
    shared_ptr<const ObjectInstance> oi2 = sysClient.get(2, uri2);

    BOOST_CHECK_EQUAL(42, oi->getUInt64(1));
    BOOST_CHECK_EQUAL(2, oi->getStringSize(2));
    BOOST_CHECK_EQUAL("test1", oi->getString(2, 0));
    BOOST_CHECK_EQUAL("test2", oi->getString(2, 1));
    BOOST_CHECK_EQUAL(-42, oi2->getInt64(4));

    std::vector<URI> children;
    sysClient.getChildren(2, uri, 3, 2, children);
    BOOST_CHECK_EQUAL(2, children.size());
    BOOST_CHECK(uri2.toString() == children.at(0).toString() ||
                uri3.toString() == children.at(0).toString());
    BOOST_CHECK(uri2.toString() == children.at(1).toString() ||
                uri3.toString() == children.at(1).toString());
    children.clear();

    BOOST_CHECK(notifs.find(uri) != notifs.end());
    BOOST_CHECK(notifs.find(uri2) != notifs.end());
    BOOST_CHECK(notifs.find(uri3) != notifs.end());
    notifs.clear();

    // remove both children
    static const char buffer2[] = 
        "{\"result\":{\"policy\":[{\"subject\":\"class1\",\"name\""
        ":\"/\",\"properties\":[{\"name\":\"prop2\",\"data\":[\"te"
        "st3\",\"test4\"]},{\"name\":\"prop1\",\"data\":84}],\"chi"
        "ldren\":[]}],\"prr\":360"
        "0},\"id\":42}";
    Document d2;
    d2.Parse(buffer2);
    const Value& result2 = d2["result"];
    const Value& policy2 = result2["policy"];
    BOOST_CHECK(policy2.IsArray());
    for (SizeType i = 0; i < policy2.Size(); ++i) {
        serializer.deserialize(policy2[i], sysClient, true, &notifs);
    }

    oi = sysClient.get(1, uri);
    BOOST_CHECK_EQUAL(84, oi->getUInt64(1));
    BOOST_CHECK_EQUAL(2, oi->getStringSize(2));
    BOOST_CHECK_EQUAL("test3", oi->getString(2, 0));
    BOOST_CHECK_EQUAL("test4", oi->getString(2, 1));
    BOOST_CHECK_THROW(sysClient.get(2, uri2), out_of_range);

    BOOST_CHECK(notifs.find(uri) != notifs.end());
    BOOST_CHECK(notifs.find(uri2) != notifs.end());
    BOOST_CHECK(notifs.find(uri3) != notifs.end());
    notifs.clear();

    // restore original
    for (SizeType i = 0; i < policy.Size(); ++i) {
        serializer.deserialize(policy[i], sysClient, true, &notifs);
    }
    sysClient.getChildren(2, uri, 3, 2, children);
    BOOST_CHECK_EQUAL(2, children.size());
    children.clear();

    BOOST_CHECK(notifs.find(uri) != notifs.end());
    BOOST_CHECK(notifs.find(uri2) != notifs.end());
    BOOST_CHECK(notifs.find(uri3) != notifs.end());
    notifs.clear();

    // remove only one child
    static const char buffer3[] = 
        "{\"result\":{\"policy\":[{\"subject\":\"class1\",\"name\""
        ":\"/\",\"properties\":[{\"name\":\"prop2\",\"data\":[\"te"
        "st1\",\"test2\"]},{\"name\":\"prop1\",\"data\":42}],\"chi"
        "ldren\":[\"/class2/-84\"]}],\"prr\":3600},\"id\":42}";
    Document d3;
    d3.Parse(buffer3);
    const Value& result3 = d3["result"];
    const Value& policy3 = result3["policy"];
    BOOST_CHECK(policy3.IsArray());
    for (SizeType i = 0; i < policy3.Size(); ++i) {
        serializer.deserialize(policy3[i], sysClient, true, &notifs);
    }
    sysClient.getChildren(2, uri, 3, 2, children);
    BOOST_CHECK_EQUAL(1, children.size());
    BOOST_CHECK_EQUAL(uri3.toString(), children.at(0).toString());

    BOOST_CHECK(notifs.find(uri) != notifs.end());
    BOOST_CHECK(notifs.find(uri2) != notifs.end());
    BOOST_CHECK(notifs.find(uri3) == notifs.end());
    notifs.clear();
}

/*
BOOST_FIXTURE_TEST_CASE( write, Fixture ) {

    StoreClient::notif_t notifs;
    shared_ptr<ObjectInstance> oi = 
        shared_ptr<ObjectInstance>(new ObjectInstance(1));
    oi->setUInt64(1, 42);
    URI uri("/");
    client1->put(1, uri, oi);
    client1->queueNotification(1, uri, notifs);
    client1->deliverNotifications(notifs);
    notifs.clear();

    oi = shared_ptr<ObjectInstance>(new ObjectInstance(*oi));
    oi->setUInt64(1, 43);
    client1->put(1, uri, oi);
    client1->queueNotification(1, uri, notifs);
    client1->deliverNotifications(notifs);
    notifs.clear();
}
*/

static bool itemPresent(StoreClient* client,
                        class_id_t class_id, const URI& uri) {
    try {
        client->get(class_id, uri);
        return true;
    } catch (std::out_of_range e) {
        return false;
    }
}

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

BOOST_AUTO_TEST_SUITE_END()
