#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <unistd.h>

#include "ovs-thread.h"
#include "ring_buffer.h"
#include "dbug.h"
#include "vlog.h"

/* protos */
void *push_on(void *arg);
void *pop_off(void *arg);

/* private */
static int push_counter = 0;
static int push_sum = 0;
static int pop_counter = 0;
static int pop_sum = 0;
static int push[PE_RING_BUFFER_LENGTH+1] = { 0 };
static struct ovs_mutex pop_lock;

/* the following two values are determined along with
 * PE_RING_BUFFER_LENGTH such that
 * PE_TEST_POP_THREAD_COUNT*PE_TEST_MAX_POP_COUNT=PE_RING_BUFFER_LENGTH,
 * which leaves 1 final entry to pop.
 */
#define PE_TEST_POP_THREAD_COUNT 20
#define PE_TEST_MAX_POP_COUNT 50

typedef struct _arg_data {
    int arg;
    ring_buffer_t *rb;
} arg_data_t;

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

    vlog_set_levels(vlog_module_from_name("test_ring_buffer"), -1, VLL_DBG);

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


//    ring_buffer_destroy(&test_rb);

    free(test_rb.buffer);

    assert_int_equal(pop_sum,push_sum);

    VLOG_LEAVE(__func__);
}

void *pop_off(void *arg) {
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
        
    VLOG_DBG("pop_off tid %p, pop_sum/push_sum %i/%i",
                        pthread_self(),pop_sum,push_sum);
    VLOG_LEAVE(__func__);

    pthread_exit((void *) NULL);
}

void *push_on(void *arg) {
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

int main(void) {
    const UnitTest tests[] = {
    //    unit_test(fill_buffer),
    //    unit_test(consume_buffer),
        unit_test(push_pop_buffer),
    };
    putenv("CMOCKA_TEST_ABORT=1");
    return run_tests(tests);
}
