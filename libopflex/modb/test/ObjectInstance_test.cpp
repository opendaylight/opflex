/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for ObjectInstance class.
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
#include <boost/assign/list_of.hpp>
#include <vector>

#include "opflex/modb/mo-internal/ObjectInstance.h"

using namespace opflex::modb;
using boost::assign::list_of;
using boost::shared_ptr;
using std::out_of_range;
using std::string;
using mointernal::ObjectInstance;

BOOST_AUTO_TEST_SUITE(ObjectInstance_test)

void checkScalar(shared_ptr<ObjectInstance> oi) {
    BOOST_CHECK_EQUAL(0xdeadbeef, oi->getUInt64(1));
    BOOST_CHECK_EQUAL(-42, oi->getInt64(2));
    BOOST_CHECK_EQUAL("value", oi->getString(3));
    BOOST_CHECK_THROW(oi->getString(42), out_of_range);
    BOOST_CHECK_THROW(oi->getInt64(42), out_of_range);
    BOOST_CHECK_THROW(oi->getUInt64(42), out_of_range);
}

BOOST_AUTO_TEST_CASE( scalar ) {
    shared_ptr<ObjectInstance> oi =
        shared_ptr<ObjectInstance>(new ObjectInstance(1));

    oi->setUInt64(1, 0xdeadbeef);
    oi->setInt64(2, -42);
    oi->setString(3, "value");
    checkScalar(oi);

    // check copy constructor
    oi = shared_ptr<ObjectInstance>(new ObjectInstance(*oi));
    checkScalar(oi);

    // check assignment
    shared_ptr<ObjectInstance> oi2 =
        shared_ptr<ObjectInstance>(new ObjectInstance(1));
    *oi2 = *oi;
    checkScalar(oi);
    checkScalar(oi2);

    BOOST_CHECK(*oi == *oi2);
    oi2->setString(3, "notvalue");
    BOOST_CHECK(*oi != *oi2);
    oi2->setString(3, "value");
    BOOST_CHECK(*oi == *oi2);
    oi2->setInt64(2, 128);
    BOOST_CHECK(*oi != *oi2);
    oi2->setInt64(2, -42);
    BOOST_CHECK(*oi == *oi2);
    oi2->setUInt64(1, 0xff);
    BOOST_CHECK(*oi != *oi2);
    oi2->setUInt64(1, 0xdeadbeef);
    BOOST_CHECK(*oi == *oi2);

    BOOST_CHECK(oi2->isSet(2, PropertyInfo::S64));
    oi2->unset(2, PropertyInfo::S64, PropertyInfo::SCALAR);
    BOOST_CHECK(!oi2->isSet(2, PropertyInfo::S64));

    BOOST_CHECK(*oi != *oi2);
}

void checkVector(shared_ptr<ObjectInstance> oi) {
    // Check uint64_t
    BOOST_CHECK_EQUAL(5, oi->getUInt64(1, 0));
    BOOST_CHECK_EQUAL(6, oi->getUInt64(1, 1));
    BOOST_CHECK_EQUAL(2, oi->getUInt64Size(1));
    BOOST_CHECK_THROW(oi->getUInt64(42, 20), out_of_range);
    BOOST_CHECK_THROW(oi->getUInt64(1, 2), out_of_range);

    // Check int64_t
    BOOST_CHECK_EQUAL(42, oi->getInt64(1, 0));
    BOOST_CHECK_EQUAL(-42, oi->getInt64(1, 1));
    BOOST_CHECK_EQUAL(3, oi->getInt64Size(1));
    BOOST_CHECK_THROW(oi->getInt64(42, 20), out_of_range);
    BOOST_CHECK_THROW(oi->getInt64(1, 3), out_of_range);

    // Check string
    BOOST_CHECK_EQUAL("str1", oi->getString(1, 0));
    BOOST_CHECK_EQUAL("str2", oi->getString(1, 1));
    BOOST_CHECK_EQUAL(4, oi->getStringSize(1));
    BOOST_CHECK_THROW(oi->getString(42, 20), out_of_range);
    BOOST_CHECK_THROW(oi->getString(1, 4), out_of_range);
}

BOOST_AUTO_TEST_CASE( vector ) {
    shared_ptr<ObjectInstance> oi =
        shared_ptr<ObjectInstance>(new ObjectInstance(1));
    oi->addUInt64(1, 5);
    oi->addUInt64(1, 6);
    oi->addInt64(1, 42);
    oi->addInt64(1, -42);
    oi->addInt64(1, 1);
    oi->addString(1, "str1");
    oi->addString(1, "str2");
    oi->addString(1, "str3");
    oi->addString(1, "str4");
    checkVector(oi);

    // check copy constructor
    oi = shared_ptr<ObjectInstance>(new ObjectInstance(*oi));
    checkVector(oi);

    // check assignment
    shared_ptr<ObjectInstance> oi2 =
        shared_ptr<ObjectInstance>(new ObjectInstance(1));
    *oi2 = *oi;
    checkVector(oi);
    checkVector(oi2);

    oi = shared_ptr<ObjectInstance>(new ObjectInstance(1));
    const std::vector<uint64_t> uv = list_of(5)(6);
    oi->setUInt64(1, uv);
    const std::vector<int64_t> sv = list_of(42)(-42)(1);
    oi->setInt64(1, sv);
    const std::vector<string> stv = list_of("str1")("str2")("str3")("str4");
    oi->setString(1, stv);
    checkVector(oi);

    BOOST_CHECK(*oi == *oi2);
    oi2->addUInt64(1, 5);
    BOOST_CHECK(*oi != *oi2);
    oi2->setUInt64(1, uv);
    BOOST_CHECK(*oi == *oi2);
    oi2->unset(1, PropertyInfo::S64, PropertyInfo::VECTOR);
    BOOST_CHECK(*oi != *oi2);
    oi2->addInt64(1, 5);
    BOOST_CHECK(*oi != *oi2);
    oi2->setInt64(1, sv);
    BOOST_CHECK(*oi == *oi2);
    oi2->unset(1, PropertyInfo::STRING, PropertyInfo::VECTOR);
    BOOST_CHECK(*oi != *oi2);
    oi2->addString(1, "blah");
    BOOST_CHECK(*oi != *oi2);
    oi2->setString(1, stv);
    BOOST_CHECK(*oi == *oi2);

    oi2->unset(1, PropertyInfo::U64, PropertyInfo::VECTOR);
    oi2->unset(1, PropertyInfo::S64, PropertyInfo::VECTOR);
    oi2->unset(1, PropertyInfo::STRING, PropertyInfo::VECTOR);
    BOOST_CHECK_EQUAL(0, oi2->getUInt64Size(1));
    BOOST_CHECK_EQUAL(0, oi2->getInt64Size(1));
    BOOST_CHECK_EQUAL(0, oi2->getStringSize(1));

    BOOST_CHECK(*oi != *oi2);

}

BOOST_AUTO_TEST_SUITE_END()
