/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <asm/errno.h>

#include "pag-thread.h"
#include "eventq.h"
#include "uuid.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(eventq);

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Work Q Command lookup structure 
 */
typedef struct _q_cmd { 
    int Cmd; 
    bool (*cmdHandler) (eventq_ele_t *wp,crew_t *crew, int crew_id); 
} q_cmd; 

/* 
 * protos for the jump table 
 */
static bool q_ping_handler(eventq_ele_t *wp, crew_t *crew, int crew_id) ;
 
/*
 * jump table for the worker_routine 
 */
static q_cmd cmdLookup[] = { 
    {SB_PING,        q_ping_handler}, 
    {0, NULL} 
}; 


/* ----------------------------------------------------------------------------
 *
 * @brief scan_queue_for_uuid()
 *        scan_quere_for_uuid routine will scan the items in the workQ
 *        looking for items with the specfied uuid value if found
 *        will return True, if not it will return False.
 *
 * @param0 <crew>                   - I
 *         workq_t pointer to the workQ to scan.
 * @param1 <uuid>                 - I
 *         uuid pointer
 * @return <found>                - O
 *         if an item in wp match the fd in the connect True, Else False.
 *
 **/
bool scan_queue_for_uuid (crew_t *crew, struct uuid *uuid) 
{
    static char mod[] = "scan_queue_for_uuid";
    eventq_ele_t *wp;
    bool retc = false;
    int a;
  
    VLOG_DBG("%s: <--\n",  mod);  
    pag_mutex_lock(&crew->mutex);

    if (crew->work_count > 0) {
        for (a = 0, wp = crew->first; 
             a < crew->work_count && wp->next != NULL; 
             a++,wp=wp->next) {
            if (!bcmp((const void *)uuid, (const void *)&wp->uuid, UUID_OCTET)) {
                retc = true;
                break;
            }
        }
    }     
    pag_mutex_unlock(&crew->mutex);
    VLOG_DBG("%s: -->%d", mod, retc);
    return retc;
}

/* =====================================================================
 * @brief work_routine () 
 *        this is the routine that will operate on a eventq_ele_t item from the 
 *        queue.
 *
 * @param0 <arg>                    - I
 *         pointer to my worker_t area
 *
 **/
void *worker_routine (void *arg) 
{
    static char mod[] = "work_routine";
    worker_p mine = (worker_t*)arg;
    crew_p crew = mine->crew;
    eventq_ele_t *work;
    int dWait = 1;


    VLOG_DBG("%s:%d: <-- arg=%p", mod, mine->index, arg);

    /* 
     * loop here forever processing work
     */ 
    while (1) {
        /* Wait while there is nothing to do, and the hope of something 
         * coming along later. If crew->first is NULL, there's no work. 
         */
    
        if (dWait) {
            VLOG_DBG("%s:%d: Crew waiting for work", mod, mine->index);
            dWait = 0;
        }

        pag_mutex_lock(&crew->mutex);

        /* If there is nothing to do wait for something......
         */
        if (crew->work_count == 0) {
            VLOG_DBG("%s:%d: waiting for work", mod, mine->index);
            pag_mutex_cond_wait(&crew->go, &crew->mutex);
        }

        if (crew->quit == 1) {
            pthread_cond_signal(&crew->done);
            VLOG_DBG("%s:%d: quiting", mod, mine->index);
            crew->crew_size--;
            pag_mutex_unlock(&crew->mutex);
            pthread_exit(NULL);
        }      
      
        if (crew->work_count > 0) {
    
            VLOG_DBG("%s:%ld:%d: Woke: work_count=%d", mod, 
                     pthread_self(), mine->index, crew->work_count);

            /* Remove and process a work item
             */
            work = crew->first;
            crew->first = work->next;
            if (crew->first == NULL)
                crew->last = NULL;

            work->next = NULL;
            crew->work_count--;

            pag_mutex_unlock (&crew->mutex);
	      
            /* ap = &work->aebsRqst; */
            /* /\* We have the work item now process it.... */
            /*  *\/ */
            /* DEBUG_OUT(2, ("(%s)Dispatching on %s \n", mod, getAebsCmdStr(work->aebsRqst.Command))); */

            /* int a; */
            /* for (a=0; cmdLookup[a].Cmd != 0; a++)  */
            /*     if (cmdLookup[a].Cmd == ap->Command) {  */
            /*         cmdLookup[a].cmdHandler(work, crew, mine->index);     /\* execute the command *\/ */
            /*         dWait = 1; */
            /*         break; */
            /*     } */
        }
        else {
            pag_mutex_unlock(&crew->mutex);
        }
    }

    VLOG_DBG("%s: -->", mod);
    return NULL;
}

