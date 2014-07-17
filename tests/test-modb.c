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
    debug_level = conf_get_value("global", "debug_level");
    
    if (strcasecmp(debug_level, "OFF")) {
        vlog_set_levels_from_string("WARN");
    }
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
    ovs_rwlock_init(&(*ndp)->rwlock);
    (*ndp)->class_id = 123456789;
    (*ndp)->content_path_id = 2;
    (*ndp)->lri = "fake_class:fkae_type";
    assert_false(parse_uri(&(*ndp)->uri, uri_str));
    (*ndp)->parent = NULL;
    (*ndp)->child_list = NULL;
    (*ndp)->properties_list = NULL;
    assert_false(modb_op(OP_INSERT, OP_SRC_INTERNAL, (void *)(*ndp),
                         IT_NODE, 1, EXT_NODE, rs));
    return(0);
}    

static void test_node_create(void **state)
{
    node_ele_p ndp;
    const char *lri_str = "fake_class:fake_type";
    int content_path_id = 2;

    (void) state;

    assert_false(node_create(&ndp, uri_str, lri_str, content_path_id));
    parsed_uri_free(&ndp->uri);
    free(ndp->lri);
    free(ndp);
}

static void test_attr_create(void **state)
{
    node_ele_p ndp;
    const char *lri_str = "fake_class:fake_type";
    int content_path_id = 2;
    char *field_name = "test_attr";
    char *field_data = "1234567";
    attribute_p ap;
    char *log_level_save = vlog_get_levels();

    (void) state;

    /* if (strcasecmp(debug_level, "OFF")) { */
    /*     vlog_set_levels_from_string("DBG"); */
    /* } */

    assert_false(node_create(&ndp, uri_str, lri_str, content_path_id));
    assert_false(attr_create(&ap, field_name, AT_STRING, field_data, strlen(field_data), ndp));
    assert_false(attr_delete(ndp, field_name));

    parsed_uri_free(&ndp->uri);
    free(ndp->lri);
    free(ndp);
    vlog_set_levels_from_string(log_level_save);
}

static void test_attr_same_create(void **state)
{
    node_ele_p ndp;
    const char *lri_str = "fake_class:fake_type";
    int content_path_id = 2;
    char *field_name = "test_attr";
    char *field_data = "1234567";
    attribute_p ap, ap1;
    char *log_level_save = vlog_get_levels();

    (void) state;

    if (strcasecmp(debug_level, "OFF")) {
        vlog_set_levels_from_string("INFO");
    }

    assert_false(node_create(&ndp, uri_str, lri_str, content_path_id));
    assert_false(attr_create(&ap, field_name, AT_STRING, field_data, strlen(field_data), ndp));
    assert_false(attr_create(&ap1, field_name, AT_STRING, field_data, strlen(field_data), ndp));
    node_dump(ndp);

    assert_false(attr_delete(ndp, field_name));
    node_dump(ndp);

    parsed_uri_free(&ndp->uri);
    free(ndp->lri);
    free(ndp);
    vlog_set_levels_from_string(log_level_save);
}

static void test_attr_same_name_data_diff(void **state)
{
    node_ele_p ndp;
    const char *lri_str = "fake_class:fake_type";
    int content_path_id = 2;
    const char *field_name1 = "test_attr";
    const char *field_name2 = "test different name same attr value";
    char *field_data = "1234567";
    attribute_p ap, ap1;
    char *log_level_save = vlog_get_levels();

    (void) state;

    if (strcasecmp(debug_level, "OFF")) {
        vlog_set_levels_from_string("INFO");
    }

    assert_false(node_create(&ndp, uri_str, lri_str, content_path_id));
    assert_false(attr_create(&ap, field_name1, AT_STRING, field_data, strlen(field_data), ndp));
    assert_false(attr_create(&ap1, field_name2, AT_STRING, field_data, strlen(field_data), ndp));
    node_dump(ndp);
    assert_false(attr_delete(ndp, field_name1));
    assert_false(attr_delete(ndp, field_name2));
    node_dump(ndp);

    parsed_uri_free(&ndp->uri);
    free(ndp->lri);
    free(ndp);
    vlog_set_levels_from_string(log_level_save);
}

static void test_node_create_delete1(void **state)
{
    node_ele_p ndp;
    const char *lri_str = "fake_class:fake_type";
    int content_path_id = 2;
    int count;

    (void) state;

    assert_false(node_create(&ndp, uri_str, lri_str, content_path_id));
    assert_false(node_delete(&ndp, &count));
}

