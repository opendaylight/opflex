/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#define DBUG_OFF 1

#include "ovs-thread.h"
#include "dbug.h"
#include "util.h"
#include "misc-util.h"
#include "ring_buffer.h"
#include "modb-event.h"
#include "vlog.h"
#include "peovs_workers.h"

VLOG_DEFINE_THIS_MODULE(peovs_workers);

/* private */

static pe_crew_t crew;
static bool pe_get_crew_quit_status(void);
static void pe_crew_create(ring_buffer_t *);
static void pe_crew_destroy(ring_buffer_t *rb);
void *pe_workers_fetch_flow(void *arg);

/* ====================================================================
 *
 * @brief pe_crew_destory() 
 *        Free all the memory and kill the specified work crew.
 *
 * @param0 <old_crew>       - I/O
 *         pointer to the crew structure that is to be destroyed
 * @return <status>         - O
 *         0 if sucessful, else the return from the pthread calls.
 *               
 **/
static void pe_crew_destroy(ring_buffer_t *rb) 
{
    static char mod[] = "pe_crew_destroy";
    int crew_index = 0;

    VLOG_ENTER(mod);
    VLOG_DBG("%s: -->", mod);

    pe_set_crew_quit_status(true);
    rb_broadcast_cond_variables(rb); //signal threads to wake up and die

    VLOG_DBG("%s: Broadcast for termination.", mod);

    for (crew_index = 0; crew_index < crew.size; crew_index++)
        xpthread_join(crew.worker[crew_index]->thread, NULL);

    ovs_rwlock_destroy(&crew.rwlock);

    VLOG_LEAVE(__func__);

}

/* ============================================================
 *
 * @brief crew_create()
 *        Create a work crew.
 *
 * @param  <none>- I
 *         the worker thread.
 * @return <status>    - void
 **/

static void pe_crew_create(ring_buffer_t *rb)
{
    static char mod[] = "pe_crew_create";
    uint32_t crew_index=0;
    int save_errno = 0;

    VLOG_ENTER(mod);

    memset((void *) &crew, 0, sizeof(pe_crew_t));

    crew.size = PE_MAX_WORKER_COUNT;

    /*
     * Initialize synchronization objects
     */
    ovs_rwlock_init(&crew.rwlock);
    pe_set_crew_quit_status(false);

    VLOG_DBG("Set quit status false. Lock initialized %p\n",
                            &crew.rwlock);
  
    /*
     * Create the worker threads.
     */
    crew.worker = xcalloc(crew.size, sizeof(pe_worker_t *));

    for (crew_index = 0; crew_index < crew.size; crew_index++) {

        crew.worker[crew_index] = xmalloc(sizeof(pe_worker_t));

        VLOG_DBG("Starting worker %i with %i bytes at %p\n",
                             crew_index,sizeof(pe_worker_t),
                             crew.worker[crew_index]);

        crew.worker[crew_index]->index = crew_index;

        pe_worker_data_t worker_data;
        worker_data.worker_id = crew.worker[crew_index];
        worker_data.rb = rb;

        VLOG_DBG("Worker's index %i recorded as %i\n",
                              crew_index, crew.worker[crew_index]->index);

        pag_pthread_create(&crew.worker[crew_index]->thread,
                        NULL,
                        pe_workers_fetch_flow,
                        (void *) &worker_data);
    }

    VLOG_LEAVE(__func__);
}

/* ============================================================
 *
 * \brief pe_workers_fetch_flow()
 *        Loop for worker thread. The call to ring_buffer_pop()
 *        blocks until there is a work item in the q. A thread
 *        fetches the item and sends it off to the pe_translate
 *        for processing...once translated and sent on its way,
 *        the thread loops back to ring_buffer_pop().
 *
 * @param[]
 *          none
 *
 * \return { void }
 *
 **/

void *pe_workers_fetch_flow(void *arg) {
    static char mod[] = "pe_workers_fetch_flow";
    pe_worker_data_t *this_worker_data = (pe_worker_data_t *) arg;
    pe_worker_t *this_worker = this_worker_data->worker_id;
    ring_buffer_t *rb = this_worker_data->rb;
    void *work_item = NULL;

    VLOG_ENTER(mod);

    VLOG_INFO("Thread %i (%p) going to work.\n",this_worker->index,
                                                &this_worker->thread); 

    /* infinite loop to process work items */
    for(;;) {

//TODO: this will end up using the modb-event.h interface
        work_item = ring_buffer_pop(rb);

        VLOG_DBG("Thread %i (%d) popped %p\n",
                               this_worker->index,
                               &this_worker->thread,
                               work_item); 

        if (pe_get_crew_quit_status() == true) {
            VLOG_INFO("Thread %i got quit message\n",this_worker->index);
            break;
        }
        pe_translate(work_item);
    }

    VLOG_INFO("Thread %i (%p) leaving work.\n",this_worker->index,
                                                this_worker->thread); 
    VLOG_LEAVE(mod);
    pthread_exit((void *) NULL);
}

/* ============================================================
 *
 * \brief pe_workers_init()
 *        Initializes the worker thread pool.
 *
 * @param[]
 *          pointer to ring buffer - space must be allocated by caller
 *
 * \return { void }
 *
 **/
void pe_workers_init(ring_buffer_t *rb) {
    static char mod[] = "pe_workers_init";

    VLOG_ENTER(mod);

    //TODO: replace with modb stuff
    ring_buffer_init(rb);

    /* create the crew and put them to sleep on modb_event_wait() */
    pe_crew_create(rb);

    VLOG_LEAVE(mod);
}

void pe_set_crew_quit_status(bool status) {
    static char mod[] = "pe_set_crew_quit_status";

    VLOG_ENTER(mod);

    ovs_rwlock_wrlock(&crew.rwlock);
    crew.quit = status;
    ovs_rwlock_unlock (&crew.rwlock);

    VLOG_LEAVE(mod);
}


/* ============================================================
 *
 * \brief pe_workers_destroy()
 *        Destroys the worker thread pool and frees the buffer.
 *
 * @param[]
 *          pointer to ring buffer
 *
 * \return { void }
 *
 **/
void pe_workers_destroy(ring_buffer_t *rb) {
    static char mod[] = "pe_workers_destroy";

    VLOG_ENTER(mod);

    pe_crew_destroy(rb);
    //TODO: will become clean up of modb stuff
    ring_buffer_destroy(rb);

    VLOG_LEAVE(mod);
}

/* ============================================================
 *
 * \brief pe_get_crew_quit_status()
 *        Return the value of crew.quit.
 *
 * @param[]
 *          none
 *
 * \return { bool }
 *
 **/
static bool pe_get_crew_quit_status() {
    static char mod[] = "pe_get_crew_quit_status";
    bool bret = false;

    VLOG_ENTER(mod);

    ovs_rwlock_rdlock(&crew.rwlock);
    bret = crew.quit;
    ovs_rwlock_unlock (&crew.rwlock);

    VLOG_LEAVE(mod);
    return(bret);
}