/**********************************************************************
 * HANDLERS
 ********************************************************************/
  
/* ==================================================================
 * @brief q_ping_handler() 
 *        
 *
 * @param0 <wp>                       - I/O
 *         eventq_ele_t pointer, holds everything I need to process 
 * @param1 <crew>                     - I
 *         crew structure
 * @param2 <crew_id>                  - I
 *         id (index) of the worker.
 * @return <status>
 *         0 for sucess, else !0
 *
 **/
static bool q_ping_handler (eventq_ele_t *wp, crew_t *crew, int crew_id) 
{
    static char mod[] = "q_ping_handle";

    VLOG_DBG("%s: <--%d", mod, crew_id);
    VLOG_DBG("%s: -->", mod);
    return (0);
}


/***************************************************************************
 * WORK Q & POOL Routines
 **************************************************************************/

/* ===================================================================
 *
 * @brief init_workpool()
 *        Builds up the workpool, which is just a large
 *        memory are of workq_ele that are drawn from for 
 *        the work queue.
 *
 * @param0 <cmd_size>       - I
 *          the size of a cmd that will end up on the queue
 * @param1 <ele_size>       - I
 *         the size of an element (work) that will be in the queue
 * @param2 <cmd_per_rqst>   - I
 *         number of cmds per rqst (cmd queuing), generally 1.
 * @param3 <qsize>          - I
 *         The size in lelement of the queue, the number of eeventq_t 
 *         that are created.
 * @param4 <eqp>            - I/O
 *         Return an initialized eventq_p (struct _eventq *), NULL if an
 *         non-out-of-memory error occurrs.
 * @returns <retc>          - O
 *         0 upon sucsess, else !0
 *
 */
bool init_workpool(int cmd_size, int ele_size, 
                   int cmd_per_request, int qsize, eventq_p *eqp) 
{
    static char mod[] = "init_workpool";
    eventq_ele_t *this, *next;
    int a;
    bool retc = 0;

    VLOG_DBG("%s: <--cmd_size:%d ele_size:%d qsize:%d ",
             mod, cmd_size, ele_size, qsize);

    /* init the queue */
    *eqp = xzalloc(sizeof(eventq_t));
    pag_mutex_init(&(*eqp)->mutex);
    (*eqp)->first = (*eqp)->last = NULL;
    (*eqp)->num_in_q = 0;
    (*eqp)->q_size = 0;
    
    /*  build up the buffer pool  */
    pag_mutex_lock(&(*eqp)->mutex);

    /* 
     * setup the link list ptrs
     */
    this = next = NULL;

    for (a=0; a < qsize; a++) {    
        next = (eventq_ele_t *)xzalloc(ele_size);
        memset(next, 0, ele_size);
        next->cmd_size = cmd_size;
        (*eqp)->q_size++;
        (*eqp)->num_in_q++;

        if (a == 0) {
            (*eqp)->first = next;
        }
        else
            this->next = next;

        if (a == (qsize - 1)) {
            next->next = NULL;
            (*eqp)->last = next;
        }
        this = next;
    }

    pag_mutex_unlock(&(*eqp)->mutex);
    VLOG_DBG("%s: -->%d", mod, retc);
    return(retc);
}

/* ==========================================================================
 * @brief get_event_from_queue()
 *        Gets a free work element from the Pool, marks it as
 *        not available (isFree = False) and returns a pointer
 *        for it.
 *
 * @return <wap>     - O
 *         Pointer of a work element from the pool, else NULL
 *         if none are available or lock error. Will display
 *         the error message from here.
 *
 **/
