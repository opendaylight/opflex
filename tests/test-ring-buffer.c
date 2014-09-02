#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "ring_buffer.h"
#include "misc-util.h"
#include "dbug.h"
#include "vlog.h"
#include "vlog-opflex.h"

/* For push_pop_buffer()
 * the following two values are determined along with
 * PE_RING_BUFFER_LENGTH such that
 * PE_TEST_POP_THREAD_COUNT*PE_TEST_MAX_POP_COUNT=PE_RING_BUFFER_LENGTH,
 * which leaves 1 final entry to pop.
 */
#define PE_RING_BUFFER_LENGTH 1000 /* TODO: eventually make this configurable 
                                    * see pe_config_defaults in peovs.h
                                    * also, there are dependencies on this
                                    * value in test_ring_buffer.c */
#define PE_RING_BUFFER_ENTRY_SIZE sizeof(void *) /* should this be configurable?
                                                  * to structs that are
                                                  * pushed into ring buf
                                                  */
#define PE_TEST_POP_THREAD_COUNT 20
#define PE_TEST_MAX_POP_COUNT 50

/* protos */
static void *push_on(void *arg);
static void *pop_off(void *arg);
static void push_pop_buffer(void **state);
static void two_buffer(void **state);
static void *two_push(void *arg);
static void *two_pull(void *arg);
static void *push2_f(void *arg);
static void *pull2_f(void *arg);

/* private */
static int push_counter = 0;
static int push_sum = 0;
static int pop_counter = 0;
static int pop_sum = 0;
static int push[PE_RING_BUFFER_LENGTH+1] = { 0 };
static struct ovs_mutex pop_lock;
/*
static bool time_to_quit = false;
static uint32_t two_push_count[2] = { 0 };
static uint32_t two_pull_count[2] = { 0 };
static unsigned int two_big_push_sum[2];
static unsigned int two_big_pull_sum[2];
const struct ovs_mutex two_push_lock[2];
const struct ovs_mutex two_pull_lock[2];
*/

static unsigned int two_push_array[2][11][5];
//static unsigned int two_pull_array[2][11][5];

typedef struct _arg_data {
    int arg;
    ring_buffer_t *rb;
} arg_data_t;

typedef struct _two_arg_data {
    unsigned int result;
    uint32_t id;
    uint32_t count;
    uint32_t which_rb;
    ring_buffer_t *rb;
} two_arg_data_t;

VLOG_DEFINE_THIS_MODULE(test_ring_buffer);


