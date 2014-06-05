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
#include <errno.h>

#include "pag_selector.h"


#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

struct selector_args {
    int n_selectors;
    struct pag_selector **selectors;
    int n_keys;
    struct pag_selector_key **keys;
    struct pag_selector_key *first_key;
};

static int trigger = 0;

/*
 * Forward declarations
 */
static void * trigger_add(void * arg);
static int add_fd(struct pollfd *fd, void *arg);
static int is_ready(void *arg);
static void * do_work(void *arg);
static int add_trigger_fd(struct pollfd *fds, void *args);

static void test_pag_selector_create(void **state);
static void test_pag_selector_create_teardown(void **state);
static void test_pag_selector_destroy(void **state);
static void test_pag_selector_destroy_setup(void **state);
static void test_pag_selector_key_create(void **state);
static void test_pag_selector_key_create_teardown(void **state);
static void test_pag_selector_key_destroy(void **state);
static void test_pag_selector_key_destroy_setup(void **state);
static void test_pag_selector_register(void **state);
static void test_pag_selector_register_setup(void **state);
static void test_pag_selector_register_teardown(void **state);
static void test_pag_selector_unregister(void **state);
static void test_pag_selector_unregister_setup(void **state);
static void test_pag_selector_unregister_teardown(void **state);
static void test_pag_selector_get_keyset(void **state);
static void test_pag_selector_get_keyset_setup(void **state);
static void test_pag_selector_get_keyset_teardown(void **state);
static void test_pag_selector_first_key(void **state);
static void test_pag_selector_first_key_setup(void **state);
static void test_pag_selector_first_key_teardown(void **state);
static void test_pag_selector_next_key(void **state);
static void test_pag_selector_next_key_setup(void **state);
static void test_pag_selector_next_key_teardown(void **state);
static void test_pag_selector_select(void **state);
static void test_pag_selector_select_setup(void **state);
static void test_pag_selector_select_teardown(void **state);
static void test_pag_selector_key_is_ready(void **state);
static void test_pag_selector_key_is_ready_setup(void **state);
static void test_pag_selector_key_is_ready_teardown(void **state);
static void test_pag_selector_key_do_work(void **state);
static void test_pag_selector_key_do_work_setup(void **state);
static void test_pag_selector_key_do_work_teardown(void **state);
static void test_pag_selector_create_1(void **state);
static void test_pag_selector_create_n(void **state);
static void test_pag_selector_key_create_1(void **state);
static void test_pag_selector_key_create_n(void **state);
static void test_pag_selectors_with_n_keys(void **state);

/*
 * Helpers
 */
static void *
trigger_add(void * arg)
{
    trigger += 1;
    return 0;
}

static int
add_fd(struct pollfd *fd, void *arg)
{
    assert_true(arg == &trigger);
    trigger_add(0);
    return 0;
}

static int
is_ready(void *arg)
{
    trigger_add(0);
    return true;
}

static void *
do_work(void *arg)
{
    assert_true(arg == &trigger);
    trigger_add(0);
    return 0;
}

static int
add_trigger_fd(struct pollfd *fds, void *args)
{
    assert_true(fds != 0);
    assert_true(args == 0);
    trigger += 1;

    return 0;
}

/**********
 * Tests
 *********/

/*****
 * Unit Test: pag_selector_create
 *****/
static void
test_pag_selector_create(void **state)
{
    struct pag_selector *selector;
    int error;

    error = pag_selector_create(trigger_add, &selector);
    assert_true(error == 0);

    *state = selector;
}

static void
test_pag_selector_create_teardown(void **state)
{
    struct pag_selector *selector = (struct pag_selector *)(*state);
    pag_selector_destroy(selector);
}

/*****
 * Unit Test: pag_selector_destroy
 *****/
static void
test_pag_selector_destroy(void **state)
{
    struct pag_selector *selector = (struct pag_selector *)(*state);
    pag_selector_destroy(selector);
}

static void
test_pag_selector_destroy_setup(void **state)
{
    struct pag_selector *selector;
    int error;

    error = pag_selector_create(trigger_add, &selector);
    assert_true(error == 0);

    *state = selector;
}

/*****
 * Unit Test: pag_selector_key_create
 *****/
static void
test_pag_selector_key_create(void **state)
{
    struct pag_selector_key *key;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;

    /* Create a selection key for this session */
    error = pag_selector_key_create(&funcs, 0, &key);
    assert_true(error == 0);

    *state = key;
}

