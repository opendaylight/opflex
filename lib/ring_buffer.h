/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#ifndef RING_BUFFER_H
#define RING_BUFFER_H 1

#include "ovs-thread.h"

/*
 * API for ring buffer
 */
//#define PE_RING_BUFFER_LENGTH 10
#define PE_RING_BUFFER_LENGTH 1000 /* eventually make this configurable 
                                    * also, there are dependencies on this
                                    * value in test_ring_buffer.c */
#define PE_RING_BUFFER_ENTRY_SIZE sizeof(void *) /* should be sizeof ptr
                                                  * to structs that are
                                                  * pushed into ring buf
                                                  */

typedef struct _ring_buffer_counters {
    int32_t pop_location;
    int32_t push_location;
    int32_t unused_count;
//    struct ovs_rwlock rwlock;
    struct ovs_mutex  lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ring_buffer_counters_t;

typedef struct _ring_buffer_cond_vars {
    uint32_t count;          /* number of condition variables in struct */
    pthread_cond_t **list;   /* array of cond variables */
} rb_cond_vars_t;

/* public prototypes */
void ring_buffer_init(void);
void ring_buffer_destroy(void);
void ring_buffer_push(void *);
void *ring_buffer_pop(void);

/* gettors */
inline uint32_t get_ring_buffer_length(void);
inline uint32_t get_ring_buffer_entry_size(void);
void rb_broadcast_cond_variables(void);

/* settors */
//bool pe_set_ring_buffer_length(uint32_t);

#endif //RING_BUFFER_H
