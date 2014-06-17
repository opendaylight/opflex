#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <unistd.h>

#define DBUG_OFF 1 //turn off debugging

#include "pag-thread.h"
#include "ring_buffer.h"
#include "pe_workers.h"
//#include "policy_enforcer.h"
#include "dbug.h"

#define PE_TEST_PRODUCER_THREADS 5
#define PE_TEST_PRODUCER_MAX_PUSH 1010

/* protos */
void *push_on(void *arg);

/* private */
static int push_counter = 0;
static int push_thread_counter = 0;

static void rt_pe_workers(void **state) {
    (void) state;
    pthread_t push_thread[PE_TEST_PRODUCER_THREADS];
    int idx[PE_TEST_PRODUCER_THREADS];
    int tcount = 0;

    //DBUG_PUSH("d:F:i:L:n:t");
    DBUG_PUSH("d:t:i:L:n:P:T:0");
    DBUG_ENTER("rt_pe_workers");
    DBUG_PRINT("DEBUG",("file %s/func %s/line %i\n",
                        __FILE__, __func__,__LINE__));

    /* sets up the consumer */
    pe_workers_init();

    /* producer */
    //(void) pthread_attr_init(&attr); //I know, check return codes...
    //(void) pthread_attr_setdetachstate(&attr,
    //                                   PTHREAD_CREATE_DETACHED);
    for(tcount = 0; tcount < PE_TEST_PRODUCER_THREADS; tcount++) {
        idx[tcount] = tcount;
        xpthread_create(&push_thread[tcount],
                        NULL, push_on, (void *) &idx[tcount]);
    }

    for(tcount = 0; tcount < PE_TEST_PRODUCER_THREADS; tcount++) {
        xpthread_join(push_thread[tcount], (void **) NULL);
        push_thread_counter++;
    }

    pe_workers_destroy();

    assert_int_equal(push_counter, PE_TEST_PRODUCER_MAX_PUSH);
    assert_int_equal(push_thread_counter, PE_TEST_PRODUCER_THREADS);

    DBUG_LEAVE;
}

void *push_on(void *arg) {
    int indexer = *(int *) arg + 1;
    int counter = 0;
    int passes = 0;
    int push[PE_TEST_PRODUCER_MAX_PUSH/PE_TEST_PRODUCER_THREADS] = { 0 };

    DBUG_ENTER("push_on");

    DBUG_PRINT("DEBUG",("Thread %i (%p) starting at %i\n",
                        indexer, pthread_self(), ((counter+1)*indexer)));

    /* compute number of pases per thread */
    passes = PE_TEST_PRODUCER_MAX_PUSH/PE_TEST_PRODUCER_THREADS;

    for(counter=0;counter<passes;counter++) {
        push[counter] = (counter + 1) * indexer;

//        DBUG_PRINT("DEBUG",("Thread %i (%p) pushing %i (%p)\n",
//                   indexer, pthread_self(), push[counter], &push[counter]));

        ring_buffer_push((void *) &push[counter]);
        push_counter++;
//        push_sum += push[counter];
    }

    DBUG_LEAVE;

    pthread_exit((void *) NULL);
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(rt_pe_workers),
    };
    return run_tests(tests);
}
