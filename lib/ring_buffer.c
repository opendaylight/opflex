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

//#define DBUG_OFF 1 //turn off debugging

#include "pag-thread.h"
#include "dbug.h"
#include "util.h"
#include "ring_buffer.h"


static void **ring_buffer; /* instead of void * this should be correct ptr
                            * type?
                            */

static ring_buffer_counters_t rb_counters;   
static uint32_t rb_length = 0;
static uint32_t rb_entry_size = 0;

/* Ring buffer size (set at run time?) */
inline uint32_t get_ring_buffer_length()
{
    return ( (uint32_t) PE_RING_BUFFER_LENGTH );
}
inline uint32_t get_ring_buffer_entry_size()
{
    return ( (uint32_t) PE_RING_BUFFER_ENTRY_SIZE );
}
void rb_broadcast_cond_variables() {
    int save_errno = 0;

    if ((save_errno = pthread_cond_broadcast(&rb_counters.not_full))
         != 0)
        pag_abort(save_errno,"pthread_cond_broadcast failed!");
    if ((save_errno = pthread_cond_broadcast(&rb_counters.not_empty))
         != 0)
        pag_abort(save_errno,"pthread_cond_broadcast failed!");

}

/* Private functions */

/* ============================================================
 *
 * \brief ring_buffer_destroy()
 *        Destroy the ring buffer.
 *
 * @param[in]
 *          none
 *
 * \return { void }
 *
 **/

void ring_buffer_destroy() {
    DBUG_ENTER("ring_buffer_destroy");
    free(ring_buffer);
    DBUG_LEAVE;
}

/* ============================================================
 *
 * \brief ring_buffer_init()
 *        Initialize the ring buffer.
 *
 * @param[in]
 *          none
 *
 * \return { void; all error codes are trapped elsewhere }
 *
 **/
void ring_buffer_init() {

    DBUG_PUSH("d:F:i:L:n:t");
    DBUG_PROCESS("ring_buffer");
    DBUG_ENTER("ring_buffer_init");

    rb_length = get_ring_buffer_length();
    rb_entry_size = get_ring_buffer_entry_size();

    rb_counters.pop_location = 0;
    rb_counters.push_location = 0;
    rb_counters.unused_count = rb_length;

    ring_buffer = xcalloc((size_t) rb_length, (size_t) rb_entry_size);

    pag_mutex_init(&rb_counters.lock);
    xpthread_cond_init(&rb_counters.not_empty,NULL);
    xpthread_cond_init(&rb_counters.not_full,NULL);

    DBUG_LEAVE;
}

/* ============================================================
 *
 * \brief ring_buffer_push()
 *        Push an entry into the ring buffer
 *
 * @param[in]
 *          input_p: the pointer to put into the ring buffer
 *
 * \return { void; all error codes are trapped elsewhere }
 *
 **/
void ring_buffer_push(void *input_p) {
    
    DBUG_ENTER("ring_buffer_push");

    pag_mutex_lock(&rb_counters.lock);
    while(((rb_counters.push_location +1) % rb_length) ==
            rb_counters.pop_location) {
        pag_mutex_cond_wait(&rb_counters.not_full,&rb_counters.lock);
    }
    ring_buffer[rb_counters.push_location] = input_p;
    rb_counters.push_location++;
    if(rb_counters.push_location >= rb_length)
            rb_counters.push_location = 0;
    xpthread_cond_signal(&rb_counters.not_empty);
    pag_mutex_unlock(&rb_counters.lock);

    DBUG_PRINT("\nDEBUG", ("Pushed %p (%i) into slot %i",
                  input_p, *(int *)input_p, (rb_counters.push_location-1)));

    DBUG_LEAVE;
}

/* ============================================================
 *
 * \brief ring_buffer_pop()
 *        Remove an entry from the ring buffer.
 *
 * @param[]
 *          none
 *
 * \return { void * containing the pointer retrieved from the buffer }
 *
 **/
void *ring_buffer_pop(void) {
    void *retval = NULL;

    DBUG_ENTER("ring_buffer_pop");

    DBUG_PRINT("\nDEBUG", ("Thread %p entering ring_buffer_pop",
                            pthread_self()));

    pag_mutex_lock(&rb_counters.lock);
    while(rb_counters.push_location == rb_counters.pop_location) {
        pag_mutex_cond_wait(&rb_counters.not_empty, &rb_counters.lock);
    }
    retval = ring_buffer[rb_counters.pop_location];
    rb_counters.pop_location++;
    if(rb_counters.pop_location >= rb_length) rb_counters.pop_location = 0;
    xpthread_cond_signal(&rb_counters.not_full);
    pag_mutex_unlock(&rb_counters.lock);

    DBUG_PRINT("\nDEBUG", ("Fetched %p (%i) from slot %i",
                         retval, *(int*)retval, (rb_counters.pop_location-1)));

    DBUG_LEAVE;
    return(retval);
}
