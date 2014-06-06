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
#include "config-file.h"
#include "uri-parser.h"
#include "dbug.h"


#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

static node_ele_t *node1;
static int modb_insert_one(node_ele_p *ndp, const char *uri_str, result_p rs);
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
    debug_level = conf_get_value("global", "debug_level");
    /* if (strcasecmp(debug_level, "OFF")) { */
    /*     DBUG_PUSH("d:t:i:L:n:P:T:0"); */
    /* } */
}
/*----------------------------------------------------------------------------
 * unittests.
 */
static void test_modb_initialize(void **state)
{
    (void) state;
    assert_false(modb_initialize());
    modb_cleanup();
}

static void test_modb_get_state(void **state)
{
    (void) state;
    assert_false(modb_initialize());
    assert_int_equal(modb_get_state(), HD_ST_INITIALIZED);
    modb_cleanup();
}

static int modb_insert_one(node_ele_p *ndp, const char *uri_str, result_p rs)
{
    *ndp = xzalloc(sizeof(node_ele_t));
    pag_rwlock_init(&(*ndp)->rwlock);
    (*ndp)->class_id = 123456789;
    (*ndp)->content_path_id = 2;
    (*ndp)->lri = "fake_class:fkae_type";
    assert_false(parse_uri(&(*ndp)->uri, uri_str));
    (*ndp)->parent = NULL;
    (*ndp)->child_list = NULL;
    (*ndp)->properties_list = NULL;
    assert_false(modb_op(OP_INSERT, (void *)(*ndp), IT_NODE, 1, EXT_NODE, rs));
    return(0);
}    

static void test_modb_insert(void **state)
{
    result_t rs;

    assert_false(modb_initialize());
    modb_insert_one(&node1, uri_str, &rs);
    modb_dump(true);
    modb_cleanup();
    free(node1);
}

static void test_modb_delete_with_node(void **state)
{
    result_t rs;
    (void) state;

    assert_false(modb_initialize());
    modb_insert_one(&node1, uri_str, &rs);    
    modb_dump(true);
    assert_false(modb_op(OP_DELETE, (void *)node1, IT_NODE, 1, EXT_NODE_AND_ATTR, &rs));
    modb_dump(true);
    modb_cleanup();
}

static void test_modb_delete_with_node_id(void **state)
{
    result_t rs;
    uint32_t node_id;
    (void) state;

    assert_false(modb_initialize());
    modb_insert_one(&node1, uri_str, &rs);    
    modb_dump(true);
    node_id = node1->id;
    assert_false(modb_op(OP_DELETE, (void *)node_id, IT_NODE_INDEX, 1, EXT_NODE, &rs));
    modb_dump(true);
    modb_cleanup();
}

static void test_modb_create_node_many(void **state)
{
#define MANY_NODES 1000
    node_ele_p nparr[MANY_NODES];
    node_ele_p ndp;
    int i;
    result_t rs;
    char tbuf[256] = {0};
    int row;

    (void) state;

    /* if (strcasecmp(debug_level, "OFF")) { */
    /*     DBUG_PUSH("d:t:i:L:n:P:T:0"); */
    /* } */
    assert_false(modb_initialize());

    /* create each node and insert it */
    for (i = 0; i < MANY_NODES; i++) {
        ndp = xzalloc(sizeof(node_ele_t));
        pag_rwlock_init(&ndp->rwlock);
        ndp->class_id = i;
        ndp->content_path_id = i;
        sprintf(tbuf, "fake_class:fake_type_%d", i);
        ndp->lri = strdup((const char *)&tbuf);
        sprintf(tbuf, "http://en.wikipedia-%d.org/wiki/Uniform_Resource_Identifier", i);
        assert_false(parse_uri(&ndp->uri, &tbuf));
        ndp->parent = NULL;
        ndp->child_list = NULL;
        ndp->properties_list = NULL;
        assert_false(modb_op(OP_INSERT, (void *)ndp, IT_NODE, 1, EXT_NODE, &rs));
        *(nparr+i) = ndp;
    }

    modb_dump(true);
    modb_cleanup();
}

static void test_modb_recovery(void **state)
{
    char *modb_fname = "./modb.dat";

    (void) state;
    modb_crash_recovery(modb_fname);
}


/* ---------------------------------------------------------------------------
 * Test driver.
 */
int main(int argc, char *argv[])
{
    int retc; 

    const UnitTest tests[] = {
        unit_test(test_modb_initialize),
        unit_test(test_modb_get_state),
        unit_test(test_modb_insert),
        unit_test(test_modb_delete_with_node),
        unit_test(test_modb_create_node_many),
    };

    test_setup();
    retc = run_tests(tests);
    return retc;
}
