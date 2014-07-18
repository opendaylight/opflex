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

VLOG_DEFINE_THIS_MODULE(peovs_ring_buffer);

/* Ring buffer size (set at run time?) */
/*
inline uint32_t get_ring_buffer_length()
{
    return ( (uint32_t) PE_RING_BUFFER_LENGTH );
}
inline uint32_t get_ring_buffer_entry_size()
{
    return ( (uint32_t) PE_RING_BUFFER_ENTRY_SIZE );
}
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


/* ============================================================
 *
 * \brief ring_buffer_destroy(ring_buffer_t *)
 *        Destroy the ring buffer.
 *
 * @param[in]
 *          pointer to the ring_buffer_t
 *
 * \return { void }
 *
 **/

void ring_buffer_destroy(ring_buffer_t *rb) {
    int count = 0;
    uint32_t len = rb->length;
    void *buf = NULL;

    VLOG_ENTER(__func__);

    count = 0;
    while(count < len) {
        buf = *rb->buffer;
        *rb->buffer++;
        count++;
        free(buf);
    }
        
    free(rb);

    VLOG_LEAVE(__func__);
}

/* ============================================================
 *
 * \brief ring_buffer_init(ring_buffer_t *)
 *        Initialize the ring buffer.
 *
 * @param[in]
 *          pointer to a ring_buffer_t - caller is responsible
 *          for allocating memory
 *
 * \return { void; all error codes are trapped elsewhere }
 *
 **/
void ring_buffer_init(ring_buffer_t *rb) {

    VLOG_ENTER(__func__);

    rb->rb_counters.pop_location = 0;
    rb->rb_counters.push_location = 0;
    rb->rb_counters.unused_count = rb->length;

    rb->buffer = xcalloc((size_t) rb->length, (size_t) rb->entry_size);

    ovs_mutex_init(&rb->rb_counters.lock);
    xpthread_cond_init(&rb->rb_counters.not_empty,NULL);
    xpthread_cond_init(&rb->rb_counters.not_full,NULL);

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
    rb->rb_counters.push_location++;
    if(rb->rb_counters.push_location >= rb->length)
            rb->rb_counters.push_location = 0;
    xpthread_cond_signal(&rb->rb_counters.not_empty);
    ovs_mutex_unlock(&rb->rb_counters.lock);

    VLOG_DBG("Pushed %p (%i) into slot %i",
                  input_p, *(int *)input_p, (rb->rb_counters.push_location-1));
    VLOG_DBG("Pushed %p into slot %i",
                  input_p, (rb->rb_counters.push_location-1));

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
    rb->rb_counters.pop_location++;
    if(rb->rb_counters.pop_location >= rb->length)
        rb->rb_counters.pop_location = 0;
    xpthread_cond_signal(&rb->rb_counters.not_full);
    ovs_mutex_unlock(&rb->rb_counters.lock);

    VLOG_DBG("Fetched %p (%i) from slot %i",
             retval, *(int*)retval, (rb->rb_counters.pop_location-1));
    VLOG_DBG("Fetched %p from slot %i",
             retval, (rb->rb_counters.pop_location-1));

    VLOG_LEAVE(__func__);
    return(retval);
}
