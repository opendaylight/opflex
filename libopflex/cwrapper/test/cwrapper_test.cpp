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

#include <iostream>
#include <boost/test/unit_test.hpp>

#include "opflex/c/offramework_c.h"
#include "opflex/c/ofloghandler_c.h"

#include "MDFixture.h"

using opflex::modb::MDFixture;

BOOST_AUTO_TEST_SUITE(cwrapper_test)

void handler(const char* file, int line, 
             const char* function, int level, 
             const char* message) {
    //std::cout << message << std::endl;
}

BOOST_FIXTURE_TEST_CASE( init, MDFixture ) {
    ofloghandler_register(LOG_DEBUG, handler);

    offramework_p framework = NULL;
    BOOST_CHECK(OF_IS_SUCCESS(offramework_create(&framework)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_set_model(framework, &md)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_start(framework)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_stop(framework)));
    BOOST_CHECK(OF_IS_SUCCESS(offramework_destroy(&framework)));
}

BOOST_AUTO_TEST_SUITE_END()
