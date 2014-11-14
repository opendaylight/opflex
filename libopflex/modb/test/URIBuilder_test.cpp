/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for URI Builder.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include "opflex/modb/URIBuilder.h"

BOOST_AUTO_TEST_SUITE(URIBuilder_test)

using namespace opflex;
using namespace modb;

BOOST_AUTO_TEST_CASE( ints ) {
    URIBuilder builder;
    BOOST_CHECK_EQUAL("/", builder.build().toString());
    builder
        .addElement("prop1")
        .addElement((uint64_t)75)
        .addElement((int64_t)-75);
    BOOST_CHECK_EQUAL("/prop1/75/-75/", builder.build().toString());

}

BOOST_AUTO_TEST_CASE( string ) {
    {
        URIBuilder builder;
        builder
            .addElement("prop3")
            .addElement("test");
        BOOST_CHECK_EQUAL("/prop3/test/", builder.build().toString());
    }
    {
        URIBuilder builder;
        builder
            .addElement("prop3")
            .addElement("t est")
            .addElement("prop4")
            .addElement("t\"est");
        BOOST_CHECK_EQUAL("/prop3/t%20est/prop4/t%22est/", 
                          builder.build().toString());
    }
    {
        URIBuilder builder;
        builder
            .addElement("prop3")
            .addElement(".-~_");
        BOOST_CHECK_EQUAL("/prop3/.-~_/", builder.build().toString());
    }
    {
        URIBuilder builder;
        builder
            .addElement("prop3")
            .addElement(",./<>?;':\"[]\\{}|~!@#$%^&*()_-+=/");
        BOOST_CHECK_EQUAL("/prop3/%2c.%2f%3c%3e%3f%3b%27%3a%22%5b%5d%5c%7b"
                          "%7d%7c~%21%40%23%24%25%5e%26%2a%28%29_-%2b%3d%2f/",
                          builder.build().toString());
    }
}

BOOST_AUTO_TEST_CASE( mac ) {
    URIBuilder builder;
    builder
        .addElement("macprop")
        .addElement(MAC("11:22:33:44:55:66"));
    BOOST_CHECK_EQUAL("/macprop/11%3a22%3a33%3a44%3a55%3a66/",
                      builder.build().toString());
}

BOOST_AUTO_TEST_CASE( uri ) {
    URIBuilder builder;
    builder
        .addElement("uriprop")
        .addElement(URI("/macprop/11%3a22%3a33%3a44%3a55%3a66/"));
    BOOST_CHECK_EQUAL("/uriprop/%2fmacprop%2f11%253a22%253a33%253a44%253a55%253a66%2f/",
                      builder.build().toString());
}

BOOST_AUTO_TEST_CASE( getelements ) {
    URI u1("/");
    std::vector<std::string> elements;
    u1.getElements(elements);
    BOOST_CHECK_EQUAL(0, elements.size());

    URI u2("/class1/42");
    elements.clear();
    u2.getElements(elements);
    BOOST_CHECK_EQUAL(2, elements.size());
    BOOST_CHECK_EQUAL("class1", elements.at(0));
    BOOST_CHECK_EQUAL("42", elements.at(1));

    URI u3("/cla%2fss1/4%2A2%20/");
    elements.clear();
    u3.getElements(elements);
    BOOST_CHECK_EQUAL(2, elements.size());
    BOOST_CHECK_EQUAL("cla/ss1", elements.at(0));
    BOOST_CHECK_EQUAL("4*2 ", elements.at(1));

    URI u4("/class1%/42%2");
    elements.clear();
    u4.getElements(elements);
    BOOST_CHECK_EQUAL(2, elements.size());
    BOOST_CHECK_EQUAL("class1", elements.at(0));
    BOOST_CHECK_EQUAL("42", elements.at(1));

    URI u5("class1///42/class2/-42/test//");
    elements.clear();
    u5.getElements(elements);
    BOOST_CHECK_EQUAL(5, elements.size());
    BOOST_CHECK_EQUAL("class1", elements.at(0));
    BOOST_CHECK_EQUAL("42", elements.at(1));
    BOOST_CHECK_EQUAL("class2", elements.at(2));
    BOOST_CHECK_EQUAL("-42", elements.at(3));
    BOOST_CHECK_EQUAL("test", elements.at(4));

    URI u6("/prop3/%2c.%2f%3c%3e%3f%3b%27%3a%22%5b%5d%5c%7b"
           "%7d%7c~%21%40%23%24%25%5e%26%2a%28%29_-%2b%3d%2f/");
    elements.clear();
    u6.getElements(elements);
    BOOST_CHECK_EQUAL(2, elements.size());
    BOOST_CHECK_EQUAL("prop3", elements.at(0));
    BOOST_CHECK_EQUAL(",./<>?;':\"[]\\{}|~!@#$%^&*()_-+=/", elements.at(1));
}

BOOST_AUTO_TEST_SUITE_END()
