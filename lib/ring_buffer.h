/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#ifndef PEOVS_RING_BUFFER_H
#define PEOVS_RING_BUFFER_H 1

#include "ovs-thread.h"

/*
 * API for ring buffer
 */

typedef struct _ring_buffer_counters {
    int32_t pop_location;
    int32_t push_location;
    int32_t unused_count;
    struct ovs_mutex  lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ring_buffer_counters_t;

typedef struct _ring_buffer {
    uint32_t length;        /* how many slots */
    uint32_t entry_size;    /* size of each slot */
    ring_buffer_counters_t rb_counters;
    void **buffer;        
} ring_buffer_t;

/* public prototypes */
void ring_buffer_init(ring_buffer_t *);
void ring_buffer_destroy(ring_buffer_t *rb);
void ring_buffer_push(ring_buffer_t *, void *);
void *ring_buffer_pop(ring_buffer_t *);
void rb_broadcast_cond_variables(ring_buffer_t *);

#endif //PEOVS_RING_BUFFER_H
