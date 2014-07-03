/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* 
 * modb_event.h - the header for the MODB eventing sub-system.
 *
 * History:
 *     28-JUN-2014 dkehn@noironetworks.com - created.
 */
#ifndef MODB_EVENT_H
#define MODB_EVENT_H 1

#include "config-file.h"
#include "ovs-thread.h"
#include "shash.h"
#include "list-util.h"
#include "uri-parser.h"


/* This define the type of object that is being passed back in the structure or
 * what you want back.
 */
typedef enum _enum_mevt_object {
    MEVT_OBJ_NODE = 1,
    MEVT_OBJ_TREE,
    MEVT_OBJ_PROPERTY,
} enum_mevt_object;

/* 
 * this defines the operation that produced the event.
 */
#define MEVT_TYPE_NOP          0x0000
#define MEVT_TYPE_ADD          0x0001
#define MEVT_TYPE_DEL          0x0002
#define MEVT_TYPE_UPD          0x0004
#define MEVT_TYPE_DESTROY      0x0008
#define MEVT_TYPE_ANY          0xffff


/* 
 * These flags define the source of the event, eith from the north or 
 * south interface, or both.
 */
#define MEVT_SRC_INTERNAL      0x0001
#define MEVT_SRC_SOUTH         0x0002
#define MEVT_SRC_NORTH         0x0004
#define MEVT_SRC_ANY           0xffff


/* 
 * modb_event is the structure that holds the event information back 
 * to the subscriber(s).
 */
typedef struct _modb_event {
    /* this defines when this event was created and is used
     * by the mamanger thread to remove events from the queue. 
     */
    struct timeval           timestamp;

    /* defines the level of MODB object this event applies to */
    enum_mevt_object         eobj;

    /* what this event operation is */
    unsigned int             etype;

    /* this is a flag defining the source of the event i.e. 
     * Northbound (policy management) or south (policy enforcer).
     * NOTE: Can be both directions, but can't a direction and 
     * MEVT_FG_SRC_IINTERNAL.
     */
    unsigned int             esrc;

    /* Number of items in the dp_arr. */
    int                      dp_count;

    /* pointer the MODB object that this event will pertain to, can be
     * node_ele_p, attribute_p or many of them.
     */
    void                     *dp_arr[];
} modb_event_t, *modb_event_p;

/*
 * subscriber structure, each subscribe call will add to an array for these
 * structures an entry of the following structure and when an event is being
 * processed from the MODB it will run through the array check each evt_type
 * and evt_source for a match for the given event and if it match will send
 * a pthread_cond_signal on the evt_go and if there is a thread waiting it
 * start processing that event. It is assumed that the event_p is a copy of the
 * event structure and the thread is free to do with it as it chooses, but
 * it is responsibility of each thread to free the event_p with a call to
 * modb_event_free(event_p ) to free the memory allocated.
 */
typedef struct _subscriber {
    /* subscribers thread id from pthread_self(). This is 
     * going to be used as the identifer between subscribers.
     */
    pthread_t             thread_id;

    /* the count of the number of times the evt_go has been
     * signalled.
     */
    long                  evt_counter;

    /* this will be the event type from the MODB, these are flags and 
     * can be OR-ed together, so for example if you wantthis thread to 
     * recieve all events your would do: (MEVT_TYPE_ADD | MEVET_TYPE_DEL |
     * MEVT_TYPE_UPD | .......).
     */
    unsigned int          evt_type;

    /* this is the source of the event, who generated the event */
    unsigned int          evt_source;

    /* the conditional that the subscriber (modb_event_wait) will wait on
     * for an event, the MODB will issue a pthread_signal on this cond.
     */
    pthread_cond_t        go;

    /* the lock for which will keep the writer and subscriber separate.
     */
    struct ovs_mutex      mutex;

    /* Once the subscriber has gotten the event, it should grab the evtp
     * into its local spave and set this to true so the MODB can generate
     * another event.
     */
    bool                  evt_read;

    /* Each time a new evtp is ready this will be set to true and the evt_read
     * set to false. The waiter will set this to false.
     */
    int                   evt_new;

    /* This is the event pointer */
    modb_event_p          evtp;
} subscriber_t, *subscriber_p;


/*
 *  Public protos
 */ 
extern bool modb_event_initialize(void);
extern int modb_event_destroy(void);
extern int modb_event_subscribe(unsigned int evt_type,
                                unsigned int evt_source);
extern int modb_event_wait(modb_event_p *evtp);
extern int modb_event_unsubscribe(unsigned int evt_type,
                                  unsigned int evt_source);
extern void modb_event_dump(void);
extern void modb_event_free(modb_event_p evtp);

#endif