static void push_pop_buffer(void **state) {
    (void) state;
    ring_buffer_t test_rb;
    pthread_attr_t attr;
    pthread_t push_thread;
    pthread_t pop_thread[PE_TEST_POP_THREAD_COUNT];
    pthread_t last_pop_thread;
    int counter = 0;
    arg_data_t push_data;
    arg_data_t pop_data;

    /* Enable debugging
    vlog_set_levels(vlog_module_from_name("test_ring_buffer"), -1, VLL_DBG);
    */
    vlog_set_levels(vlog_module_from_name("test_ring_buffer"), -1, VLL_INFO);

    VLOG_ENTER(__func__);
    VLOG_DBG("file %s/func %s/line %i\n",
                        __FILE__, __func__,__LINE__);

    memset(&test_rb,0,sizeof(ring_buffer_t));
    test_rb.length = PE_RING_BUFFER_LENGTH;
    test_rb.entry_size = PE_RING_BUFFER_ENTRY_SIZE;

    test_rb.buffer = xcalloc(test_rb.length, test_rb.entry_size);

    ring_buffer_init(&test_rb);

    push_data.rb = &test_rb;
    pop_data.rb = &test_rb;
    push_data.arg = 0;
    pop_data.arg = 0;

    /* producer */
    (void) pthread_attr_init(&attr); //non portable
    (void) pthread_attr_setdetachstate(&attr,
                                       PTHREAD_CREATE_DETACHED);
    //TODO: check return codes`
    pthread_create(&push_thread, &attr, push_on, (void *) &push_data);

    /* make sure the producer fills the buffer */
    while(push_counter != (PE_RING_BUFFER_LENGTH-1))
        sleep(1);

    /* consumer */
    ovs_mutex_init(&pop_lock);
    for(counter=0;counter<(PE_TEST_POP_THREAD_COUNT);counter++) {
        //TODO:check return codes
        pthread_create(&pop_thread[counter], NULL,
                        pop_off, (void *) &pop_data);
        VLOG_DBG("created pop thread %i",counter);
    }

    for(counter=0;counter<(PE_TEST_POP_THREAD_COUNT);counter++) {
        xpthread_join(pop_thread[counter], (void **) NULL);
        VLOG_DBG("joined pop thread %i",counter);
    }

    /* at this point, there should be 1 remaing entry to pop */
    
    pop_data.arg = 1;
    //TODO:check return codes
    pthread_create(&last_pop_thread, NULL, pop_off, (void *) &pop_data);
    VLOG_DBG("created last pop thread");
    xpthread_join(last_pop_thread, (void **) NULL);
    VLOG_DBG("joined last pop thread");

    VLOG_DBG("results: push_count %i, push_sum %i",
                            push_counter,push_sum);
    VLOG_DBG("         pop_count %i,  pop_sum %i",
                            pop_counter,pop_sum);

    free(test_rb.buffer);

    assert_int_equal(pop_sum,push_sum);

    VLOG_LEAVE(__func__);
}

static void *pop_off(void *arg) {
    void *pop = NULL;
    int count_pops = 0;
    arg_data_t *mydata = (arg_data_t *) arg;

    VLOG_ENTER(__func__);

    if(mydata->arg != 1) {
        for(count_pops = 0; count_pops < PE_TEST_MAX_POP_COUNT; count_pops++) {
            pop = ring_buffer_pop(mydata->rb);
            ovs_mutex_lock(&pop_lock);
            pop_counter++;
            pop_sum += *((int *) pop);
            ovs_mutex_unlock(&pop_lock);
        }
    } else {
        pop = ring_buffer_pop(mydata->rb);
        ovs_mutex_lock(&pop_lock);
        pop_counter++;
        pop_sum += *((int *) pop);
        ovs_mutex_unlock(&pop_lock);
    }
        
    VLOG_DBG("pop_off tid %u, pop_sum/push_sum %i/%i",
                        pthread_self(),pop_sum,push_sum);
    VLOG_LEAVE(__func__);

    pthread_exit((void *) NULL);
}

static void *push_on(void *arg) {
    int counter = 0;
    arg_data_t *mydata = (arg_data_t *) arg;

    VLOG_ENTER(__func__);

    for(counter=0;counter<(PE_RING_BUFFER_LENGTH+1);counter++) {
        push[counter] = counter + 1;
        ring_buffer_push(mydata->rb, (void *) &push[counter]);
        push_counter++;
        push_sum += push[counter];
    }

    VLOG_LEAVE(__func__);

    return((void *) NULL);
}