eventq_ele_t *get_event_from_queue(eventq_t *qp) 
{
    static char mod[] = "get_event_from_queue";
    eventq_ele_t *wap;
    int count;

    VLOG_DBG("%s: <--", mod);
    /* 
     * very dangerous, we will stay in theis loop if 
     * while there are no more free work_ele available.
     */
    pag_mutex_lock(&qp->mutex);

    if (qp->num_in_q > 0) {
        if ((wap = qp->first) != NULL) {
            /* int a; */
            /* struct iovec *ip; */
      
            /* take the first one from the queue 
             */
            qp->first = wap->next;
            --qp->num_in_q;
            count = qp->num_in_q;
      
            pag_mutex_unlock(&qp->mutex);
            VLOG_DBG("%s: Returning with %p num_in_q %d UNLOCKED",
                     mod, wap, count);

            wap->next = NULL;	
            /* wap->bhead = NULL; */
            /* wap->numBlks = 0; */

            /* Make sure all iovecs are reset       */
            /* for(a=0, ip=wap->iov_p; a < wap->iovAllocated; a++, ip++) { */
            /*     ip->iov_base = NULL; */
            /*     ip->iov_len = 0; */
            /* } */
            /* wap->iovCnt = 0; */

            VLOG_DBG("%s: eventq_t.mutex UNLOCKED", mod);
            return (wap);	
        } else {
            VLOG_ERR("%s: The event queue is out of sync showing a count, "
                     "but next is NULL", mod);
        }
    } else {
        VLOG_DBG("%s: No Elements left in the event queue\n", mod);
    }
    
    pag_mutex_unlock(&qp->mutex);
    VLOG_DBG("%s: -->", mod);
    return NULL;    
}

/* ==========================================================================
 * @brief put_work_back_in_queue()
 *        puts a work_ele back in the pool. If the iov_p has any memory
 *        attached to it it will leave it there to be recycled by the next
 *        eventq_ele_t requester
 *
 * @param0 <wp>              - I
 *         eventq_ele_t pointer to the work element.
 * @return <status>          - O
 *         0 if everything ok, else !0.
 *
 **/
bool put_work_back_in_queue (eventq_t * qp, eventq_ele_t *wp ) 
{
    static char mod[] = "put_work_back_in_queue";
    bool retc = 0;
    int count;
    /* struct iovec *ip; */
    /* int a; */

    VLOG_DBG("%s: <--", mod);
    
    /* Reset all iovecs
     */
    /* for(a=0, ip=wp->iov_p; a < wp->iovAllocated; a++, ip++) { */
    /*     ip->iov_base = NULL; */
    /*     ip->iov_len = 0; */
    /* } */
    /* wp->iovCnt = 0; */

    /* buffer blocks to NULL */
    /* wp->bhead = NULL;  */
    /* wp->numBlks = 0; */

    pag_mutex_lock(&qp->mutex);

    //DEBUG_OUT(THIS_DB_LEVEL, ("(%s) Called LOCKED :num_in_q=%d \n", 
    //    mod,qp->.num_in_q));

    /* back in the pool @ the end
     */
    qp->last->next = wp;
    qp->last = wp;
    bzero((void *)&wp->uuid, UUID_OCTET);
    wp->next = NULL;
    ++qp->num_in_q;
    count = qp->num_in_q;
  
    /* relase the lock */
    pag_mutex_unlock(&qp->mutex);
    wp = NULL;
    VLOG_DBG("%s: -->%d", mod, retc);
    return retc;    
}
 
/* ====================================================================
 *
 * @brief eventq_add()
 *        Add an item to a work queue. If the work_count was 0 a
 *        signal is sent to the threads that makeup the crew to get
 *        them going.
 *
 * @param0 <crew>      - I
 *         Pointer to the crew structure that holds the work Q.
 * @param1 <request>   - I
 *         Pointer to a work element that is to be added to the Q
 * @return <status>    - O
 *         0 if sucessful, else error return from the pthread
 *         calls
 *
 */
int eventq_add (crew_p crew, eventq_ele_t *request)
{
    static char mod[] = "eventq_add";
    int status = 0;
    int wcnt;
    pthread_t me = pthread_self();
  
    VLOG_DBG("%s:%ld) <--Called crew_size=%d work request=%p\n", 
             mod, me, crew->crew_size, request);

    /* 
     * get ahold of the crew.
     */
    pag_mutex_lock(&crew->mutex);

    /*
     * Add the request to the end of the queue, updating the
     * first and last pointers.
     */
    if (crew->first == NULL) {
        crew->first = request;
        crew->last = request;
    } else {
        crew->last->next = request;
        crew->last = request;
    }
  
    wcnt = ++crew->work_count;  
    pag_mutex_unlock(&crew->mutex);

    if ((wcnt - 1) == 0) {
        status = pthread_cond_signal(&crew->go);
        VLOG_DBG("%s:%ld: Sending the signal to GO! work_count=%d",
                                  mod, me, wcnt);
        if (status) {
            VLOG_ERR("%s: Error sending the signal: %s", 
                     mod, strerror(status));
        }
    }

    VLOG_DBG("%s:%ld: -->Work_count=%d", mod, me, wcnt);
    return(status);
}


