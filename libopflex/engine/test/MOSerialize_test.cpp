/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for MOSerializer class.
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

#include "opflex/engine/internal/MOSerializer.h"

#include "BaseFixture.h"

using namespace opflex::engine;
using namespace opflex::engine::internal;
using namespace opflex::modb;
using namespace opflex::modb::mointernal;
using namespace opflex::logging;
using namespace rapidjson;

using mointernal::ObjectInstance;
using std::out_of_range;
using std::string;
using std::make_pair;

BOOST_AUTO_TEST_SUITE(MOSerialize_test)

BOOST_FIXTURE_TEST_CASE( mo_serialize , BaseFixture ) {
    MOSerializer serializer(&db);
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);

    OF_SHARED_PTR<ObjectInstance> oi =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(1));
    oi->setUInt64(1, 42);
    oi->addString(2, "test1");
    oi->addString(2, "test2");
    URI uri("/");
    client1->put(1, uri, oi);

    OF_SHARED_PTR<ObjectInstance> oi2 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(2));
    oi2->setInt64(4, -42);
    URI uri2("/class2/-42");
    client1->put(2, uri2, oi2);
    client1->addChild(1, uri, 3, 2, uri2);

    OF_SHARED_PTR<ObjectInstance> oi3 =
        OF_SHARED_PTR<ObjectInstance>(new ObjectInstance(2));
    oi3->setInt64(4, -84);
    URI uri3("/class2/-84");
    client1->put(2, uri3, oi3);
    client1->addChild(1, uri, 3, 2, uri3);

    writer.StartObject();

    writer.String("result");

    writer.StartObject();
    writer.String("policy");
    writer.StartArray();
    serializer.serialize(1, uri, *client1, writer);
    writer.EndArray();

    writer.String("prr");
    writer.Uint(7200);
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
    BOOST_CHECK_EQUAL(7200, prr.GetInt());

    const Value& policy = result["policy"];
    BOOST_CHECK(policy.IsArray());
    BOOST_CHECK_EQUAL(3, policy.Size());

    const Value& mo1 = policy[SizeType(0)];
    BOOST_CHECK_EQUAL("class1", const_cast<char *>(mo1["subject"].GetString()));
    BOOST_CHECK_EQUAL("/", const_cast<char *>(mo1["uri"].GetString()));
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
        BOOST_CHECK(std::string("/class2/-42") == mo2["uri"].GetString() ||
                    std::string("/class2/-84") == mo2["uri"].GetString());
        BOOST_CHECK_EQUAL("class2", const_cast<char *>(mo2["subject"].GetString()));
        BOOST_CHECK_EQUAL("class1", const_cast<char *>(mo2["parent_subject"].GetString()));
        BOOST_CHECK_EQUAL("/", const_cast<char *>(mo2["parent_uri"].GetString()));
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
        "{\"result\":{\"policy\":[{\"subject\":\"class1\",\"uri\""
        ":\"/\",\"properties\":[{\"name\":\"prop2\",\"data\":[\"te"
        "st1\",\"test2\"]},{\"name\":\"prop1\",\"data\":42}],\"chi"
        "ldren\":[\"/class2/-84\",\"/class2/-42\"]},{\"subject\":"
        "\"class2\",\"uri\":\"/class2/-84\",\"properties\":[{\"na"
        "me\":\"prop4\",\"data\":-84}],\"children\":[],\"parent_su"
        "bject\":\"class1\",\"parent_uri\":\"/\",\"parent_relatio"
        "n\":\"class2\"},{\"subject\":\"class2\",\"uri\":\"/class"
        "2/-42\",\"properties\":[{\"name\":\"prop4\",\"data\":-42}"
        "],\"children\":[],\"parent_subject\":\"class1\",\"parent_"
        "uri\":\"/\",\"parent_relation\":\"class2\"}],\"prr\":540"
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
    OF_SHARED_PTR<const ObjectInstance> oi = sysClient.get(1, uri);
    OF_SHARED_PTR<const ObjectInstance> oi2 = sysClient.get(2, uri2);

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
        "{\"result\":{\"policy\":[{\"subject\":\"class1\",\"uri\""
        ":\"/\",\"properties\":[{\"name\":\"prop2\",\"data\":[\"te"
        "st3\",\"test4\"]},{\"name\":\"prop1\",\"data\":84}],\"chi"
        "ldren\":[]}],\"prr\":540"
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
        "{\"result\":{\"policy\":[{\"subject\":\"class1\",\"uri\""
        ":\"/\",\"properties\":[{\"name\":\"prop2\",\"data\":[\"te"
        "st1\",\"test2\"]},{\"name\":\"prop1\",\"data\":42}],\"chi"
        "ldren\":[\"/class2/-84\"]}],\"prr\":7200},\"id\":42}";
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
    children.clear();

    // Notify parent that child removed
    BOOST_CHECK(notifs.find(uri) != notifs.end());
    // child changed
    BOOST_CHECK(notifs.find(uri2) != notifs.end());
    // child not changed
    BOOST_CHECK(notifs.find(uri3) == notifs.end());
    notifs.clear();

    // Don't change anything
    for (SizeType i = 0; i < policy3.Size(); ++i) {
        serializer.deserialize(policy3[i], sysClient, true, &notifs);
    }
    sysClient.getChildren(2, uri, 3, 2, children);
    BOOST_CHECK_EQUAL(1, children.size());
    BOOST_CHECK_EQUAL(uri3.toString(), children.at(0).toString());
    BOOST_CHECK_EQUAL(0, notifs.size());
}

