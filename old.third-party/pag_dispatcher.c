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
#include <errno.h>

#include "list.h"
#include "latch.h"
#include "hmapx.h"
#include "pag-thread.h"
#include "pag_tpool.h"
#include "pag_selector.h"


struct pag_disp_pool {
    int next_dispatcher;                 /* used for assigning dispatcher */
    int n_dispatchers;                   /* total dispatchers in pool */
    struct list dispatchers;             /* the list of dispatchers */
    struct latch exit_latch;             /* Tells child threads to exit. */
    struct pag_tpool *workers;           /* the threads assigned to dispatchers */
};

struct pag_dispatcher {
    struct pag_disp_pool *dpool;       /* to get back to the pool */
    struct hmapx keymap;                 /* looks up the dispatcher, based on key */
    struct pag_mutex mutex;            /* Mutex guarding the following. */
    struct list list_node PAG_GUARDED; /* used to link dispatchers together */
    struct pag_selector *selector PAG_GUARDED;     /* per-dispatcher selector */
};

static struct pag_dispatcher * assign_dispatcher(struct pag_disp_pool *dpool);
static void *run_dispatcher(void *arg);
static struct pag_dispatcher * key_find_dispatcher(struct pag_disp_pool *dpool,
                                                   struct pag_selector_key *key);

/* 
 * Crude, round robin assignment for now. Should
 * make this more efficient, but dispatcher assignement
 * shouldn't be frequent, and there shouldn't be 
 * that many dispatchers to traverse
 */
static struct pag_dispatcher *
assign_dispatcher(struct pag_disp_pool *dpool)
{
    struct pag_dispatcher *dispatcher, *next_dispatcher;
    int index = 0;

    LIST_FOR_EACH_SAFE (dispatcher, next_dispatcher, list_node, &dpool->dispatchers) {
        if (++index >= (dpool->next_dispatcher % dpool->n_dispatchers)) {
            dpool->next_dispatcher++;
            return dispatcher;
        }
    }
    return 0;
}


static void *
run_dispatcher(void *arg)
{
    struct pag_dispatcher *dispatcher = (struct pag_dispatcher *)arg;
    struct pag_selector_key *key;
    int error = 0;

    if (!dispatcher || !dispatcher->selector) {
        return 0;
    }

    while (!latch_is_set(&dispatcher->dpool->exit_latch)) {

        /* 
         * Have the selector register all the
         * selector keys for the subsequent poll.
         *
         * TODO: Come up with a less obtrusive selector
         *       guard idiom
         */
        pag_mutex_lock(&dispatcher->mutex);
        pag_selector_get_keyset(dispatcher->selector);
        pag_mutex_unlock(&dispatcher->mutex);

        /* wait on the selector */
        pag_selector_select(dispatcher->selector);

        /* 
         * Check all the selector keys to see if they were 
         * triggered, and invoke the callbacks.
         */
        pag_mutex_lock(&dispatcher->mutex);
        pag_selector_first_key(dispatcher->selector, &key);
        while (!error && key) {
            if (pag_selector_key_is_ready(key)) {
                /* Invoke the callback registered on the key */
                pag_selector_key_do_work(key);
            }

            pag_selector_next_key(dispatcher->selector,&key);
        }
        pag_mutex_unlock(&dispatcher->mutex);
    }
    return 0;
}

int
pag_dispatch_pool_create(int n_dispatchers, 
                         void *(*select_func)(void *),
                         struct pag_disp_pool **p_pool)
{
    struct pag_dispatcher *dispatcher;
    struct pag_selector *selector;
    struct pag_disp_pool *dpool;
    int error = 0;
    int i;

    if (!p_pool) {
        return -EINVAL;
    }

    dpool = xzalloc(sizeof(*dpool));
    dpool->n_dispatchers = n_dispatchers;
    dpool->next_dispatcher = 0;
    latch_init(&dpool->exit_latch);

    /* 
     * create thread pool -- only need single WQE and 1 batch, 
     * since this is just doing a run-to-completion in the
     * dispatcher
     */
    dpool->workers = pag_tpool_create(n_dispatchers, 1, 1);
    if (!dpool->workers) {
        return -ENOMEM;
    }

    /* 
     * Create a dispatcher for each thread in the pool, and
     * assign it the dispatcher worker function
     */
    list_init(&dpool->dispatchers);
    for (i=0; (i<n_dispatchers) && !error ; i++ ) {
        dispatcher = xzalloc(sizeof(*dispatcher));
        dispatcher->dpool = dpool;
        error = pag_selector_create(select_func, &selector);
        if (!error) {
            hmapx_init(&dispatcher->keymap);
            dispatcher->selector = selector;
            error = pag_tpool_add_work(dpool->workers, 0, run_dispatcher, dispatcher);
            list_push_back(&dpool->dispatchers, &dispatcher->list_node);
        }
    }
    *p_pool = dpool;

    return error;
}


void
pag_dispatch_pool_destroy(struct pag_disp_pool *dpool)
{
    /* Walk the list of dispatchers, freeing as we go */
    if (!dpool)
        return;

    /* release the dispatchers */
    latch_set(&dpool->exit_latch);

    pag_tpool_destroy(dpool->workers);

    /* wait for them all to exit */
    latch_poll(&dpool->exit_latch);

    latch_destroy(&dpool->exit_latch);

    /* free up individual dispatchers */
    while (!list_is_empty(&dpool->dispatchers)) {
        list_pop_front(&dpool->dispatchers);
    }

    free(dpool);
}

int
pag_dispatch_register(struct pag_disp_pool *dpool, 
                      struct pag_selector_key *key)
{
    struct pag_dispatcher *dispatcher;
    int error;

    /* Assign a dispatcher */
    dispatcher = assign_dispatcher(dpool);
    if (!dispatcher) {
        return -ENOMEM;
    }

    hmapx_add(&dispatcher->keymap, key);
    pag_mutex_lock(&dispatcher->mutex);
    
    /* Add the object to the dispatcher */
    error = pag_selector_register(dispatcher->selector, key);

    pag_mutex_unlock(&dispatcher->mutex);

    return error;
}

static struct pag_dispatcher *
key_find_dispatcher(struct pag_disp_pool *dpool,
                    struct pag_selector_key *key)
{

    struct pag_dispatcher *dispatcher, *next_dispatcher;

    LIST_FOR_EACH_SAFE (dispatcher, next_dispatcher, list_node, &dpool->dispatchers) {
        if (hmapx_contains(&dispatcher->keymap, key)) {
            return dispatcher;
        }
    }
    return 0;
}

int
pag_dispatch_unregister(struct pag_disp_pool *dpool,
                        struct pag_selector_key *key)
{
    struct pag_dispatcher *dispatcher;
    int error = 0;

    /* look up the dispatcher */
    dispatcher = key_find_dispatcher(dpool, key);
    if (!dispatcher) {
        return -ENOENT;
    }

    pag_mutex_lock(&dispatcher->mutex);
    hmapx_find_and_delete(&dispatcher->keymap, key);
 
    /* remove the object from the dispatcher */
    pag_selector_unregister(dispatcher->selector, key);

    pag_mutex_unlock(&dispatcher->mutex);

    return error;
}