/* ====================================================================
 *
 * @brief crew_destory() 
 *        Free all the memory and kill the specified work crew.
 *
 * @param0 <old_crew>       - I/O
 *         pointer to the crew structure that is to be destroyed
 * @return <status>         - O
 *         0 if sucessful, else the return from the pthread calls.
 *               
 **/
int crew_destroy(eventq_t *qp, crew_t *crew) 
{
    static char mod[] = "crew_destroy";
    int status;
    int count;

    VLOG_DBG("%s: -->", mod);

    pag_mutex_lock(&crew->mutex);
    crew->quit = 1;
    pag_mutex_unlock (&crew->mutex);
    pthread_cond_broadcast(&crew->go);

    /*
     * If the crew is busy, wait for them to finish.
     */
    /* for (count=0; count < crew->crew_size; count++) { */
    /*     pthread_cancel(crew->crew[count].thread); */
    /*     pthread_detach(crew->crew[count].thread); */
    /*     VLOG_DBG("%s: terminated thread: %d", mod, count); */
    /* } */
    while (crew->crew_size > 0) {
        if (crew->work_count == 0) {
            pthread_cond_broadcast(&crew->go);
            VLOG_DBG("%s: Issued Go for termination.", mod);
        }
        //pag_mutex_cond_wait(&crew->done, &crew->mutex);
    }

    /* pag_mutex_lock(&qp->mutex); */
    /* pag_mutex_unlock (&crew->mutex); */
    /* pag_mutex_unlock (&qp->mutex); */

    /* pag_mutex_destroy(&qp->mutex); */
    pag_mutex_destroy(&crew->mutex);
    status = pthread_cond_destroy(&crew->done);
    status = pthread_cond_destroy(&crew->go);

    VLOG_DBG("%s: -->%d", mod, status);
    return (status);
}

/* ============================================================
 *
 * @brief crew_create()
 *        Create a work crew.
 *
 * @param0 <crew>      - I
 *         pointer to the crew pointer to the caller allocated area
 *         to create the crew information.
 * @param1 <crew_size> - I
 *         Number of threads in the crew to be created.
 * @param2 <max_crew_size> - I
 *         When adding worker threads (crew) is will not exceed
 *         max_crew_size.
 * @param3 <worker_routine> - I
 *         the worker thread.
 * @return <status>    - O
 *         0 if successful, else status will erflect a system error
 *         code.
 **/
static pthread_attr_t wc_attr;

bool crew_create (crew_t *crew, int crew_size, int max_crew_size, 
                 void *(*worker_routine)(void *args))
{
    static char mod[] = "crew_create";
    int crew_index;
    int retc = 0;

    VLOG_DBG("%s <-- crew=%p crew_size=%d\n", mod, crew, crew_size);

    /*
     * We won't create more than CREW_SIZE members
     */
    if (crew_size > max_crew_size) {
        VLOG_ERR("%s:Number of thread to create %d exceeds %d.\n", 
                 mod, crew_size, max_crew_size);
        retc = EINVAL;
        goto rtn_return;
    }
  
    crew->crew_size = crew_size;
    crew->work_count = 0;
    crew->first = NULL;
    crew->last = NULL;
    crew->quit = 0;
    
    /*
     * Initialize synchronization objects
     */
    pag_mutex_init(&crew->mutex);
    retc = pthread_cond_init (&crew->done, NULL);
    if (retc != 0) goto rtn_return;
    retc = pthread_cond_init (&crew->go, NULL);
    if (retc != 0) goto rtn_return;
  
    /*
     * Create the worker threads.
     */
    crew->crew = xzalloc(max_crew_size * sizeof(worker_t));
    for (crew_index = 0; crew_index < crew_size; crew_index++) {
        crew->crew[crew_index].index = crew_index;
        crew->crew[crew_index].crew = crew;

        retc = pthread_attr_init(&wc_attr);
        /* retc = pthread_attr_setdetachstate(&wc_attr,  */
        /*                                    PTHREAD_CREATE_DETACHED); */
        if (retc == 0)
            retc = pthread_create (&crew->crew[crew_index].thread,
                                   NULL, worker_routine, (void*)&crew->crew[crew_index]);
        if (retc != 0) {
            VLOG_ERR("%s: Can't create thread %d crew thread: %s\n", 
                     mod, crew_index, strerror(retc));
        }
    }
 rtn_return:
    return(retc);
}