static void
test_pag_selector_key_create_teardown(void **state)
{
    struct pag_selector_key *key = (struct pag_selector_key *)(*state);
    pag_selector_key_destroy(key);
}

/*****
 * Unit Test: pag_selector_key_destroy
 *****/
test_pag_selector_key_destroy(void **state)
{
    struct pag_selector_key *key = (struct pag_selector_key *)(*state);
    pag_selector_key_destroy(key);
}

static void
test_pag_selector_key_destroy_setup(void **state)
{
    struct pag_selector_key *key;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;

    /* Create a selection key for this session */
    error = pag_selector_key_create(&funcs, 0, &key);
    assert_true(error == 0);

    *state = key;
}

/*****
 * Unit Test: pag_selector_register
 *****/
static void
test_pag_selector_register(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    int error;

    error = pag_selector_register(args->selectors[0], args->keys[0]);
    assert_true(error == 0);
}

static void
test_pag_selector_register_setup(void **state)
{
    struct selector_args *args;
    int error;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create a selection key for this selector */
    args->keys = malloc(sizeof(*args->keys));
    assert_true(args->keys != 0);
    error = pag_selector_key_create(&funcs, 0, &args->keys[0]);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_selector_register_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    pag_selector_unregister(args->selectors[0], args->keys[0]);
    pag_selector_key_destroy(args->keys[0]);
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/*****
 * Unit Test: pag_selector_unregister
 *****/
static void
test_pag_selector_unregister(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    pag_selector_unregister(args->selectors[0], args->keys[0]);
}

static void
test_pag_selector_unregister_setup(void **state)
{
    struct selector_args *args;
    int error;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create a selection key for this selector */
    args->keys = malloc(sizeof(*args->keys));
    assert_true(args->keys != 0);
    error = pag_selector_key_create(&funcs, 0, &args->keys[0]);
    assert_true(error == 0);

    error = pag_selector_register(args->selectors[0], args->keys[0]);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_selector_unregister_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    pag_selector_key_destroy(args->keys[0]);
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/*****
 * Unit Test: pag_selector_get_keyset
 *****/
static void
test_pag_selector_get_keyset(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    int error;

    assert_true(trigger == 0);
    error = pag_selector_get_keyset(args->selectors[0]);
    assert_true(error == 0);
    assert_true(trigger == 1);
}

static void
test_pag_selector_get_keyset_setup(void **state)
{
    struct selector_args *args;
    struct pag_key_handlers funcs = {
        .add_fd   = add_trigger_fd,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;

    trigger = 0;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create a selection key for this selector */
    args->keys = malloc(sizeof(*args->keys));
    assert_true(args->keys != 0);
    error = pag_selector_key_create(&funcs, 0, &args->keys[0]);
    assert_true(error == 0);

    error = pag_selector_register(args->selectors[0], args->keys[0]);
    assert_true(error == 0);

    *state = args;

}

static void
test_pag_selector_get_keyset_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    pag_selector_unregister(args->selectors[0], args->keys[0]);
    pag_selector_key_destroy(args->keys[0]);
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/*****
 * Unit Test: pag_selector_first_key
 *****/
static void
test_pag_selector_first_key(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    struct pag_selector_key *key = 0;
    int error;

    error = pag_selector_first_key(args->selectors[0], &key);
    assert_true(error == 0);
    assert_true(key != 0);
}

static void
test_pag_selector_first_key_setup(void **state)
{
    struct selector_args *args;
    struct pag_key_handlers funcs = {
        .add_fd   = add_trigger_fd,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;

    trigger = 0;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create a selection key for this selector */
    args->keys = malloc(sizeof(*args->keys));
    assert_true(args->keys != 0);
    error = pag_selector_key_create(&funcs, 0, &args->keys[0]);
    assert_true(error == 0);

    error = pag_selector_register(args->selectors[0], args->keys[0]);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_selector_first_key_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    pag_selector_unregister(args->selectors[0], args->keys[0]);
    pag_selector_key_destroy(args->keys[0]);
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/*****
 * Unit Test: pag_selector_next_key
 *****/
static void
test_pag_selector_next_key(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    struct pag_selector_key *key;
    bool has_next;
    int i;

    key = args->first_key;
    for (i=0; i<args->n_keys-1; i++) {
        has_next = pag_selector_next_key(args->selectors[0], &key);
        assert_true(has_next == true);
    }
    has_next = pag_selector_next_key(args->selectors[0], &key);
    assert_true(has_next == false);
    assert_true(key == 0);
}

static void
test_pag_selector_next_key_setup(void **state)
{
    struct selector_args *args;
    struct pag_key_handlers funcs = {
        .add_fd   = add_trigger_fd,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;
    int i;

    trigger = 0;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create two selection keys for this selector */
    args->n_keys = 2;
    args->keys = malloc(args->n_keys*sizeof(*args->keys));
    assert_true(args->keys != 0);
    for (i=0; i< args->n_keys; i++) {
        error = pag_selector_key_create(&funcs, 0, &args->keys[i]);
        assert_true(error == 0);
        error = pag_selector_register(args->selectors[0], args->keys[i]);
        assert_true(error == 0);
    }

    error = pag_selector_first_key(args->selectors[0], &args->first_key);
    assert_true(error == 0);
    assert_true(args->first_key != 0);

    *state = args;
}

static void
test_pag_selector_next_key_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    int i;

    for (i=0; i< args->n_keys; i++) {
        pag_selector_unregister(args->selectors[0], args->keys[i]);
        pag_selector_key_destroy(args->keys[i]);
    }
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/*****
 * Unit Test: pag_selector_select
 *****/
static void
test_pag_selector_select(void **state)
{
    struct pag_selector *selector = (struct pag_selector *)(*state);

    assert_true(trigger == 0);
    pag_selector_select(selector);
    assert_true(trigger == 1);
}

static void
test_pag_selector_select_setup(void **state)
{
    struct pag_selector *selector;
    int error;

    trigger = 0;

    error = pag_selector_create(trigger_add, &selector);
    assert_true(error == 0);

    *state = selector;
}

static void
test_pag_selector_select_teardown(void **state)
{
    struct pag_selector *selector = (struct pag_selector *)(*state);

    pag_selector_destroy(selector);
}

/*****
 * Unit Test: pag_selector_key_is_ready
 *****/
static void
test_pag_selector_key_is_ready(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);
    bool is_ready;

    assert_true(trigger == 0);
    is_ready = pag_selector_key_is_ready(args->keys[0]);
    assert_true(is_ready == true);
    assert_true(trigger == 1);
}

static void
test_pag_selector_key_is_ready_setup(void **state)
{
    struct selector_args *args;
    int error;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = is_ready,
        .do_work  = 0,
        .work_arg = 0,
    };

    trigger = 0;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create a selection key for this selector */
    args->keys = malloc(sizeof(*args->keys));
    assert_true(args->keys != 0);
    error = pag_selector_key_create(&funcs, 0, &args->keys[0]);
    assert_true(error == 0);

    error = pag_selector_register(args->selectors[0], args->keys[0]);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_selector_key_is_ready_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    pag_selector_unregister(args->selectors[0], args->keys[0]);
    pag_selector_key_destroy(args->keys[0]);
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/*****
 * Unit Test: pag_selector_key_do_work
 *****/
static void
test_pag_selector_key_do_work(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    assert_true(trigger == 0);
    pag_selector_key_do_work(args->keys[0]);
    assert_true(trigger == 1);
}

static void
test_pag_selector_key_do_work_setup(void **state)
{
    struct selector_args *args;
    int error;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = do_work,
        .work_arg = &trigger,
    };

    trigger = 0;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    /* create the selector */
    args->selectors = malloc(sizeof(*args->selectors));
    assert_true(args->selectors != 0);
    error = pag_selector_create(trigger_add, &args->selectors[0]);
    assert_true(error == 0);

    /* Create a selection key for this selector */
    args->keys = malloc(sizeof(*args->keys));
    assert_true(args->keys != 0);
    error = pag_selector_key_create(&funcs, 0, &args->keys[0]);
    assert_true(error == 0);

    error = pag_selector_register(args->selectors[0], args->keys[0]);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_selector_key_do_work_teardown(void **state)
{
    struct selector_args *args = (struct selector_args *)(*state);

    pag_selector_unregister(args->selectors[0], args->keys[0]);
    pag_selector_key_destroy(args->keys[0]);
    pag_selector_destroy(args->selectors[0]);
    free(args->keys);
    free(args->selectors);
    free(args);
}

/***
 * Functional Test: Selector Basic
 *
 * Create a single selector, and invoke the select method
 ****/
static void
test_pag_selector_create_1(void **state)
{
    struct pag_selector *selector;
    int error;

    (void) state;

    /* start count with 0 */
    trigger = 0;

    /* Create a single selector */
    error = pag_selector_create(trigger_add,
                                &selector);

    assert_true(error == 0);
    pag_selector_select(selector);
    pag_selector_destroy(selector);

    assert_true(trigger == 1);
}

/***
 * Functional Test: Multi-Selector
 *
 * Create multiple selectors, and invoke the select method
 ****/
static void
test_pag_selector_create_n(void **state)
{
    struct pag_selector **selectors;
    int error;
    int i;
    int n_selectors = 10;

    (void) state;

    /* start count with 0 */
    trigger = 0;

    selectors = malloc(n_selectors*sizeof(*selectors));
    for (i=0; i<n_selectors; i++ ) {
        /* Create a single selector */
        error = pag_selector_create(trigger_add,
                                    &selectors[i]);
        assert_true(error == 0);
    }

    if (!error) {
        for (i=0; i<n_selectors; i++ ) {
            pag_selector_select(selectors[i]);
            pag_selector_destroy(selectors[i]);
        }
    }
    
    assert_true(trigger == n_selectors);
}

/***
 *  Functional Test: Selector Key Basic
 *
 * Create a single selector, and add a single selector key
 ****/
static void
test_pag_selector_key_create_1(void **state)
{
    struct pag_selector *selector;
    struct pag_selector_key *key;
    int error;

    (void) state;

    /* initialize trigger */
    trigger = 0;

    struct pag_key_handlers funcs = {
        .add_fd   = add_fd,
        .key_arg  = &trigger,
        .is_ready = is_ready,
        .do_work  = do_work,
        .work_arg = &trigger,
    };

    /* Create a selection key for this session */
    error = pag_selector_key_create(&funcs, 0, &key);


    /* Create a single selector */
    error = pag_selector_create(trigger_add,
                                &selector);
    assert_true(error == 0);

    /* Register the key against the selector */
    error = pag_selector_register(selector, key);
    assert_true(error == 0);

    /* add 1 to trigger */
    error = pag_selector_get_keyset(selector);
    assert_true(error == 0);

    /* add 1 to trigger */
    pag_selector_select(selector);

    pag_selector_first_key(selector, &key);
    while (!error && key) {
        assert_true(pag_selector_key_is_ready(key) == true);

        /* Invoke the callback registered on the key */
        pag_selector_key_do_work(key);
        pag_selector_next_key(selector, &key);
    }
    assert_true(trigger == 4);
}

/***
 * Functional Test: Multi Selector Key
 *
 * Create a single selector, and add multiple selector keys
 ****/
static void
test_pag_selector_key_create_n(void **state)
{
    struct pag_selector *selector;
    struct pag_selector_key *key;
    int n_keys = 10;
    int error;
    int i;

    struct pag_key_handlers funcs = {
        .add_fd   = add_fd,
        .key_arg  = &trigger,
        .is_ready = is_ready,
        .do_work  = do_work,
        .work_arg = &trigger,
    };

    (void) state;

    /* initialize trigger count */
    trigger = 0;


    /* Create a single selector */
    error = pag_selector_create(trigger_add,
                                &selector);
    assert_true(error == 0);

    /* 
     * Create selection keys for the selector, and
     * register them against the seleector
     */
    for (i=0; i<n_keys; i++ ) {
        error = pag_selector_key_create(&funcs, 0, &key);
        assert_true(error == 0);
        error = pag_selector_register(selector, key);
        assert_true(error == 0);
    }


    error = pag_selector_get_keyset(selector);
    assert_true(error == 0);

    /* increment trigger by 1 */
    pag_selector_select(selector);

    /* increment trigger by n_keys */
    error = pag_selector_first_key(selector, &key);
    assert_true(error == 0);

    while (!error && key) {
        /* increment trigger by n_keys */
        assert_true(pag_selector_key_is_ready(key));
        /* Invoke the callback registered on the key */

        /* increment trigger by n_keys */
        pag_selector_key_do_work(key);
        pag_selector_next_key(selector, &key);
    }
    assert_true(trigger == 1+(3*n_keys));
}

/***
 * Functional Test: Multi Selectors and Selector Keys
 *
 * Create multiples selectors, and add multiple selector keys
 ****/
static void
test_pag_selectors_with_n_keys(void **state)
{
    struct pag_selector **selectors;
    struct pag_selector_key *key, *iterator = 0;
    int n_selectors = 10, n_keys = 10;
    int error = 0;
    int i,j;

    struct pag_key_handlers funcs = {
        .add_fd   = add_fd,
        .key_arg  = &trigger,
        .is_ready = is_ready,
        .do_work  = do_work,
        .work_arg = &trigger,
    };

    (void) state;

    /* initialize trigger */
    trigger = 0;

    selectors = malloc(n_selectors*sizeof(*selectors));

    /* 
     * Create a selector, then create a pool of
     * selection keys for the selector, and
     * register them against the selector
     */
    for (i=0; i<n_selectors && !error; i++) {
        error = pag_selector_create(trigger_add,
                                    &selectors[i]);
        assert_true(error == 0);
        for (j=0; j<n_keys && !error; j++ ) {
            error = pag_selector_key_create(&funcs, 0, &key);
            assert_true(error == 0);
            error = pag_selector_register(selectors[i], key);
            assert_true(error == 0);
        }
    }

    for (i=0; i<n_selectors && !error; i++) {
        /* add n_keys to trigger */
        error = pag_selector_get_keyset(selectors[i]);
        assert_true(error == 0);

        /* add 1 to trigger */
        pag_selector_select(selectors[i]);

        error = pag_selector_first_key(selectors[i], &iterator);
        assert_true(error == 0);
        while (iterator) {
            /* add n_keys to trigger */
            assert_true(pag_selector_key_is_ready(iterator));

            /* Invoke the callback registered on the key */
            /* add n_keys to trigger */
            pag_selector_key_do_work(iterator);
            pag_selector_next_key(selectors[i], &iterator);
        }
    }
    assert_true(trigger == n_selectors*(1 + 3*n_keys));
}

/* ---------------------------------------------------------------------------
 * Test driver.
 */
int
main(int argc, char *argv[])
{
    int retc; 

    const UnitTest tests[] = {
        /*
         * unit tests
         */
        unit_test_teardown(test_pag_selector_create,
                           test_pag_selector_create_teardown),
        unit_test_setup(test_pag_selector_destroy,
                        test_pag_selector_destroy_setup),
        unit_test_teardown(test_pag_selector_key_create,
                           test_pag_selector_key_create_teardown),
        unit_test_setup(test_pag_selector_key_destroy,
                        test_pag_selector_key_destroy_setup),
        unit_test_setup_teardown(test_pag_selector_register,
                                 test_pag_selector_register_setup,
                                 test_pag_selector_register_teardown),
        unit_test_setup_teardown(test_pag_selector_unregister,
                                 test_pag_selector_unregister_setup,
                                 test_pag_selector_unregister_teardown),
        unit_test_setup_teardown(test_pag_selector_get_keyset,
                                 test_pag_selector_get_keyset_setup,
                                 test_pag_selector_get_keyset_teardown),
        unit_test_setup_teardown(test_pag_selector_first_key,
                                 test_pag_selector_first_key_setup,
                                 test_pag_selector_first_key_teardown),
        unit_test_setup_teardown(test_pag_selector_next_key,
                                 test_pag_selector_next_key_setup,
                                 test_pag_selector_next_key_teardown),
        unit_test_setup_teardown(test_pag_selector_select,
                                 test_pag_selector_select_setup,
                                 test_pag_selector_select_teardown),
        unit_test_setup_teardown(test_pag_selector_key_is_ready,
                                 test_pag_selector_key_is_ready_setup,
                                 test_pag_selector_key_is_ready_teardown),
        unit_test_setup_teardown(test_pag_selector_key_do_work,
                                 test_pag_selector_key_do_work_setup,
                                 test_pag_selector_key_do_work_teardown),


        /*
         * Functional tests (TODO: could remove later)
         */
        unit_test(test_pag_selector_create_1),
        unit_test(test_pag_selector_create_n),
        unit_test(test_pag_selector_key_create_1),
        unit_test(test_pag_selector_key_create_n),
        unit_test(test_pag_selectors_with_n_keys),
    };

    retc = run_tests(tests);

    return retc;
}
