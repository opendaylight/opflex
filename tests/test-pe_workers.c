#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "ring_buffer.h"
#include "peovs_workers.h"
#include "vlog.h"
#include "vlog-opflex.h"
#include "dbug.h"

VLOG_DEFINE_THIS_MODULE(test_pe_workers);

#define PE_RING_BUFFER_LENGTH 1000 /* TODO: eventually make this configurable 
                                    * see pe_config_defaults in peovs.h
                                    * also, there are dependencies on this
                                    * value in test_ring_buffer.c */
#define PE_RING_BUFFER_ENTRY_SIZE sizeof(void *) /* should this be configurable?
                                                  * to structs that are
                                                  * pushed into ring buf
                                                  */
#define PE_TEST_PRODUCER_THREADS 5
#define PE_TEST_PRODUCER_MAX_PUSH 1010

typedef struct _arg_data {
    int thread_count;
    ring_buffer_t *rb;
} arg_data_t;

/* protos */
void *push_on(void *arg);

/* private */
static int push_counter = 0;
static int push_thread_counter = 0;

static void rt_pe_workers(void **state) {
    (void) state;
    pthread_t push_thread[PE_TEST_PRODUCER_THREADS];
    int tcount = 0;
    ring_buffer_t test_rb;
    arg_data_t push_data;

    VLOG_ENTER(__func__);
    VLOG_DBG("file %s/func %s/line %i\n",
                        __FILE__, __func__,__LINE__);

    memset(&test_rb,0, sizeof(ring_buffer_t));
    test_rb.length = PE_RING_BUFFER_LENGTH;
    test_rb.entry_size = PE_RING_BUFFER_ENTRY_SIZE;
    test_rb.buffer = xcalloc(test_rb.length, test_rb.entry_size);

    push_data.rb = &test_rb;

    /* sets up the consumer */
    pe_workers_init(&test_rb);

    /* producer */
    //(void) pthread_attr_init(&attr); //I know, check return codes...
    //(void) pthread_attr_setdetachstate(&attr,
    //                                   PTHREAD_CREATE_DETACHED);
    for(tcount = 0; tcount < PE_TEST_PRODUCER_THREADS; tcount++) {
        push_data.thread_count = tcount;
        //TODO: check return codes
        pthread_create(&push_thread[tcount],
                        NULL, push_on, (void *) &push_data);
    }

    for(tcount = 0; tcount < PE_TEST_PRODUCER_THREADS; tcount++) {
        xpthread_join(push_thread[tcount], (void **) NULL);
        ++push_thread_counter;
    }

    pe_workers_destroy(&test_rb);

    free(test_rb.buffer);

    //assert_int_equal(push_counter, PE_TEST_PRODUCER_MAX_PUSH);
    assert_int_equal(push_thread_counter, (PE_TEST_PRODUCER_THREADS));

    VLOG_LEAVE(__func__);
}

void *push_on(void *arg) {
    ring_buffer_t *rb = ((arg_data_t *) arg)->rb;
    int indexer = (((arg_data_t *) arg)->thread_count) + 1;
    int counter = 0;
    int passes = 0;
    int push[PE_TEST_PRODUCER_MAX_PUSH/PE_TEST_PRODUCER_THREADS] = { 0 };

    VLOG_ENTER(__func__);

    VLOG_DBG("Thread %i (%p) starting at %i\n",
                        indexer, pthread_self(), ((counter+1)*indexer));

    /* compute number of pases per thread */
    passes = PE_TEST_PRODUCER_MAX_PUSH/PE_TEST_PRODUCER_THREADS;

    for(counter=0;counter<passes;counter++) {
        push[counter] = (counter + 1) * indexer;

        ring_buffer_push(rb, (void *) &push[counter]);
        push_counter++;
//        push_sum += push[counter];
    }

    VLOG_LEAVE(__func__);

    pthread_exit((void *) NULL);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_pe_workers),
    };
    return run_tests(tests);
}
