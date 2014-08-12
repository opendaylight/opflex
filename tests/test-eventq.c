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

#include "gendcls.h"
#include "config-file.h"
#include "fnm-util.h"
#include "ovs-thread.h"
#include "hash-util.h"
#include "dirs.h"
#include "eventq.h"
#include "tv-util.h"


#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

/* Protos */
static void *test_thread (void *arg);
static void test_setup(void);

/* locals */
static crew_p workq;
static bool thread_running = false;
static eventq_p evt_qp = NULL;
static eventq_ele_t *ep = NULL;
static int cmd_size = 10;
static int qsize = 100;
static int num_threads = 4;

/* ---------------------------------------------------------------------------
 * test setup and other items.
 */
static void test_setup(void)
{
    char *test_conf_fname = "./test-policy-agent.conf";

    conf_load(test_conf_fname);
    conf_dump(stdout);
}


/*----------------------------------------------------------------------------
 * unittests.
 */
static void test_dummy(void **state)
{
    (void) state;
}

static void test_eventq_workpool_teardown(void **state)
{
    (void) state;
    eventq_ele_t *loc_ep = NULL;
    
    while ((loc_ep = get_event_from_queue(evt_qp)) != NULL) {
        if (loc_ep->dp) {
            free(loc_ep->dp);
        }
        free(loc_ep);
    }
    free(evt_qp);
}

static void test_eventq_init_workpool(void **state)
{
    (void) state;
    bool retc;

    retc = init_workpool(cmd_size, sizeof(eventq_ele_t), 1, qsize, &evt_qp);
    assert_false(retc);
    assert_non_null(evt_qp);
}

static void test_eventq_init_workpool_rep1(void **state)
{
    (void) state;
    int a;
    int old_cmd_size = cmd_size;
    int old_qsize = qsize;
    int count = 100;

    cmd_size = 1000;
    qsize = 1000000;

    for (a=0; a < count; a++) {
        test_eventq_init_workpool(state);
        test_eventq_workpool_teardown(state);
    }
    cmd_size = old_cmd_size;
    qsize = old_qsize;
}

static void test_eventq_init_workpool_rep2(void **state)
{
    (void) state;
    int old_cmd_size = cmd_size;
    int old_qsize = qsize;
    int a;

    cmd_size = 100;
    qsize = 1000000;

    for (a=0; a < 10; a++) {
        test_eventq_init_workpool(state);
        test_eventq_workpool_teardown(state);
    }
    cmd_size = old_cmd_size;
    qsize = old_qsize;
}

static void test_eventq_get(void **state)
{
    (void) state;
    int count;
    eventq_ele_t *loc_ep = NULL;

    test_eventq_init_workpool(state);

    for (count = 0; ; count++) {
        loc_ep = get_event_from_queue(evt_qp);

        if (loc_ep == NULL && count == qsize) {
            break;
        } else {
            assert_int_not_equal(count, qsize);
        } 
        if (count > qsize) assert_int_equal(count, qsize);
        if (loc_ep == NULL) {
            assert_null(loc_ep);
        } else {
            if (loc_ep->dp) 
                free(loc_ep->dp);
            free(loc_ep);
        }
    }
    free(evt_qp);
}

static void test_eventq_get_put(void **state)
{
    (void) state;
    int count;
    int retc;
    eventq_ele_t *loc_ep = NULL;

    test_eventq_init_workpool(state);

    for (count = 0; ; count++) {
        if (count == qsize) break;
        loc_ep = get_event_from_queue(evt_qp);
        assert_non_null(loc_ep);
        retc = put_work_back_in_queue(evt_qp, loc_ep);
        assert_false(retc);
    }
    test_eventq_workpool_teardown(state);
}

/* ----------------------------------------------------------------------------
 * crew tests 
 */

static void test_crew_create(void **state)
{
    (void) state;
    bool retc;
    int sav_num_threads = num_threads;
    
    num_threads = 50;
    test_eventq_init_workpool(state);
    workq = xzalloc(sizeof(crew_t));
    assert_non_null(workq);
    
    retc = crew_create(workq, num_threads, num_threads, test_thread);
    assert_false(retc);
    num_threads = sav_num_threads;
}

static void test_crew_destroy(void **state)
{
    (void) state;
    bool retc;

    if (workq) {
        retc = crew_destroy(evt_qp, workq);
        assert_false(retc);
        free(workq);
    }
    if (evt_qp) {
        test_eventq_workpool_teardown(state);
    }
}

static void test_eventq_add(void **state)
{
    (void) state;
    int retc;
    eventq_ele_t *rqstp = NULL;
    int count = 0;
    struct timeval t0, elapsed;
    

    num_threads = 4;
    rqstp = xzalloc(sizeof(eventq_ele_t));
    assert_non_null(rqstp);

    /* fill in request */
    rqstp->state = 1;
    rqstp->tv = t0 = tv_tod();
    rqstp->cmd_size = cmd_size;
    rqstp->dp = xzalloc(cmd_size);
    assert_non_null(rqstp->dp);

    test_crew_create(state);
    retc = eventq_add(workq, rqstp);
    assert_false(retc);
    for (count=0; count < 10000000; count++) {
        if (rqstp->state == 2) {
            elapsed = tv_subtract(tv_tod(), rqstp->tv);
            DEBUG("test_eventq_add: elapsed: %s\n", 
                  tv_show(elapsed, false, NULL));
            break;
        } else {
            sleep(.5);
        }

    }

    free(rqstp->dp);
    free(rqstp);

    test_crew_destroy(state);
    sleep(1);
}

