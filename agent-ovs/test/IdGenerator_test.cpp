/*
 * Test suite for class IdGenerator.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <cstdio>
#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include "IdGenerator.h"

using namespace std;
using namespace boost;
using namespace ovsagent;
using namespace opflex::modb;

BOOST_AUTO_TEST_SUITE(IdGenerator_test)

BOOST_AUTO_TEST_CASE(get_erase) {
    string dir(".");
    string nmspc("idtest");
    URI u1("/uri/one");
    URI u2("/uri/two");
    uint32_t u1_id, u2_id;

    {
        IdGenerator idgen;
        idgen.setPersistLocation(dir);
        idgen.initNamespace(nmspc);
        u1_id = idgen.getId(nmspc, u1);
        u2_id = idgen.getId(nmspc, u2);
        BOOST_CHECK(u1_id != 0);
        BOOST_CHECK(u2_id != 0);
        BOOST_CHECK(u1_id == idgen.getId(nmspc, u1));
    }

    {
        IdGenerator idgen1(boost::chrono::milliseconds(15));
        idgen1.setPersistLocation(dir);
        idgen1.initNamespace(nmspc);
        BOOST_CHECK(u1_id == idgen1.getId(nmspc, u1));
        BOOST_CHECK(u2_id == idgen1.getId(nmspc, u2));

        // erase but resurrect before cleanup
        idgen1.erase(nmspc, u1);
        uint32_t u1_id_1 = idgen1.getId(nmspc, u1);
        BOOST_CHECK(u1_id == u1_id_1);
        boost::this_thread::sleep(boost::posix_time::milliseconds(20));
        idgen1.cleanup();

        u1_id_1 = idgen1.getId(nmspc, u1);
        BOOST_CHECK(u1_id == u1_id_1);

        // erase and allow to die
        idgen1.erase(nmspc, u1);
        boost::this_thread::sleep(boost::posix_time::milliseconds(20));
        idgen1.cleanup();

        u1_id_1 = idgen1.getId(nmspc, u1);
        BOOST_CHECK(u1_id_1 != 0);
        BOOST_CHECK(u1_id != u1_id_1);

        remove(idgen1.getNamespaceFile(nmspc).c_str());
    }

    {
        IdGenerator idgen2;
        BOOST_CHECK_EQUAL(idgen2.getId(nmspc, u1), 0);
    }
}

BOOST_AUTO_TEST_SUITE_END()