static void two_buffer(void **state) {

/* This test sets up two ring buffers: trb1 and trb2. Then,
 * threads are created to push and pop data onto and off each
 * trb*
 */

    two_arg_data_t push1;
    two_arg_data_t push2;
    two_arg_data_t pull1;
    two_arg_data_t pull2;
    ring_buffer_t trb1;
    ring_buffer_t trb2;
    pthread_t push_thread1;
    pthread_t push_thread2;
    pthread_t pull_thread1;
    pthread_t pull_thread2;

    /* Enable for debugging
    vlog_set_levels(vlog_module_from_name("test_ring_buffer"), -1, VLL_DBG);
    */

    VLOG_ENTER(__func__);

    memset((void *) &trb1, 0, sizeof(ring_buffer_t));
    memset((void *) &trb2, 0, sizeof(ring_buffer_t));
    memset((void *) &push1, 0, sizeof(two_arg_data_t));
    memset((void *) &push2, 0, sizeof(two_arg_data_t));
    memset((void *) &pull1, 0, sizeof(two_arg_data_t));
    memset((void *) &pull2, 0, sizeof(two_arg_data_t));
    memset((void *) two_push_array, 0, 110 * sizeof(unsigned int));
//    memset((void *) two_pull_array, 0, 110 * sizeof(unsigned int));

    trb1.length = 10;
    trb2.length = 20;
    trb1.entry_size = 8;
    trb2.entry_size = 8;

    trb1.buffer = xcalloc((size_t) trb1.length, (size_t) trb1.entry_size);    
    trb2.buffer = xcalloc((size_t) trb2.length, (size_t) trb2.entry_size);    

    ring_buffer_init(&trb1);
    ring_buffer_init(&trb2);

    push1.rb = &trb1;
    push1.which_rb = 0;
    push2.rb = &trb2;
    push2.which_rb = 1;
    pull1.rb = &trb1;
    pull1.which_rb = 0;
    pull2.rb = &trb2;
    pull2.which_rb = 1;

    //Start threads to push
    pag_pthread_create(&push_thread1,NULL,two_push,(void *) &push1);
    pag_pthread_create(&push_thread2,NULL,two_push,(void *) &push2);

    //Start threads to pull
    pag_pthread_create(&pull_thread1,NULL,two_pull,(void *) &pull1);
    pag_pthread_create(&pull_thread2,NULL,two_pull,(void *) &pull2);

    //give threads time to finish
    //sleep(5);

    //Set flag to quit?
    //time_to_quit = true;

    //Broadcast signals?
    //rb_broadcast_cond_variables(&trb1);
    //rb_broadcast_cond_variables(&trb2);

    //pthread_join
    pag_pthread_join(push_thread1, NULL);
    pag_pthread_join(push_thread2, NULL);
    pag_pthread_join(pull_thread1, NULL);
    pag_pthread_join(pull_thread2, NULL);

    VLOG_INFO("Push Result 1: %u\t Pull Result 1: %u\n"
              "Push Count  1: %u\t Pull Count  1: %u\n"
              "Push Result 2: %u\t Pull Result 2: %u\n"
              "Push Count  2: %u\t Pull Count  2: %u\n",
               push1.result,
               pull1.result,
               push1.count,
               pull1.count,
               push2.result,
               pull2.result,
               push2.count,
               pull2.count);
 
    //asserts
    assert_int_equal(push1.result, pull1.result);
    assert_int_equal(push2.result, pull2.result);

    //Return memory
    free(trb1.buffer);
    free(trb2.buffer);
    ring_buffer_destroy(&trb1);
    ring_buffer_destroy(&trb2);

    VLOG_LEAVE(__func__);
}

static void *two_push(void *arg) {
    pthread_t tid[11];
    two_arg_data_t tad[11];
    int count = 0;
    uint32_t rcount = 0;
    unsigned int sum = 0;

    VLOG_ENTER(__func__);

    for (count = 0; count < 11; count++) {
        tad[count].rb = ((two_arg_data_t *) arg)->rb;
        tad[count].which_rb = ((two_arg_data_t *) arg)->which_rb;
        tad[count].id = count;
        tad[count].result = 0;
        tad[count].count = 0;
        pag_pthread_create(&tid[count], NULL, push2_f, (void *) &tad[count]);
    }

    for (count = 0; count < 11; count++) {
        pag_pthread_join(tid[count], NULL);
        sum += tad[count].result;
        rcount += tad[count].count;
    }

    ((two_arg_data_t *) arg)->result = sum;
    ((two_arg_data_t *) arg)->count = rcount;
    VLOG_DBG("%s RB (%p) TID %u: sum = %u\n", __func__,
                                              ((two_arg_data_t *) arg)->rb,
                                              pthread_self(),
                                              sum);

    VLOG_LEAVE(__func__);
    return(NULL);
}

