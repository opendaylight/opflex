/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include "modb.h"
#include "modb-event.h"
#include "config-file.h"
#include "uri-parser.h"
#include "vlog.h"


#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

static node_ele_t *node1;
static int modb_insert_one(node_ele_p *ndp, const char *uri_str, result_p rs);
static void create_node_tree(node_ele_p *parentp, int count);


static const char *uri_str = 
    "http://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Naming.2C_addressing.2C_and_identifying_resources";
static const char *debug_level;

/* ---------------------------------------------------------------------------
 * test setup and other items.
 */
static void test_setup(void) 
{
    char *test_conf_fname = "./test-policy-agent.conf";

    conf_initialize(NULL);
    if (!conf_file_isloaded()) {
        conf_load(test_conf_fname);
    }
    conf_dump(stdout);
    assert_false(modb_initialize());

}

static void test_teardown(void) 
{
    modb_cleanup();
}

/*----------------------------------------------------------------------------
 * unittests.
 */
static void test_modb_event_initialize(void **state)
{
    (void) state;
    assert_false(modb_event_initialize());
    assert_false(modb_event_destroy());
}

static void test_modb_event_subscribe(void **state)
{
    (void) state;

    assert_false(modb_event_initialize());
    assert_return_code(modb_event_subscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    modb_event_dump();
    assert_return_code(modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    assert_false(modb_event_destroy());
    modb_cleanup();
}

static void test_modb_event_subscribe_no_unsubscribe(void **state)
{
    (void) state;

    assert_false(modb_event_initialize());
    assert_return_code(modb_event_subscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    modb_event_dump();
    assert_false(modb_event_destroy());
    modb_cleanup();
}

/* ---------------------------------------------------------------------------
 * Test driver.
 */
int main(int argc, char *argv[])
{
    int retc; 

    const UnitTest tests[] = {
        unit_test(test_modb_event_initialize),
        unit_test(test_modb_event_subscribe),
        unit_test(test_modb_event_subscribe_no_unsubscribe),
    };

    test_setup();
    retc = run_tests(tests);
    test_teardown();
    return retc;
}
