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
#include <unistd.h>

#include "pag_selector.h"
#include "pag_dispatcher.h"


#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

struct disp_args {
    struct pag_disp_pool *dpool;
    struct pag_selector_key *key;
};

static int trigger;

/*
 * Forward declarations
 */
static void * trigger_add(void * arg);
static int add_sel_fds(struct pollfd *fd, void *arg);
static int sel_ready(void *arg);
static void * sel_work(void *arg);

static void test_pag_dispatch_pool_create(void **state);
static void test_pag_dispatch_pool_create_teardown(void **state);
static void test_pag_dispatch_pool_destroy(void **state);
static void test_pag_dispatch_pool_destroy_setup(void **state);
static void test_pag_dispatch_register(void **state);
static void test_pag_dispatch_register_setup(void **state);
static void test_pag_dispatch_register_teardown(void **state);
static void test_pag_dispatch_unregister(void **state);
static void test_pag_dispatch_unregister_setup(void **state);
static void test_pag_dispatch_unregister_teardown(void **state);
static void test_pag_dispatch_size_1(void **state);
static void test_pag_dispatcher_size_n(void **state);
static void test_pag_dispatch_with_keys(void **state);

/*
 * Helpers
 */
static void *
trigger_add(void * arg)
{
    trigger += 1;
    return 0;
}

static void *
select_func(void * arg)
{
    fd_set rfds;
    struct timeval tv;
    int retval;

    trigger_add(0);

    /* Watch stdout (fd 1) to see when it has output. */
    FD_ZERO(&rfds);
    FD_SET(1, &rfds);

    /* timeout of 1 second. */
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    retval = select(1, &rfds, NULL, NULL, &tv);
    /* Don’t rely on the value of tv now! */

    return 0;
}

static int
add_sel_fds(struct pollfd *fd, void *arg)
{
    trigger_add(0);
    return 0;
}

static int
sel_ready(void *arg)
{
    trigger_add(0);
    return true;
}

static void *
sel_work(void *arg)
{
    trigger_add(0);
    return 0;
}

/**********
 * Tests
 *********/

/*****
 * Unit Test: pag_dispatch_pool_create
 *****/
static void
test_pag_dispatch_pool_create(void **state)
{
    struct pag_disp_pool *dpool = 0;
    int error;

    error = pag_dispatch_pool_create(1, select_func, &dpool);
    assert_true(error == 0);
    assert_true(dpool != 0);

    *state = dpool;
}

static void
test_pag_dispatch_pool_create_teardown(void **state)
{
    struct pag_disp_pool *dpool = (struct pag_disp_pool *)(*state);

    pag_dispatch_pool_destroy(dpool);
}

/*****
 * Unit Test: pag_dispatch_pool_destroy
 *****/
static void
test_pag_dispatch_pool_destroy(void **state)
{
    struct pag_disp_pool *dpool = (struct pag_disp_pool *)(*state);

    pag_dispatch_pool_destroy(dpool);
}

static void
test_pag_dispatch_pool_destroy_setup(void **state)
{
    struct pag_disp_pool *dpool = 0;
    int error;

    error = pag_dispatch_pool_create(1, select_func, &dpool);
    assert_true(error == 0);
    assert_true(dpool != 0);

    *state = dpool;
}

/*****
 * Unit Test: pag_dispatch_register
 *****/
static void
test_pag_dispatch_register(void **state)
{
    struct disp_args *args = (struct disp_args *)(*state);
    int error;

    error = pag_dispatch_register(args->dpool, args->key);
    assert_true(error == 0);
}

