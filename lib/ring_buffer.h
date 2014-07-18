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
 *  Note: currently this is implemented via static functions. It may make
 *        sense to generalize and make this instansiable (is that a word?)
 */
//#define PE_RING_BUFFER_LENGTH 10
#define PE_RING_BUFFER_LENGTH 1000 /* TODO: eventually make this configurable 
                                    * see pe_config_defaults in policy_enforcer.h
                                    * also, there are dependencies on this
                                    * value in test_ring_buffer.c */
#define PE_RING_BUFFER_ENTRY_SIZE sizeof(void *) /* should this be configurable?
                                                  * to structs that are
                                                  * pushed into ring buf
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
    void **buffer;        
    ring_buffer_counters_t rb_counters;
} ring_buffer_t;

/* public prototypes */
void ring_buffer_init(ring_buffer_t *);
void ring_buffer_destroy(ring_buffer_t *);
void ring_buffer_push(ring_buffer_t *, void *);
void *ring_buffer_pop(ring_buffer_t *);
void rb_broadcast_cond_variables(ring_buffer_t *);

/* gettors */
//inline uint32_t get_ring_buffer_length(void);
//inline uint32_t get_ring_buffer_entry_size(void);

/* settors */
//bool pe_set_ring_buffer_length(uint32_t);

#endif //PEOVS_RING_BUFFER_H
