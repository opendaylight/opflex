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
#include <time.h>
#include <pthread.h>
#include <cmocka.h>

#include "ovs-thread.h"
#include "modb.h"
#include "modb-event.h"
#include "config-file.h"
#include "uri-parser.h"
#include "tv-util.h"
#include "vlog.h"
#include "vlog-opflex.h"

VLOG_DEFINE_THIS_MODULE(test_modb_event);

#ifdef NDEBUG
#define DEBUG(...)
#else
#define DEBUG(...) printf(__VA_ARGS__)
#endif

static int modb_insert_one(node_ele_p *ndp, const char *uri_str, result_p rs);
static void create_node_tree(node_ele_p *parentp, int count);
static char *mevt_obj_to_string(int otype);
static void *subscriber_thread1(void *arg);
static void *subscriber_thread_upd(void *arg);
static void *subscriber_thread_del(void *arg);
static void *subscriber_thread_ins(void *arg);

static const char *uri_str = 
    "http://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Naming.2C_addressing.2C_and_identifying_resources";
static const char *debug_level;

#define THREAD_WAIT 400000

/* static pthread_mutex_t upd_mutex = PTHREAD_MUTEX_INITIALIZER; */
/* static pthread_mutex_t del_mutex = PTHREAD_MUTEX_INITIALIZER; */
/* static pthread_mutex_t ins_mutex = PTHREAD_MUTEX_INITIALIZER; */

/* static unsigned long upd_counter; */
/* static unsigned long del_counter; */
/* static unsigned long ins_counter; */


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

static void test_teardown(void) 
{
    modb_cleanup();
}

static void thread_delay(int timev)
{
    struct timespec wts, tem;
    wts.tv_sec = timev;
    wts.tv_nsec = THREAD_WAIT;
    /* printf("============= thread_delay: %d ======================\n", timev); */
    nanosleep(&wts, &tem);
    modb_event_is_all_read();
}

