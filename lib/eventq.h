/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef EVENT_Q_H
#define EVENT_Q_H 1

#include <config.h>
#include <sys/time.h>

#include "pag-thread.h"
#include "timeval.h"
#include "uuid.h"


#define EVENT_QUEUE_SIZE 1024

/* 
 * q_cmd - commands
 */

enum q_cmd_types {
    SB_PING
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The element of an event
 */
typedef struct _eventq_ele {
    struct uuid        uuid;     /* UID for reference.          */
    struct _eventq_ele *next;    /* linkto the next item in q   */
    uint32_t           state;    /* state flag                  */
    struct timeval     tv;       /* performance data timeings   */
    int                cmd_size;
    void               *dp;      /* data ptr                    */
} eventq_ele_t;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * the event Q external interface.
 **/
typedef struct _eventq {
    struct pag_mutex mutex;             /* control over the queue.     */
    eventq_ele_t     *first, *last; 
    int              num_in_q;          /* number of free ele in the q  */
    int              q_size;            /* total number of ele in the q */
} eventq_t, *eventq_p;
    
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * One of these is initialized for each worker thread in the
 * crew. It contains the "identity" of each worker.
 */
typedef struct worker_tag {
    int                 index;          /* Thread's index */
    pthread_t           thread;         /* Thread for stage */
    struct crew_tag     *crew;          /* Pointer to crew */
} worker_t, *worker_p;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The work queue
 */
typedef struct crew_tag {
  int                 crew_size;      /* Size of array */
  worker_t            *crew;          /* ptr to workers (crew) members */
  int                 work_count;     /* Count of work items */
  eventq_ele_t        *first, *last;  /* First & last work item */
  int                 quit;           /* thread should quit */
  struct pag_mutex    mutex;          /* Mutex for crew data */
  pthread_cond_t      go;             /* work in Q      */
  pthread_cond_t      done;           /* will indicate when we are done */
} crew_t, *crew_p;


/*
 * Protos
 */

extern void *worker_routine(void *arg);
extern bool init_workpool(int cmd_size, int ele_size, 
                          int cmd_per_request, int qsize, eventq_p *eqp);
extern eventq_ele_t *get_event_from_queue(eventq_t *qp);
extern bool put_work_back_in_queue(eventq_t *qp, eventq_ele_t *wp);
extern bool crew_create(crew_t *crew, int crew_size, int max_crew_size,
                       void *(*worker_routine)(void *args));
extern int workq_init(int num_threads);
extern bool scan_queue_for_uuid(crew_t *crew, struct uuid *uuid);
extern int crew_destroy(eventq_t *qp, crew_t *old_crew);
extern int eventq_add(crew_p crew, eventq_ele_t *request);

#endif /* end EVENT_Q_H */
