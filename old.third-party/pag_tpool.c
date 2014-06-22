/* 
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>

#include "list.h"
#include "latch.h"
#include "util.h"
#include "pag_tpool.h"
#include "pag-thread.h"
#include "errno.h"


/*
 * Data types
 */

/* Thread pool thread/worker */
struct tpool_thread {
    char *name;
    struct pag_tpool *boss;

    struct pag_mutex mutex;            /* Mutex guarding the following. */
    struct list task_q PAG_GUARDED;
    size_t n_wqe PAG_GUARDED;

    pthread_t thread;
    pthread_cond_t wake_cond;          /* Wakes 'thread' while holding
                                          'mutex'. */
};

/* Thread pool */
struct pag_tpool {
    int n_workers;
    int max_qlen;
    size_t n_batch;
    struct tpool_thread *workers;
    struct latch exit_latch;           /* Tells child threads to exit. */

    /*
     *  Simple round-robin assignment for now.  This
     *  can be changed to support multiple strategies
     *  later if there's a need for it.
     */
    int next_worker;

    /* 
     * TBD: do we want some form of cookie, which can be used
     *      to give as a hint for attempting to schedule jobs
     *      from the same source on a single thread in the pool
     *      (i.e. per-source serialization)?
     */
};

/*
 * Linked list of Work Queue Elements (WQEs). This allows
 * for the threads to queue up multiple WQEs for processing
 */
struct tpool_wqe {
    struct list task_node;

    /* The user-provided call for work to do */
    void * (*do_work)(void *arg);

    /* type-less variable, used for passing argument to do_work */
    void *work_arg;
};

/*
 * Forward Declarations
 */
static void *tpool_worker(void *arg);
static void batch_tasks(struct tpool_thread *worker, struct list *task_q );

/*
 * Methods
 */
static void
batch_tasks(struct tpool_thread *worker, struct list *task_q)
{
    struct tpool_wqe *tpool_work, *next;
    LIST_FOR_EACH_SAFE (tpool_work, next, task_node, task_q) {
        tpool_work->do_work(tpool_work->work_arg);
        list_remove(&tpool_work->task_node);
        free(tpool_work);
    }
}

static void *
tpool_worker(void *arg)
{
    struct tpool_thread *worker = arg;

    worker->name = xasprintf("thread_%u", pagthread_id_self());
    set_subprogram_name("%s", worker->name);

    while (!latch_is_set(&worker->boss->exit_latch)) {
        struct list wqe = LIST_INITIALIZER(&wqe);
        size_t i;
                
        pag_mutex_lock(&worker->mutex);

        /* 
         * Qualify wait with check to make sure boss is not joining 
         * on the worker threads.  
         */
        if (!worker->n_wqe
            && !latch_is_set(&worker->boss->exit_latch)) {
            pag_mutex_cond_wait(&worker->wake_cond, &worker->mutex);
        }                 
        
        /*
         * Collect up to batch amount of work requests to avoid adding 
         * (presumably variable) request processing time to the amount 
         * of time that the mutex is held.
         */
        for (i = 0; i < worker->boss->n_batch; i++) {
            if (worker->n_wqe) {
                worker->n_wqe--;
                list_push_back(&wqe, list_pop_front(&worker->task_q));
            } else {
                break;
            }
        }
        pag_mutex_unlock(&worker->mutex);

        /* execute the batched tasks */
        batch_tasks(worker, &wqe);
    }   
    return 0;
}

struct pag_tpool *
pag_tpool_create(int n_workers, int max_qlen, int n_batch )
{
    struct pag_tpool *boss;
    int i;

    boss = xzalloc(sizeof(*boss));
    boss->n_workers = n_workers;
    boss->max_qlen = max_qlen;
    boss->n_batch = n_batch;
    boss->workers = xzalloc(sizeof(*boss->workers)*n_workers);

    latch_init(&boss->exit_latch);

    for(i=0; i < n_workers; i++ ) {
        struct tpool_thread *worker = &boss->workers[i];
        list_init(&worker->task_q);
        worker->n_wqe = 0;
        worker->boss = boss;
        xpthread_cond_init(&worker->wake_cond, NULL);
        pag_mutex_init(&worker->mutex);
        xpthread_create(&worker->thread, NULL, tpool_worker,
                        worker);
    }
    boss->next_worker = 0;

    return boss;
}

static inline struct tpool_thread *
assign_worker(struct pag_tpool *pool, void *id)
{
    /* TODO: Not using id for now */
    return &pool->workers[pool->next_worker % pool->n_workers];
}

int
pag_tpool_add_work(struct pag_tpool *pool, void *id,
                   void * (*work_func)(void *), void *work_arg)
{
    struct tpool_thread *worker;
    struct tpool_wqe *wqe;
    size_t n_bytes, left;
    int error = 0;

    if (!pool || !work_func)
        return -EINVAL;

    wqe = xmalloc(sizeof *wqe);
    wqe->do_work  = work_func;
    wqe->work_arg = work_arg;

    worker = assign_worker ( pool, id );

    pag_mutex_lock(&worker->mutex);
    if (worker->n_wqe < pool->max_qlen) {
        list_push_back(&worker->task_q, &wqe->task_node);
        worker->n_wqe++;
        pool->next_worker++;
        xpthread_cond_signal(&worker->wake_cond);
    } else {
        free(wqe);
        error = -EBUSY;
    }
    pag_mutex_unlock(&worker->mutex);

    return error;
}

void
pag_tpool_destroy(struct pag_tpool *pool)
{
    if (!pool)
        return;

    if (pool->workers) {
        size_t i;

        /* signal to worker threads that we're exiting */
        latch_set(&pool->exit_latch);

        /* join all the ending threads */
        for (i = 0; i < pool->n_workers; i++) {
            struct tpool_thread *worker = &pool->workers[i];

            pag_mutex_lock(&worker->mutex);
            xpthread_cond_signal(&worker->wake_cond);
            pag_mutex_unlock(&worker->mutex);
            xpthread_join(worker->thread, NULL);
        }

        /* go clean up resources from the threads */
        for (i = 0; i < pool->n_workers; i++) {
            struct tpool_thread *worker = &pool->workers[i];
            struct tpool_wqe *wqe, *next;

            LIST_FOR_EACH_SAFE (wqe, next, task_node, &worker->task_q) {
                list_remove(&wqe->task_node);
                free(wqe);
            }
            pag_mutex_destroy(&worker->mutex);

            xpthread_cond_destroy(&worker->wake_cond);
            free(worker->name);
        }
        latch_poll(&pool->exit_latch);

        free(pool->workers);
        pool->workers = NULL;
        pool->n_workers = 0;
    }

    latch_destroy(&pool->exit_latch);
    free(pool);
}