static void test_node_create_del_with_children(void **state)
{
#define NUM_OF_CHILDREN 10
    node_ele_p parent;
    node_ele_p child[NUM_OF_CHILDREN] = {NULL};
    const char *lri_str = "fake_class:fake_type";
    char lri_buf[256] = {0};
    char uri_buf[256] = {0};
    int content_path_id = 999999999;
    int count, i;

    (void) state;

    assert_false(node_create(&parent, uri_str, &lri_str, content_path_id));
    for (i=0; i < NUM_OF_CHILDREN; i++) {
        memset(lri_buf, 0, sizeof(lri_buf));
        sprintf(lri_buf, "fake_class:fake_type_%d", i);
        memset(uri_buf, 0, sizeof(uri_buf));
        sprintf(uri_buf, "http://en.wikipedia-%d.org/wiki/Uniform_Resource_Identifier", i);
        content_path_id = i;
        assert_false(node_create(&child[i], uri_str, lri_str, content_path_id));
        assert_false(node_attach_child(parent, child[i]));
    }
    assert_false(node_delete(&parent, &count));
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
    assert_false(modb_op(OP_DELETE, OP_SRC_INTERNAL, (void *)node1, IT_NODE, 1,
                         EXT_NODE_AND_ATTR, &rs));
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
    assert_false(modb_op(OP_DELETE, OP_SRC_INTERNAL, (void *)&node_id,
                         IT_NODE_INDEX, 1, EXT_NODE, &rs));
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

    (void) state;

    assert_false(modb_initialize());

    /* create each node and insert it */
    for (i = 0; i < MANY_NODES; i++) {
        ndp = xzalloc(sizeof(node_ele_t));
        ovs_rwlock_init(&ndp->rwlock);
        ndp->class_id = i;
        ndp->content_path_id = i;
        sprintf(tbuf, "fake_class:fake_type_%d", i);
        ndp->lri = strdup((const char *)&tbuf);
        sprintf(tbuf, "http://en.wikipedia-%d.org/wiki/Uniform_Resource_Identifier", i);
        assert_false(parse_uri(&ndp->uri, (const char *)&tbuf));
        ndp->parent = NULL;
        ndp->child_list = NULL;
        ndp->properties_list = NULL;
        assert_false(modb_op(OP_INSERT, OP_SRC_INTERNAL, (void *)ndp,
                             IT_NODE, 1, EXT_NODE, &rs));
        *(nparr+i) = ndp;
    }

    modb_dump(true);
    modb_cleanup();
}

static void create_node_tree(node_ele_p *parentp, int count)
{
    node_ele_p ndp;
    char tbuf[256] = {0};
    node_ele_p parent;
    int nc = 0, i;
    int create_count = count;

    /* parent */
    nc = 1;
    parent = xzalloc(sizeof(node_ele_t));
    ovs_rwlock_init(&parent->rwlock);
    parent->class_id = nc;
    parent->content_path_id = nc;
    sprintf(tbuf, "fake_class:fake_type_%d", nc);
    parent->lri = strdup((const char *)&tbuf);
    sprintf(tbuf, "http://en.wikipedia-%d.org/wiki/Uniform_Resource_Identifier", nc);
    assert_false(parse_uri(&parent->uri, (const char *)&tbuf));
    head_list_create(&parent->child_list);
    parent->parent = NULL;

    /* children */
    
    for (i=0, nc=2; nc < create_count; nc++, i++) {
        ndp = xzalloc(sizeof(node_ele_t));
        ovs_rwlock_init(&ndp->rwlock);
        ndp->class_id = nc;
        ndp->content_path_id = nc;
        sprintf(tbuf, "fake_class:fake_type_%d", nc);
        ndp->lri = strdup((const char *)&tbuf);
        sprintf(tbuf, "http://en.wikipedia.org/wiki/node_%d", nc);
        assert_false(parse_uri(&ndp->uri, (char *)&tbuf));
        ndp->parent = parent;
        ndp->child_list = NULL;
        ndp->properties_list = NULL;

        list_add(&parent->child_list->list, -1, (void *)ndp);
        parent->child_list->num_elements++;
    }
    *parentp = parent;
}

static void test_modb_create_node_tree(void **state)
{
    result_t rs;
    node_ele_p parent;

    (void) state;

    assert_false(modb_initialize());
    create_node_tree(&parent, 5);
    assert_false(modb_op(OP_INSERT, OP_SRC_INTERNAL, (void *)parent,
                         IT_NODE, 1, EXT_NODE, &rs));
    modb_dump(true);
    modb_cleanup();
}

static void test_modb_delete_node_tree(void **state)
{
    result_t rs;
    node_ele_p parent;

    (void) state;
    
    assert_false(modb_initialize());

    /* create the tree */
    create_node_tree(&parent, 5);
    assert_false(modb_op(OP_INSERT, OP_SRC_INTERNAL, (void *)parent,
                         IT_NODE, 1, EXT_NODE, &rs));
    modb_dump(true);

    /* delete it. */
    assert_false(modb_op(OP_DELETE, OP_SRC_INTERNAL, (void *)parent,
                         IT_NODE, 1, EXT_FULL, &rs));
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
        unit_test(test_node_create_delete1),
        unit_test(test_node_create_del_with_children),
        unit_test(test_modb_get_state),
        unit_test(test_modb_insert),
        unit_test(test_modb_delete_with_node),
        unit_test(test_modb_create_node_many),
        unit_test(test_modb_create_node_tree),
        unit_test(test_modb_delete_node_tree),
        unit_test(test_attr_create),
        unit_test(test_attr_same_create),
        unit_test(test_attr_same_name_data_diff),
    };

    test_setup();
    retc = run_tests(tests);
    return retc;
}
