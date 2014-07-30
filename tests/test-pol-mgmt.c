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

#include "pol-mgmt.h"
#include "sess.h"
#include "config-file.h"
#include "uri-parser.h"
#include "vlog.h"


#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif


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
}

/*----------------------------------------------------------------------------
 * unittests.
 */
static void test_pm_initialize(void **state)
{
    (void) state;
    assert_false(pm_initialize());
    pm_cleanup();
}


/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Session tests
 */
static void test_sess_open(void **state)
{
    char *default_controller;
    (void) state;
    assert_false(pm_initialize());

    /* the following assumes that there is a simulator running at
     * the default _controller
     */
    default_controller = conf_get_value(PM_SECTION, "default_controller");
    assert_false(sess_open(default_controller, SESS_TYPE_CLIENT));
    assert_return_code(sess_get_session_count(), 1);
    assert_false(sess_close(default_controller));
    pm_cleanup();
}


/* ---------------------------------------------------------------------------
 * Test driver.
 */
int main(int argc, char *argv[])
{
    int retc; 

    const UnitTest tests[] = {
        unit_test(test_pm_initialize),
        unit_test(test_sess_open),
    };

    test_setup();
    retc = run_tests(tests);
    return retc;
}