BOOST_FIXTURE_TEST_CASE( types , BaseFixture ) {
    MOSerializer serializer(&db);
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);

    StoreClient& sysClient = db.getStoreClient("_SYSTEM_");
    URI c2u("/class2/32/");
    URI c4u("/class4/test/");
    URI brokenref("class4/blahblah/");
    URI c5u("/class5/test/");
    URI c6u("/class4/test/class6/test2/");
    URI c7u("/class4/test/class7/0");

    OF_SHARED_PTR<ObjectInstance> oi1 = OF_MAKE_SHARED<ObjectInstance>(1);
    OF_SHARED_PTR<ObjectInstance> oi2 = OF_MAKE_SHARED<ObjectInstance>(2);
    OF_SHARED_PTR<ObjectInstance> oi4 = OF_MAKE_SHARED<ObjectInstance>(4);
    OF_SHARED_PTR<ObjectInstance> oi5 = OF_MAKE_SHARED<ObjectInstance>(5);
    OF_SHARED_PTR<ObjectInstance> oi6 = OF_MAKE_SHARED<ObjectInstance>(6);
    OF_SHARED_PTR<ObjectInstance> oi7 = OF_MAKE_SHARED<ObjectInstance>(7);

    oi2->setInt64(4, 32);
    oi2->setMAC(15, MAC("aa:bb:cc:dd:ee:ff"));
    oi5->setString(10, "test");
    oi5->addReference(11, 4, c4u);
    // add an unresolved reference
    oi5->addReference(11, 4, brokenref);
    oi4->setString(9, "test");
    oi6->setString(13, "test2");
    oi7->setUInt64(14, 0);

    sysClient.put(1, URI::ROOT, oi1);
    sysClient.put(2, c2u, oi2);
    sysClient.put(4, c4u, oi4);
    sysClient.put(5, c5u, oi5);
    sysClient.put(6, c6u, oi6);
    sysClient.put(7, c7u, oi7);
    sysClient.addChild(1, URI::ROOT, 3, 2, c2u);
    sysClient.addChild(1, URI::ROOT, 8, 4, c4u);
    sysClient.addChild(1, URI::ROOT, 24, 5, c5u);
    sysClient.addChild(4, c4u, 12, 6, c6u);
    sysClient.addChild(4, c4u, 25, 7, c7u);

    writer.StartArray();
    serializer.serialize(1, URI::ROOT, sysClient, writer);
    writer.EndArray();
    string str(buffer.GetString());

    //std::cerr << str << std::endl;

    sysClient.remove(1, URI::ROOT, true);
    BOOST_CHECK_THROW(sysClient.get(1, URI::ROOT), out_of_range);
    BOOST_CHECK_THROW(sysClient.get(2, c2u), out_of_range);
    BOOST_CHECK_THROW(sysClient.get(4, c4u), out_of_range);
    BOOST_CHECK_THROW(sysClient.get(5, c5u), out_of_range);
    BOOST_CHECK_THROW(sysClient.get(6, c6u), out_of_range);
    BOOST_CHECK_THROW(sysClient.get(7, c7u), out_of_range);

    Document d;
    d.Parse(str.c_str());
    Value::ConstValueIterator it;
    for (it = d.Begin(); it != d.End(); ++it) {
        serializer.deserialize(*it, sysClient, true, NULL);
    }
    BOOST_CHECK_EQUAL("test", sysClient.get(4, c4u)->getString(9));
    BOOST_CHECK_EQUAL("test", sysClient.get(5, c5u)->getString(10));
    BOOST_CHECK(make_pair((class_id_t)4ul, c4u) ==
                sysClient.get(5, c5u)->getReference(11, 0));
    BOOST_CHECK_EQUAL("test2", sysClient.get(6, c6u)->getString(13));
    BOOST_CHECK_EQUAL(0, sysClient.get(7, c7u)->getUInt64(14));
    BOOST_CHECK_EQUAL(MAC("aa:bb:cc:dd:ee:ff"), sysClient.get(2, c2u)->getMAC(15));

    // not testing result for now...just checking for crashes
    serializer.displayMODB(std::cout, true, true, false, 120);
    serializer.displayUnresolved(std::cout, true, true);
}

BOOST_AUTO_TEST_SUITE_END()
