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

//#define DBUG_OFF 1

#include "pag-thread.h"
#include "dbug.h"
#include "util.h"
#include "misc-util.h"
#include "ring_buffer.h"
#include "vlog.h"
#include "pe_workers.h"

VLOG_DEFINE_THIS_MODULE(pe_workers);

/* private */

static pe_crew_t crew;
static bool pe_get_crew_quit_status(void);
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
void pe_crew_destroy(pe_crew_t *crew) 
{
    static char mod[] = "pe_crew_destroy";
    rb_cond_vars_t *cond_list;
    int counter = 0;

    DBUG_ENTER(mod);
    VLOG_DBG("%s: -->", mod);

    pe_set_crew_quit_status(true);
    rb_broadcast_cond_variables(); //signal threads to wake up and die

    VLOG_DBG("%s: Broadcast for termination.", mod);

    pag_rwlock_destroy(&crew->rwlock);

    DBUG_LEAVE;

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

void pe_crew_create ()
{
    static char mod[] = "pe_crew_create";
    uint32_t crew_index=0;
    int save_errno = 0;

    DBUG_ENTER(mod);

    memset((void *) &crew, 0, sizeof(pe_crew_t));

    crew.size = PE_MAX_WORKER_COUNT;
    pe_set_crew_quit_status(false);

    //VLOG_DBG("%s <-- crew=%p crew_size=%d\n", mod, crew, crew.size);

    /*
     * Initialize synchronization objects
     */
    pag_rwlock_init(&crew.rwlock);

    DBUG_PRINT("DEBUG ",("Set quit status false. Lock initialized %p\n",
                            &crew.rwlock));
  
    /*
     * Create the worker threads.
     */
    crew.worker = xcalloc(crew.size, sizeof(pe_worker_t *));

    for (crew_index = 0; crew_index < crew.size; crew_index++) {

        crew.worker[crew_index] = xmalloc(sizeof(pe_worker_t));

        DBUG_PRINT("DEBUG ",("Starting worker %i with %i bytes at %p\n",
                             crew_index,sizeof(pe_worker_t),
                             crew.worker[crew_index]));

        crew.worker[crew_index]->index = crew_index;

        DBUG_PRINT("DEBUG ",("Worker's index %i recorded as %i\n",
                              crew_index, crew.worker[crew_index]->index));


        if((save_errno = pthread_attr_init(&crew.worker[crew_index]->attr))
           != 0)
            strerr_wrapper(save_errno); //core dump

        if((save_errno = 
            pthread_attr_setdetachstate(&crew.worker[crew_index]->attr,
                                       PTHREAD_CREATE_DETACHED)) != 0)
            strerr_wrapper(save_errno); //core dump

        xpthread_create(&crew.worker[crew_index]->thread,
                        &crew.worker[crew_index]->attr,
                        pe_workers_fetch_flow,
                        (void *) crew.worker[crew_index]);
    }

    DBUG_LEAVE;
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
    pe_worker_t *this_worker = (pe_worker_t *) arg;
    void *work_item = NULL;

    DBUG_ENTER(mod);

    VLOG_INFO("Thread %i (%p) going to work.\n",this_worker->index,
                                                &this_worker->thread); 

    /* infinite loop to process work items */
    for(;;) {
        DBUG_PRINT("\nDEBUG ",("Thread %i (%d) calling ring_buffer_pop\n",
                               this_worker->index,
                               &this_worker->thread));
        work_item = ring_buffer_pop();
        DBUG_PRINT("\nDEBUG ",("Thread %i (%d) popped %p\n",
                               this_worker->index,
                               &this_worker->thread,
                               work_item)); 
        if (pe_get_crew_quit_status() == true) {
            DBUG_PRINT("\nDEBUG ",("Thread %i got quit\n",this_worker->index));
            break;
        }
        pe_translate(work_item);
    }

    VLOG_INFO("Thread %i (%p) leaving work.\n",this_worker->index,
                                                this_worker->thread); 
    DBUG_LEAVE;
    pthread_exit((void *) NULL);
}

/* ============================================================
 *
 * \brief pe_workers_init()
 *        Initializes the worker thread pool.
 *
 * @param[]
 *          none
 *
 * \return { void }
 *
 **/
void pe_workers_init() {
    static char mod[] = "pe_workers_init";

    DBUG_PUSH("d:F:i:L:n:t");
    DBUG_PROCESS("pe_workers");
    DBUG_ENTER(mod);

    ring_buffer_init();

    /* create the crew and put them to sleep on the ring buffer*/
    pe_crew_create();

    DBUG_LEAVE;
}

void pe_set_crew_quit_status(bool status) {
    static char mod[] = "pe_set_crew_quit_status";

    DBUG_ENTER(mod);

    pag_rwlock_wrlock(&crew.rwlock);
    crew.quit = status;
    pag_rwlock_unlock (&crew.rwlock);

    DBUG_LEAVE;
}


/* ============================================================
 *
 * \brief pe_workers_destroy()
 *        Destroys the worker thread pool and frees the buffer.
 *
 * @param[]
 *          none
 *
 * \return { void }
 *
 **/
void pe_workers_destroy() {
    static char mod[] = "pe_workers_destroy";

    DBUG_ENTER(mod);

    pe_crew_destroy(&crew);
    ring_buffer_destroy();

    DBUG_LEAVE;
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

    DBUG_ENTER(mod);

    pag_rwlock_rdlock(&crew.rwlock);
    bret = crew.quit;
    pag_rwlock_unlock (&crew.rwlock);

    DBUG_LEAVE;
    return(bret);
}