static void *subscriber_thread1(void *arg)
{
    static char *mod = "subscriber_thread";
    modb_event_p mevtp;
    unsigned int chk_type;
    int retc = 0;
    pthread_t this = pthread_self();
    char *etp;
    bool run_flag = true;

    printf("%s: ======= thread: %lu started\n", mod, this);
    assert_return_code(modb_event_subscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    for (run_flag=true; run_flag; ) {
        printf("subscriber_threads: %lu WAITING\n", this);

        assert_return_code(modb_event_wait(&mevtp), 0);
        chk_type = mevtp->etype;

        etp = modb_event_etype_to_string(mevtp->etype);
        printf("%s: +++++++ EVENT RECEIVED: %lu ts:%s type:%s id:%lu count:%d +++++\n",
               mod, this, tv_show(mevtp->timestamp, true, NULL), etp, 
               mevtp->event_id, mevtp->dp_count); 
        free(etp);
        fflush(stdout);

        if (mevtp->etype & MEVT_TYPE_DESTROY) {
            retc = 0;
            printf("%s: ****** DESTROY EVENT RECEIVED: %lu ts:%s id:%ld type:0x%04x *****\n",
                   mod, this, tv_show(mevtp->timestamp, true, NULL), 
                   mevtp->event_id, mevtp->etype);
            printf("***** %s:%lu setting run_flag to false\n", mod, this);
            run_flag = false;
        }
        memset(mevtp, 0, sizeof(modb_event_t));
        modb_event_free(mevtp);
    }
    assert_return_code(modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    pthread_exit(&retc);
    return(NULL);
} 

static void *subscriber_thread_upd(void *arg)
{
    static char *mod = "subscriber_thread_upd";
    modb_event_p mevtp;
    unsigned int chk_type;
    int retc = 0;
    pthread_t this = pthread_self();
    char *etp;
    bool run_flag = true;

    printf("%s: ======= thread: %lu started\n", mod, this);
    assert_return_code(modb_event_subscribe(MEVT_TYPE_UPD, MEVT_SRC_ANY), 0);
    for (run_flag=true; run_flag; ) {
        printf("subscriber_threads: %lu WAITING\n", this);

        assert_return_code(modb_event_wait(&mevtp), 0);
        chk_type = mevtp->etype;

        etp = modb_event_etype_to_string(mevtp->etype);
        printf("%s: +++++++ EVENT RECEIVED: %lu ts:%s type:%s id:%ld count:%d +++++\n", mod, this,
               tv_show(mevtp->timestamp, true, NULL), etp, mevtp->event_id, mevtp->dp_count); 
        free(etp);
        fflush(stdout);

        if (mevtp->etype & MEVT_TYPE_DESTROY) {
            retc = 0;
            printf("%s: ****** DESTROY EVENT RECEIVED: %lu ts:%s id:%ld type:0x%04x *****\n",
                   mod, this, tv_show(mevtp->timestamp, true, NULL),
                   mevtp->event_id, mevtp->etype);
            run_flag = false;
        }
        memset(mevtp, 0, sizeof(modb_event_t));
        modb_event_free(mevtp);
    }
    assert_return_code(modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    pthread_exit(&retc);
    return(NULL);
} 

static void *subscriber_thread_del(void *arg)
{
    static char *mod = "subscriber_thread_del";
    modb_event_p mevtp;
    unsigned int chk_type;
    int retc = 0;
    pthread_t this = pthread_self();
    char *etp;
    bool run_flag = true;

    printf("%s: ======= thread: %lu started\n", mod, this);
    assert_return_code(modb_event_subscribe(MEVT_TYPE_DEL, MEVT_SRC_ANY), 0);
    for (run_flag=true; run_flag; ) {
        printf("subscriber_threads: %lu WAITING\n", this);

        assert_return_code(modb_event_wait(&mevtp), 0);
        chk_type = mevtp->etype;

        etp = modb_event_etype_to_string(mevtp->etype);
        printf("%s: +++++++ EVENT RECEIVED: %lu ts:%s type:%s id:%ld count:%d +++++\n", mod, this,
               tv_show(mevtp->timestamp, true, NULL), etp, mevtp->event_id, mevtp->dp_count); 
        free(etp);
        fflush(stdout);

        if (mevtp->etype & MEVT_TYPE_DESTROY) {
            retc = 0;
            printf("%s: ****** DESTROY EVENT RECEIVED: %lu ts:%s id:%ld type:0x%04x *****\n",
                   mod, this, tv_show(mevtp->timestamp, true, NULL), 
                   mevtp->event_id, mevtp->etype);
            run_flag = false;
        }
        memset(mevtp, 0, sizeof(modb_event_t));
        modb_event_free(mevtp);
    }
    assert_return_code(modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    pthread_exit(&retc);
    return(NULL);
} 

static void *subscriber_thread_ins(void *arg)
{
    static char *mod = "subscriber_thread_ins";
    modb_event_p mevtp;
    unsigned int chk_type;
    int retc = 0;
    pthread_t this = pthread_self();
    char *etp;
    bool run_flag = true;

    printf("%s: ======= thread: %lu started\n", mod, this);
    assert_return_code(modb_event_subscribe(MEVT_TYPE_INS, MEVT_SRC_ANY), 0);
    for (run_flag=true; run_flag; ) {
        printf("subscriber_threads: %lu WAITING\n", this);

        assert_return_code(modb_event_wait(&mevtp), 0);
        chk_type = mevtp->etype;

        etp = modb_event_etype_to_string(mevtp->etype);
        printf("%s: +++++++ EVENT RECEIVED: %lu ts:%s type:%s id:%ld count:%d +++++\n", mod, this,
               tv_show(mevtp->timestamp, true, NULL), etp, mevtp->event_id, mevtp->dp_count); 
        free(etp);
        fflush(stdout);

        if (mevtp->etype & MEVT_TYPE_DESTROY) {
            retc = 0;
            printf("%s: ****** DESTROY EVENT RECEIVED: %lu ts:%s id:%ld type:0x%04x *****\n",
                   mod, this, tv_show(mevtp->timestamp, true, NULL),
                   mevtp->event_id, mevtp->etype);
            run_flag = false;
        }
        memset(mevtp, 0, sizeof(modb_event_t));
        modb_event_free(mevtp);
    }
    assert_return_code(modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    pthread_exit(&retc);
    return(NULL);
} 

static char *mevt_obj_to_string(int otype)
{
    static char *mevt_obj_lookup[] = {
        "Node",
        "Tree",
        "Property"
    };
    return(mevt_obj_lookup[otype]);
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

/*----------------------------------------------------------------------------
 * unittests.
 */
static void test_modb_event_initialize(void **state)
{
    (void) state;
    assert_false(modb_initialize());
    modb_cleanup();
}

static void test_modb_event_subscribe(void **state)
{
    (void) state;

    assert_false(modb_initialize());
    assert_return_code(modb_event_subscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    modb_event_dump();
    assert_return_code(modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    modb_cleanup();
}

static void test_modb_event_subscribe_no_unsubscribe(void **state)
{
    (void) state;

    assert_false(modb_initialize());
    assert_return_code(modb_event_subscribe(MEVT_TYPE_ANY, MEVT_SRC_ANY), 0);
    modb_event_dump();
    modb_cleanup();
}

static void test_modb_event_etype_to_string( void **state)
{
    char *etype_string;
    (void) state;

    etype_string = modb_event_etype_to_string(MEVT_TYPE_NOP);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_INS);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_DEL);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_UPD);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_INS | MEVT_TYPE_DEL);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_DESTROY);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_TEST);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_ANY);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    etype_string = modb_event_etype_to_string(MEVT_TYPE_INS | MEVT_TYPE_DESTROY | MEVT_TYPE_UPD);
    printf("etype_string=%s\n", etype_string); 
    free(etype_string);
    
}
static pthread_attr_t wc_attr;

static void test_modb_event_subscribers(void **state)
{
#define NUM_SUBSCRIBERS 9
    int i;
    pthread_t tid[NUM_SUBSCRIBERS];
    unsigned int filter_type = MEVT_TYPE_ANY;
    (void) state;
    node_ele_p ndp[5] = {0};
    node_ele_t node;
    char tname[64];
    int *retval;
    int retc;

    assert_false(modb_initialize());

    for (i=0; i < NUM_SUBSCRIBERS; i++) {
        /* Create the subscribers */
        bzero(&tname, 64);
        sprintf(tname, "thread-%d", i);
        retc = pthread_attr_init(&wc_attr);
        retc = pthread_attr_setdetachstate(&wc_attr,
                                           PTHREAD_CREATE_JOINABLE);
        pthread_create(&tid[i], NULL, subscriber_thread1, &filter_type);
        printf("test_modb_event_subscribers: thread:%lu started\n", tid[i]);
    }

    while (modb_event_subscribers() != NUM_SUBSCRIBERS)
        thread_delay(0);

    ndp[0] = &node;
    assert_return_code(modb_event_push(MEVT_TYPE_TEST, MEVT_SRC_INTERNAL,
                                      MEVT_OBJ_NODE, 1, (void **)ndp), 0);

    thread_delay(1);
    assert_return_code(modb_event_push(MEVT_TYPE_DESTROY, MEVT_SRC_INTERNAL,
                                      MEVT_OBJ_NODE, 1, (void *)&node), 0);

    for (i=0; i < NUM_SUBSCRIBERS; i++) {
        pthread_join(tid[i], (void *)&retval);
        printf("pthread_join:%lu\n", tid[i]);
    }

    modb_cleanup();
}

static void test_modb_event_subscribers_filters(void **state)
{
#define NUM_SUBSCRIBERS_2 3
    int i;
    pthread_t tid[NUM_SUBSCRIBERS_2];
    unsigned int filter_type = MEVT_TYPE_ANY;
    (void) state;
    node_ele_p ndp[5] = {0};
    node_ele_t node;
    int *retval;
    int retc;
    bool all_threads_active = false;
    int act_thr;

    assert_false(modb_initialize());

    /* Create the subscribers */
    retc = pthread_attr_init(&wc_attr);
    retc = pthread_attr_setdetachstate(&wc_attr,
                                       PTHREAD_CREATE_JOINABLE);
    pthread_create(&tid[i++], NULL, subscriber_thread_upd, &filter_type);
    pthread_create(&tid[i++], NULL, subscriber_thread_del, &filter_type);
    pthread_create(&tid[i++], NULL, subscriber_thread_ins, &filter_type);


    for (all_threads_active = false; all_threads_active == false; ) {
        act_thr = modb_event_subscribers();
        if (act_thr == NUM_SUBSCRIBERS_2) {
            all_threads_active = true;
        }
        else {
            thread_delay(0);
        }
    }

    ndp[0] = &node;

    /* events */
    assert_return_code(modb_event_push(MEVT_TYPE_INS, MEVT_SRC_NORTH,
                                      MEVT_OBJ_NODE, 1, (void **)ndp), 0);

    thread_delay(0);
    assert_return_code(modb_event_push(MEVT_TYPE_UPD, MEVT_SRC_NORTH,
                                      MEVT_OBJ_NODE, 1, (void **)ndp), 0);

    thread_delay(0);
    assert_return_code(modb_event_push(MEVT_TYPE_DEL, MEVT_SRC_NORTH,
                                      MEVT_OBJ_NODE, 1, (void **)ndp), 0);

    thread_delay(1);


    /* Destroy the eventing system */
    assert_return_code(modb_event_push(MEVT_TYPE_DESTROY, MEVT_SRC_INTERNAL,
                                      MEVT_OBJ_NODE, 1, (void *)&node), 0);

    thread_delay(1);

    for (i=0; i < NUM_SUBSCRIBERS_2; i++) {
        pthread_join(tid[i], (void *)&retval);
        printf("pthread_join:%lu\n", tid[i]);
    }

    modb_cleanup();
}

static void test_modb_event_db_integration(void **state)
{
#define NUM_DB_INTRG_SUBSCRIBERS 4
    int i;
    pthread_t tid[NUM_DB_INTRG_SUBSCRIBERS];
    unsigned int filter_type = MEVT_TYPE_ANY;
    (void) state;
    node_ele_p ndp[5] = {0};
    node_ele_t node;
    char tname[64];
    int *retval;
    int retc;
    node_ele_p parent;
    result_t rs;

    assert_false(modb_initialize());

    create_node_tree(&parent, 2);

    for (i = 0; i < NUM_DB_INTRG_SUBSCRIBERS; i++) {
        /* Create the subscribers */
        bzero(&tname, 64);
        sprintf(tname, "thread-%d", i);
        retc = pthread_attr_init(&wc_attr);
        retc = pthread_attr_setdetachstate(&wc_attr,
                                           PTHREAD_CREATE_JOINABLE);
        pthread_create(&tid[i], NULL, subscriber_thread1, &filter_type);
        printf("test_modb_event_subscribers: thread:%lu started\n", tid[i]);
    }

    while (modb_event_subscribers() != NUM_DB_INTRG_SUBSCRIBERS)
        thread_delay(0);

    /* subscribers are waiting */
    assert_false(modb_op(OP_INSERT, OP_SRC_INTERNAL, (void *)parent,
                         IT_NODE, 1, EXT_NODE, &rs));
    modb_dump(true);
    thread_delay(1);
    assert_false(modb_op(OP_DELETE, OP_SRC_INTERNAL, (void *)parent,
                         IT_NODE, 1, EXT_NODE, &rs));
    modb_dump(true);
    thread_delay(1);
    modb_cleanup();
    thread_delay(1);

    for (i=0; i < NUM_DB_INTRG_SUBSCRIBERS; i++) {
        pthread_join(tid[i], (void *)&retval);
        printf("pthread_join:%lu\n", tid[i]);
    }
}


/* ---------------------------------------------------------------------------
 * Test driver.
 */
int main(int argc, char *argv[])
{
    int retc; 

    const UnitTest tests[] = {
        unit_test(test_modb_event_etype_to_string),
        unit_test(test_modb_event_initialize),
        unit_test(test_modb_event_subscribe),
        unit_test(test_modb_event_subscribe_no_unsubscribe),
        unit_test(test_modb_event_subscribers),
        unit_test(test_modb_event_subscribers_filters),
        unit_test(test_modb_event_db_integration),
    };

    test_setup();
    retc = run_tests(tests);
    test_teardown();
    return retc;
}
