/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

/*
* Ring buffer management code
*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#include "ovs-thread.h"
#include "vlog.h"
#include "util.h"
#include "ring_buffer.h"

VLOG_DEFINE_THIS_MODULE(ring_buffer);

/* ============================================================
 *
 * \brief ring_buffer_init(ring_buffer_t *)
 *        Initialize the ring buffer.
 *
 * @param[in]
 *          pointer to a ring_buffer_t - caller is responsible
 *          for allocating memory for the ring_buffer_t, as well
 *          as the buffer itself
 *
 * \return { void; all error codes are trapped elsewhere }
 *
 **/
void ring_buffer_init(ring_buffer_t *rb) {

    VLOG_ENTER(__func__);

    /* Enable debugging
    vlog_set_levels(vlog_module_from_name("ring_buffer"), -1, VLL_DBG);
    */
    vlog_set_levels(vlog_module_from_name("ring_buffer"), -1, VLL_INFO);

    rb->rb_counters.pop_location = 0;
    rb->rb_counters.push_location = 0;
    rb->rb_counters.unused_count = rb->length;

    ovs_mutex_init(&rb->rb_counters.lock);
    xpthread_cond_init(&rb->rb_counters.not_empty,NULL);
    xpthread_cond_init(&rb->rb_counters.not_full,NULL);

    VLOG_LEAVE(__func__);
}

void ring_buffer_destroy(ring_buffer_t *rb) {

    VLOG_ENTER(__func__);

    ovs_mutex_destroy(&rb->rb_counters.lock);
    xpthread_cond_destroy(&rb->rb_counters.not_empty);
    xpthread_cond_destroy(&rb->rb_counters.not_full);

    VLOG_LEAVE(__func__);
}

/* ============================================================
 *
 * \brief ring_buffer_push(ring_buffer_t *, void *)
 *        Push an entry into the ring buffer
 *
 * @param[in]
 *
 *          rb: the pointer to the ring buffer
 *
 *          input_p: the pointer to put into the ring buffer
 *
 * \return { void; all error codes are trapped elsewhere }
 *
 **/
void ring_buffer_push(ring_buffer_t *rb, void *input_p) {
    
    VLOG_ENTER(__func__);

    ovs_mutex_lock(&rb->rb_counters.lock);
    while(((rb->rb_counters.push_location +1) % rb->length) ==
            rb->rb_counters.pop_location) {
        ovs_mutex_cond_wait(&rb->rb_counters.not_full,&rb->rb_counters.lock);
    }
    rb->buffer[rb->rb_counters.push_location] = input_p;

    if (VLOG_IS_DBG_ENABLED() == true) {
    /* The first dbg output statement is for the test-ring-buffer code
     * which uses ints in its data. The second is more general purpose.
     * If you turn on debugging, you should probably turn off the first.
     */
        VLOG_DBG("RB(%p) Pushed %p (%i) into slot %i", rb,
                  input_p, *(int *)input_p, rb->rb_counters.push_location);
//      VLOG_DBG("RB(%p) Pushed %p into slot %i", rb,
//                  input_p, (rb->rb_counters.push_location-1));
    }

    rb->rb_counters.push_location++;
    if(rb->rb_counters.push_location >= rb->length)
            rb->rb_counters.push_location = 0;
    xpthread_cond_signal(&rb->rb_counters.not_empty);
    ovs_mutex_unlock(&rb->rb_counters.lock);

    VLOG_LEAVE(__func__);
}

/* ============================================================
 *
 * \brief ring_buffer_pop(ring_buffer_t *)
 *        Remove an entry from the ring buffer.
 *
 * @param[in]
 *          pointer to the ring buffer
 *
 * \return { void * containing the pointer retrieved from the buffer }
 *
 **/
void *ring_buffer_pop(ring_buffer_t *rb) {
    void *retval = NULL;

    VLOG_ENTER(__func__);

    VLOG_DBG("Thread %p entering ring_buffer_pop", pthread_self());

    ovs_mutex_lock(&rb->rb_counters.lock);
    while(rb->rb_counters.push_location == rb->rb_counters.pop_location) {
        ovs_mutex_cond_wait(&rb->rb_counters.not_empty, &rb->rb_counters.lock);
    }
    retval = rb->buffer[rb->rb_counters.pop_location];

    if (VLOG_IS_DBG_ENABLED() == true) {
    /* The first dbg output statement is for the test-ring-buffer code
     * which uses ints in its data. The second is more general purpose.
     * If you turn on debugging, you should probably turn off the first.
     */
        VLOG_DBG("RB(%p) Fetched %p (%i) from slot %i", rb,
             retval, *(int*)retval, rb->rb_counters.pop_location);
//      VLOG_DBG("RB(%p) Fetched %p from slot %i", rb,
//             retval, rb->rb_counters.pop_location);
    }

    rb->rb_counters.pop_location++;
    if(rb->rb_counters.pop_location >= rb->length)
        rb->rb_counters.pop_location = 0;
    xpthread_cond_signal(&rb->rb_counters.not_full);
    ovs_mutex_unlock(&rb->rb_counters.lock);

    VLOG_LEAVE(__func__);
    return(retval);
}

/* helper function for waking up all threads, usually to tell them
 * to quit.
 */
void rb_broadcast_cond_variables(ring_buffer_t *rb) {
    int save_errno = 0;

    if ((save_errno = pthread_cond_broadcast(&rb->rb_counters.not_full))
         != 0)
        ovs_abort(save_errno,"pthread_cond_broadcast failed!");
    if ((save_errno = pthread_cond_broadcast(&rb->rb_counters.not_empty))
         != 0)
        ovs_abort(save_errno,"pthread_cond_broadcast failed!");

}