static void *two_pull(void *arg) {
    pthread_t tid[11];
    two_arg_data_t tad[11];
    int count = 0;
    uint32_t rcount = 0;
    unsigned int sum = 0;

    VLOG_ENTER(__func__);
    for (count = 0; count < 11; count++) {
        tad[count].rb = ((two_arg_data_t *) arg)->rb;
        tad[count].which_rb = ((two_arg_data_t *) arg)->which_rb;
        tad[count].id = count;
        tad[count].result = 0;
        tad[count].count = 0;
        pag_pthread_create(&tid[count], NULL, pull2_f, &tad[count]);
    }
    for (count = 0; count < 11; count++) {
        pag_pthread_join(tid[count], NULL);
        sum += tad[count].result;
        rcount += tad[count].count;
    }

    ((two_arg_data_t *) arg)->result = sum;
    ((two_arg_data_t *) arg)->count = rcount;
    VLOG_DBG("%s RB (%p) TID %u: sum = %u\n", __func__, 
                                              ((two_arg_data_t *) arg)->rb,
                                              pthread_self(),
                                              sum);

    VLOG_LEAVE(__func__);
    return(NULL);
}

static void *push2_f(void *arg) {
    two_arg_data_t *tad = (two_arg_data_t *) arg;
    int count = 0;
    unsigned int sum = 1;
    unsigned int rstate = pthread_self();

    VLOG_ENTER(__func__);

    for (count = 0; count < 5; count++) {
        /* get an int between 1 and 20...don't really
         * care about the degree of randomness or would
         * use something like drand48_r or better.
         */
        two_push_array[tad->which_rb][tad->id][count] = 
                       ((rand_r(&rstate)) % 20) + 1;
        ring_buffer_push(tad->rb,
                    (void *) &two_push_array[tad->which_rb][tad->id][count]);
        tad->count++;
        sum += two_push_array[tad->which_rb][tad->id][count];


        VLOG_DBG("Just pushed %p(%u) in RB-%u(%p) sum = %u\n",
                  &two_push_array[tad->which_rb][tad->id][count],
                  two_push_array[tad->which_rb][tad->id][count],
                  tad->which_rb,tad->rb,sum);

    }

    /* reduce sum by 1 so that it will line up with the sum from pull2_f() */
    sum -= 1;
    tad->result = sum;

    VLOG_DBG("%s RB-%u(%p) TID %u: sum = %u\n", __func__, tad->which_rb,
                                              tad->rb,
                                              pthread_self(), sum);

    VLOG_LEAVE(__func__);
    return(NULL);
}

static void *pull2_f(void *arg) {
    two_arg_data_t *tad = (two_arg_data_t *) arg;
    void *pull[5];
    int count = 0;
    unsigned int sum = 0;

    VLOG_ENTER(__func__);

    for (count = 0; count < 5; count++) {
        pull[count] = ring_buffer_pop(tad->rb);
        VLOG_DBG("Just Pulled %p(%u) from  RB-%u(%p)\n",
                pull[count], *((unsigned int *) pull[count]),
                tad->which_rb,tad->rb);
        if (*((unsigned int *) pull[count]) > 20)
            ovs_abort(-1, "pulled %u (%p)\n",
                      *((unsigned int *) pull[count]),pull[count]);
        tad->count++;
        sum += *((unsigned int *) pull[count]);
        VLOG_DBG("%s: RB-%u(%p) sum = %u\n",
                 __func__,tad->which_rb,tad->rb, sum);
    }
    tad->result = sum;

    VLOG_DBG("%s RB-%u(%p) TID %u: sum = %u\n", __func__,
                                               tad->which_rb, tad->rb,
                                              pthread_self(), sum);

    VLOG_LEAVE(__func__);
    return(NULL);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(two_buffer),
        unit_test(push_pop_buffer),
    };
    putenv("CMOCKA_TEST_ABORT=1");
    return run_tests(tests);
}
