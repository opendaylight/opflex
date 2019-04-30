/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for MOs.
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


#include <stdexcept>
#include <boost/test/unit_test.hpp>
#include <boost/optional.hpp>

#include "opflex/modb/URIBuilder.h"
#include "FrameworkFixture.h"
#include "TestListener.h"

// This is a hand-coded version of testmodel that exercises the
// features a generated model would require
#include "testmodel/class1.h"

BOOST_AUTO_TEST_SUITE(MO_test)

using namespace opflex;
using namespace modb;
using namespace ofcore;

using boost::optional;

BOOST_FIXTURE_TEST_CASE( model, FrameworkFixture ) {
    TestListener listener;

    testmodel::class1::registerListener(framework, &listener);
    testmodel::class2::registerListener(framework, &listener);

    Mutator mutator1(framework, "owner1");
    OF_SHARED_PTR<testmodel::class1> root =
        testmodel::class1::createRootElement(framework);
    root->setProp1(42);
    OF_SHARED_PTR<testmodel::class2> c2 = root->addClass2(-42);
    mutator1.commit();

    Mutator mutator2(framework, "owner2");
    OF_SHARED_PTR<testmodel::class3> c3 = c2->addClass3(17, "test");

    OF_SHARED_PTR<testmodel::class4> c4 = root->addClass4("class4name");
    OF_SHARED_PTR<testmodel::class5> c5 = root->addClass5("class5name");
    c5->addClass4Ref("class4name");
    mutator2.commit();

    URI uri1("/");
    URI uri2("/class2/-42/");
    URI uri3("/class2/-42/class3/17/test/");

    WAIT_FOR(listener.contains(uri1), 500);
    WAIT_FOR(listener.contains(uri2), 500);

    optional<OF_SHARED_PTR<testmodel::class1> > r1 =
        testmodel::class1::resolve(framework, URI("/wrong"));
    BOOST_CHECK(!r1);
    r1 = testmodel::class1::resolve(framework, uri1);
    BOOST_CHECK(r1);
    BOOST_CHECK_EQUAL(42, r1.get()->getProp1().get());
    BOOST_CHECK_EQUAL(uri1.toString(), r1.get()->getURI().toString());
    r1 = testmodel::class1::resolve(framework);
    BOOST_CHECK((bool)r1);
    BOOST_CHECK_EQUAL(42, r1.get()->getProp1().get());

    optional<OF_SHARED_PTR<testmodel::class2> > r2 =
        testmodel::class2::resolve(framework, URI("/wrong"));
    BOOST_CHECK(!r2);
    r2 = testmodel::class2::resolve(framework, uri2);
    BOOST_CHECK(r2);
    BOOST_CHECK_EQUAL(-42, r2.get()->getProp4().get());
    r2 = testmodel::class2::resolve(framework, -42);
    BOOST_CHECK((bool)r2);
    BOOST_CHECK_EQUAL(-42, r2.get()->getProp4().get());
    r2 = r1.get()->resolveClass2(-42);
    BOOST_CHECK(r2);
    BOOST_CHECK_EQUAL(-42, r2.get()->getProp4().get());

    std::vector<OF_SHARED_PTR<testmodel::class2> > out2;
    r1->get()->resolveClass2(out2);
    BOOST_CHECK_EQUAL(1, out2.size());
    BOOST_CHECK_EQUAL(-42, out2[0]->getProp4().get());

    optional<OF_SHARED_PTR<testmodel::class3> > r3 =
        testmodel::class3::resolve(framework, URI("/wrong"));
    r3 = testmodel::class3::resolve(framework, uri3);
    BOOST_CHECK(r3);
    BOOST_CHECK_EQUAL(17, r3.get()->getProp6().get());
    BOOST_CHECK_EQUAL("test", r3.get()->getProp7().get());
    r3 = testmodel::class3::resolve(framework, -42, 17, "test");
    BOOST_CHECK((bool)r3);
    BOOST_CHECK_EQUAL(17, r3.get()->getProp6().get());
    BOOST_CHECK_EQUAL("test", r3.get()->getProp7().get());
    r3 = r2.get()->resolveClass3(17, "test");
    BOOST_CHECK(r3);
    BOOST_CHECK_EQUAL(17, r3.get()->getProp6().get());
    BOOST_CHECK_EQUAL("test", r3.get()->getProp7().get());

    std::vector<OF_SHARED_PTR<testmodel::class3> > out3;
    r2->get()->resolveClass3(out3);
    BOOST_CHECK_EQUAL(1, out3.size());
    BOOST_CHECK_EQUAL(17, out3[0]->getProp6().get());
    BOOST_CHECK_EQUAL("test", out3[0]->getProp7().get());

    optional<OF_SHARED_PTR<testmodel::class4> > r4 =
        r1.get()->resolveClass4("class4name");
    optional<OF_SHARED_PTR<testmodel::class5> > r5 =
        r1.get()->resolveClass5("class5name");
    BOOST_CHECK(r4);
    BOOST_CHECK(r5);

    reference_t u = r5.get()->getClass4Ref(0);
    BOOST_CHECK_EQUAL(r4.get()->getURI().toString(),
                      r5.get()->getClass4Ref(0).second.toString());


    //Mutator mutator3(framework, "owner1");
    //r2.remove();
    //mutator3.commit();

    Mutator mutator4(framework, "owner2");
    testmodel::class4::remove(framework, uri3);
    mutator4.commit();

    //out2 = class2::resolve(uri2);
    //BOOST_CHECK(!out2);
    r4 = testmodel::class4::resolve(framework, "class4Name");
    BOOST_CHECK(!r4);

    testmodel::class1::unregisterListener(framework, &listener);
    testmodel::class2::unregisterListener(framework, &listener);

}

BOOST_AUTO_TEST_SUITE_END()
