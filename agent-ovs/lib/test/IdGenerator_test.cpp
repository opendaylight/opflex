/*
 * Test suite for class IdGenerator.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "IdGenerator.h"

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <sstream>
#include <thread>
#include <chrono>

using namespace std;
using namespace boost;
using namespace opflexagent;

static bool garbage_cb_false(const std::string&, const std::string&) {
    return false;
}
static bool garbage_cb_true(const std::string&, const std::string&) {
    return true;
}

BOOST_AUTO_TEST_SUITE(IdGenerator_test)

BOOST_AUTO_TEST_CASE(get_erase) {
    string dir(".");
    string nmspc("idtest");
    string u1("/uri/one");
    string u2("/uri/two");
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
        BOOST_CHECK_EQUAL(u1, idgen.getStringForId(nmspc, u1_id).get());
        BOOST_CHECK_EQUAL(u2, idgen.getStringForId(nmspc, u2_id).get());
    }

    {
        IdGenerator idgen1(std::chrono::milliseconds(15));
        idgen1.setPersistLocation(dir);
        idgen1.initNamespace(nmspc);
        BOOST_CHECK(u1_id == idgen1.getId(nmspc, u1));
        BOOST_CHECK(u2_id == idgen1.getId(nmspc, u2));

        // erase but resurrect before cleanup
        idgen1.erase(nmspc, u1);
        uint32_t u1_id_1 = idgen1.getId(nmspc, u1);
        BOOST_CHECK(u1_id == u1_id_1);
        BOOST_CHECK_EQUAL(u1, idgen1.getStringForId(nmspc, u1_id_1).get());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen1.cleanup();

        u1_id_1 = idgen1.getId(nmspc, u1);
        BOOST_CHECK(u1_id == u1_id_1);
        BOOST_CHECK_EQUAL(u1, idgen1.getStringForId(nmspc, u1_id_1).get());

        // erase and allow to die
        idgen1.erase(nmspc, u1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen1.cleanup();

        BOOST_CHECK(!idgen1.getStringForId(nmspc, u1_id_1));
        u1_id_1 = idgen1.getId(nmspc, u1);
        BOOST_CHECK(u1_id_1 != 0);

        remove(idgen1.getNamespaceFile(nmspc).c_str());
    }

    {
        IdGenerator idgen2;
        BOOST_CHECK_EQUAL(idgen2.getId(nmspc, u1), -1);
        BOOST_CHECK(!idgen2.getStringForId(nmspc, -1));
    }
}

BOOST_AUTO_TEST_CASE(garbage) {
    IdGenerator idgen(std::chrono::milliseconds(15));
    string nmspc("idtest");
    const string u1("/uri/one");
    const string u2("/uri/two");

    idgen.initNamespace(nmspc, 1, 15);
    BOOST_CHECK_EQUAL(1, idgen.getId(nmspc, u1));
    BOOST_CHECK_EQUAL(2, idgen.getId(nmspc, u2));

    idgen.collectGarbage(nmspc, garbage_cb_true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();

    BOOST_CHECK_EQUAL(1, idgen.getId(nmspc, u1));
    BOOST_CHECK_EQUAL(2, idgen.getId(nmspc, u2));
    BOOST_CHECK_EQUAL(13, idgen.getRemainingIds(nmspc));

    idgen.collectGarbage(nmspc, garbage_cb_false);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();

    BOOST_CHECK_EQUAL(15, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(1, idgen.getId(nmspc, u2));
    BOOST_CHECK_EQUAL(2, idgen.getId(nmspc, u1));
}

BOOST_AUTO_TEST_CASE(free_range) {
    IdGenerator idgen(std::chrono::milliseconds(15));
    string nmspc("idtest");
    idgen.initNamespace(nmspc, 1, 20);

    vector<string> uris;
    for (int i = 1; i <= 20; i++) {
        std::stringstream s;
        s << "/uri/" << i;
        uris.push_back(s.str());
    }

    BOOST_CHECK_EQUAL(20, idgen.getRemainingIds(nmspc));
    for (int i = 0; i < 20; i++) {
        BOOST_CHECK_EQUAL(1, idgen.getFreeRangeCount(nmspc));
        BOOST_CHECK_EQUAL(i+1, idgen.getId(nmspc, uris[i]));
    }
    BOOST_CHECK_EQUAL(0, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(0, idgen.getFreeRangeCount(nmspc));

    // add free to empty range
    idgen.erase(nmspc, uris[10]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(1, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(1, idgen.getFreeRangeCount(nmspc));

    // add isolated at start and end
    idgen.erase(nmspc, uris[6]);
    idgen.erase(nmspc, uris[14]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(3, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(3, idgen.getFreeRangeCount(nmspc));

    // merge to range above and below
    idgen.erase(nmspc, uris[7]);
    idgen.erase(nmspc, uris[13]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(5, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(3, idgen.getFreeRangeCount(nmspc));

    // merge to center range
    idgen.erase(nmspc, uris[9]);
    idgen.erase(nmspc, uris[11]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(7, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(3, idgen.getFreeRangeCount(nmspc));

    // merge all ranges into one range
    idgen.erase(nmspc, uris[8]);
    idgen.erase(nmspc, uris[12]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(9, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(1, idgen.getFreeRangeCount(nmspc));

    // add to beginning and end
    idgen.erase(nmspc, uris[0]);
    idgen.erase(nmspc, uris[19]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(11, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(3, idgen.getFreeRangeCount(nmspc));

    // add isolated between ranges
    idgen.erase(nmspc, uris[3]);
    idgen.erase(nmspc, uris[17]);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    idgen.cleanup();
    BOOST_CHECK_EQUAL(13, idgen.getRemainingIds(nmspc));
    BOOST_CHECK_EQUAL(5, idgen.getFreeRangeCount(nmspc));
}

BOOST_AUTO_TEST_CASE(persist_range) {
    string dir(".");
    string nmspc("idtest");

    vector<string> uris;
    for (int i = 1; i <= 20; i++) {
        std::stringstream s;
        s << "/uri/" << i;
        uris.push_back(s.str());
    }

    {
        IdGenerator idgen(std::chrono::milliseconds(15));
        idgen.setPersistLocation(dir);
        remove(idgen.getNamespaceFile(nmspc).c_str());
        idgen.initNamespace(nmspc, 1, 20);

        BOOST_CHECK_EQUAL(20, idgen.getRemainingIds(nmspc));
        for (int i = 0; i < 20; i++) {
            BOOST_CHECK_EQUAL(1, idgen.getFreeRangeCount(nmspc));
            BOOST_CHECK_EQUAL(i+1, idgen.getId(nmspc, uris[i]));
        }
        BOOST_CHECK_EQUAL(0, idgen.getRemainingIds(nmspc));
        BOOST_CHECK_EQUAL(0, idgen.getFreeRangeCount(nmspc));
    }

    {
        IdGenerator idgen(std::chrono::milliseconds(15));
        idgen.setPersistLocation(dir);
        idgen.initNamespace(nmspc, 1, 20);
        BOOST_CHECK_EQUAL(0, idgen.getRemainingIds(nmspc));
        BOOST_CHECK_EQUAL(0, idgen.getFreeRangeCount(nmspc));

        for (int i = 0; i < 20; i++) {
            BOOST_CHECK_EQUAL(uris[i], idgen.getStringForId(nmspc, i+1).get());
            BOOST_CHECK_EQUAL(i+1, idgen.getId(nmspc, uris[i]));
        }

        idgen.erase(nmspc, uris[10]);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen.cleanup();
    }

    {
        IdGenerator idgen(std::chrono::milliseconds(15));
        idgen.setPersistLocation(dir);
        idgen.initNamespace(nmspc, 1, 20);
        BOOST_CHECK_EQUAL(1, idgen.getRemainingIds(nmspc));
        BOOST_CHECK_EQUAL(1, idgen.getFreeRangeCount(nmspc));

        idgen.erase(nmspc, uris[5]);
        idgen.erase(nmspc, uris[15]);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen.cleanup();
    }

    {
        IdGenerator idgen(std::chrono::milliseconds(15));
        idgen.setPersistLocation(dir);
        idgen.initNamespace(nmspc, 1, 20);
        BOOST_CHECK_EQUAL(3, idgen.getRemainingIds(nmspc));
        BOOST_CHECK_EQUAL(3, idgen.getFreeRangeCount(nmspc));

        idgen.erase(nmspc, uris[4]);
        idgen.erase(nmspc, uris[9]);
        idgen.erase(nmspc, uris[11]);
        idgen.erase(nmspc, uris[14]);
        idgen.erase(nmspc, uris[16]);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen.cleanup();
    }

    {
        IdGenerator idgen(std::chrono::milliseconds(15));
        idgen.setPersistLocation(dir);
        idgen.initNamespace(nmspc, 1, 20);
        BOOST_CHECK_EQUAL(8, idgen.getRemainingIds(nmspc));
        BOOST_CHECK_EQUAL(3, idgen.getFreeRangeCount(nmspc));

        for (int i = 0; i < 20; i++) {
            idgen.erase(nmspc, uris[i]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen.cleanup();

        for (int i = 0; i < 20; i++) {
            idgen.getId(nmspc, uris[i]);
        }
        for (int i = 0; i < 10; i++) {
            idgen.erase(nmspc, uris[i]);
        }
        for (int i = 11; i < 20; i++) {
            idgen.erase(nmspc, uris[i]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        idgen.cleanup();
    }

    {
        IdGenerator idgen(std::chrono::milliseconds(15));
        idgen.setPersistLocation(dir);
        idgen.initNamespace(nmspc, 1, 20);
        BOOST_CHECK_EQUAL(19, idgen.getRemainingIds(nmspc));
        BOOST_CHECK_EQUAL(2, idgen.getFreeRangeCount(nmspc));

        remove(idgen.getNamespaceFile(nmspc).c_str());
    }

}

BOOST_AUTO_TEST_SUITE_END()