static void test_eventq_add_repeat(void **state)
{
    (void) state;
    int retc;
    eventq_ele_t *rqstp = NULL;
    int count = 0;
    struct timeval t0, elapsed;
    struct timeval start;
    int test_loop_counter = 1000;
    
    num_threads = 4;
    rqstp = xzalloc(sizeof(eventq_ele_t));
    assert_non_null(rqstp);

    /* fill in request */
    rqstp->state = 1;
    /* rqstp->uuid.parts[0] = 0; */
    rqstp->tv = t0 = tv_tod();
    rqstp->cmd_size = cmd_size;
    rqstp->dp = xzalloc(cmd_size);
    assert_non_null(rqstp->dp);

    test_crew_create(state);

    for (count=0; count < test_loop_counter; count++) {
        /* setup the work ele header and send it */
        rqstp->state = 1;
        /* rqstp->uuid.parts[0] = count; */
        rqstp->tv = t0 = tv_tod();
        retc = eventq_add(workq, rqstp);
        assert_false(retc);

        start = tv_tod();
        while (rqstp->state != 2) {
            elapsed = tv_subtract(tv_tod(), start);
            if (elapsed.tv_sec > 2) {
                DEBUG("test_eventq_add_repeat: TOOK TOO LONG\n");
                assert_false(elapsed.tv_sec);
            }
        }

        elapsed = tv_subtract(tv_tod(), rqstp->tv);
        DEBUG("test_eventq_add: elapsed: %s\n", 
              tv_show(elapsed, false, NULL));
    }

    free(rqstp->dp);
    free(rqstp);

    test_crew_destroy(state);
    sleep(1);
}

static void *test_thread (void *arg) 
{
    static char mod[] = "test_thread";
    worker_p mine = (worker_t*)arg;
    crew_p crew = mine->crew;
    eventq_ele_t *work;
    int dWait = 1;

    /* 
     * loop here forever processing work
     */ 
    for (thread_running = true; ; ) {
        /* Wait while there is nothing to do, and the hope of something 
         * coming along later. If crew->first is NULL, there's no work. 
         */
    
        if (dWait) {
            /* DEBUG("%s:%d: Crew waiting for work\n", mod, mine->index); */
            dWait = 0;
        }

        ovs_mutex_lock(&crew->mutex);

        /* If there is nothing to do wait for something......
         */
        if (crew->work_count == 0) {
            /* DEBUG("%s:%d: waiting for work\n", mod, mine->index); */
            ovs_mutex_cond_wait(&crew->go, &crew->mutex);
        }

        if (crew->quit == 1) {
            pthread_cond_signal(&crew->done);
            /* DEBUG("%s:%d: quiting\n", mod, mine->index); */
            crew->crew_size--;
            ovs_mutex_unlock(&crew->mutex);
            goto rtn_return;
        }

        /* DEBUG("%s: crew work_count = %d\n", mod, crew->work_count); */
      
        if (crew->work_count > 0) {
            /* DEBUG("%s:%ld:%d: ======== Woke: work_count=%d=========\n", */
            /*       mod, pthread_self(), mine->index, crew->work_count); */

            /* Remove and process a work item
             */
            work = crew->first;
            crew->first = work->next;
            if (crew->first == NULL)
                crew->last = NULL;

            work->next = NULL;
            crew->work_count--;

            ovs_mutex_unlock (&crew->mutex);
	
            work->state = 2;
            DEBUG("%s: Dispatching on: %s \n",
                  tv_show(tv_subtract(tv_tod(), work->tv), false, NULL));
        }
        else {
            ovs_mutex_unlock(&crew->mutex);
        }
    }

 rtn_return:
    thread_running = false;
    //    DEBUG("%s: exiting.\n", mod);
    pthread_exit(NULL);
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Test driver.
 */
int main(int argc, char *argv[])
{
    int retc; 

    const UnitTest tests[] = {
        unit_test_setup_teardown(test_eventq_init_workpool,
                                 test_dummy,
                                 test_eventq_workpool_teardown),
        unit_test(test_eventq_init_workpool_rep1),
        unit_test(test_eventq_init_workpool_rep2),
        unit_test(test_eventq_get),
        unit_test(test_eventq_get_put),
        unit_test_setup_teardown(test_crew_create,
                                 test_dummy, test_crew_destroy),
        unit_test(test_eventq_add),
        unit_test(test_eventq_add_repeat),
   };

    test_setup();
    retc = run_tests(tests);

    return retc;
}