static void
test_pag_dispatch_register_setup(void **state)
{
    struct disp_args *args;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    error = pag_dispatch_pool_create(1, select_func, &args->dpool);
    assert_true(error == 0);
    assert_true(args->dpool != 0);

    /* Create a selection key for this session */
    error = pag_selector_key_create(&funcs, 0, &args->key);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_dispatch_register_teardown(void **state)
{
    struct disp_args *args = (struct disp_args *)(*state);
    int error;

    error = pag_dispatch_unregister(args->dpool, args->key);
    assert_true(error == 0);
    pag_dispatch_pool_destroy(args->dpool);
}

/*****
 * Unit Test: pag_dispatch_unregister
 *****/
static void
test_pag_dispatch_unregister(void **state)
{
    struct disp_args *args = (struct disp_args *)(*state);
    int error;

    error = pag_dispatch_unregister(args->dpool, args->key);
    assert_true(error == 0);
}

static void
test_pag_dispatch_unregister_setup(void **state)
{
    struct disp_args *args;
    struct pag_key_handlers funcs = {
        .add_fd   = 0,
        .key_arg  = 0,
        .is_ready = 0,
        .do_work  = 0,
        .work_arg = 0,
    };
    int error;

    args = malloc(sizeof(*args));
    assert_true(args != 0);

    error = pag_dispatch_pool_create(1, select_func, &args->dpool);
    assert_true(error == 0);
    assert_true(args->dpool != 0);

    /* Create a selection key for this session */
    error = pag_selector_key_create(&funcs, 0, &args->key);
    assert_true(error == 0);

    error = pag_dispatch_register(args->dpool, args->key);
    assert_true(error == 0);

    *state = args;
}

static void
test_pag_dispatch_unregister_teardown(void **state)
{
    struct disp_args *args = (struct disp_args *)(*state);

    pag_dispatch_pool_destroy(args->dpool);
}

/***
 * Functional Test: Dispatcher Basic
 *
 * Create a dispatch pool of size 1
 ****/
static void
test_pag_dispatch_size_1(void **state)
{
    struct pag_disp_pool *dpool;
    int n_dispatchers = 1;
    int error = 0;

    (void) state;

    /* initialize trigger */
    trigger = 0;

    error = pag_dispatch_pool_create(n_dispatchers, trigger_add, &dpool);
    assert_true(error == 0);
 
    /* give dispatcher thread time to run */
    sleep(1);

    pag_dispatch_pool_destroy(dpool);

    assert_true(trigger >= n_dispatchers);
}

/***
 * Functional Test: Dispatcher Intermediate
 *
 * Create a dispatch pool of size >1
 ****/
static void
test_pag_dispatcher_size_n(void **state)
{
    struct pag_disp_pool *dpool;
    int n_dispatchers = 10;
    int error = 0;

    (void) state;

    trigger = 0;

    error = pag_dispatch_pool_create(n_dispatchers, trigger_add, &dpool);
    assert_true(error == 0);

    /* give dispatcher threads time to run */
    sleep(1);
    pag_dispatch_pool_destroy(dpool);

    assert_true(trigger >= n_dispatchers);
}

/***
 * Functional Test: Dispatcher with actual selection
 *
 * Create a dispatch pool which does a select with timeout
 ****/
static void
test_pag_dispatch_with_keys(void **state)
{
    struct pag_disp_pool *dpool;
    int n_dispatchers = 1, n_keys=20;
    int error = 0;
    int i;

    (void) state;

    trigger = 0;

    error = pag_dispatch_pool_create(n_dispatchers, select_func, &dpool);
    assert_true(error == 0);

    /*
     *  Create some selector keys and register them on the dispatcher pool
     */
    for (i=0; i<n_keys && !error; i++) {
        struct pag_selector_key *key;
        struct pag_key_handlers funcs = {
            .add_fd   = add_sel_fds,
            .key_arg  = ((void *)(long)i),
            .is_ready = sel_ready,
            .do_work  = sel_work,
            .work_arg = 0,
        };


        /* Create a selection key for this session */
        error = pag_selector_key_create(&funcs, 0, &key);
        assert_true(error == 0);
        error = pag_dispatch_register(dpool, key);
        assert_true(error == 0);
    }

    /* give dispatcher thread time to run */
    sleep(1);
    /* Trigger dispatcher by releasing */
    pag_dispatch_pool_destroy(dpool);

    assert_true(trigger >= ((n_dispatchers*3*n_keys)+1));
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
         * Unit Tests
         */
        unit_test_teardown(test_pag_dispatch_pool_create,
                           test_pag_dispatch_pool_create_teardown),
        unit_test_setup(test_pag_dispatch_pool_destroy,
                        test_pag_dispatch_pool_destroy_setup),
        unit_test_setup_teardown(test_pag_dispatch_register,
                                 test_pag_dispatch_register_setup,
                                 test_pag_dispatch_register_teardown),
        unit_test_setup_teardown(test_pag_dispatch_unregister,
                                 test_pag_dispatch_unregister_setup,
                                 test_pag_dispatch_unregister_teardown),

        /*
         * Functional tests (TODO: could remove later)
         */
        unit_test(test_pag_dispatch_size_1),
        unit_test(test_pag_dispatcher_size_n),
        unit_test(test_pag_dispatch_with_keys),
    };

    retc = run_tests(tests);

    return retc;
}
