/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/
#ifndef PE_WORKERS_H
#define PE_WORKERS_H 1

#include <stdbool.h>

#include "config-file.h"

#define PE_MAX_WORKER_COUNT 10 /* needs to be a config variable
                                * pe_config_defaults
                                */

/* Each of the following depends on the other, so these come first */
typedef struct _pe_worker pe_worker_t;
typedef struct _pe_crew pe_crew_t;

/* One of these is initialized for each worker thread in the
 * crew. It contains the "identity" of each worker.
 *  */
struct _pe_worker {
    int                 index;          /* Thread's index */
    pthread_t           thread;         /* Thread */
    pthread_attr_t      attr;           /* Attribute for detaching */
};

/*
 * The crew
 */
struct _pe_crew {
  int                 size;           /* Number of crew members */
  pe_worker_t         **worker;       /* ptr to array of workers
                                       * (crew members) */
  bool                quit;           /* should thread quit?*/
  struct ovs_rwlock   rwlock;         /* rw lock for crew data */
  pthread_cond_t      done;           /* will indicate when we are done */
};

/* prototypes */
void pe_workers_destroy(void);
void pe_workers_init(void);
void pe_set_crew_quit_status(bool);   /* set this to true and all workers
                                       * will terminate gracefully.
                                       */

#endif //PE_WORKERS_H
