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

#ifndef PAG_DISPATCHER_H
#define PAG_DISPATCHER_H 1

#include <stdint.h>

#include "pag_tpool.h"
#include "pag_selector.h"

struct pag_disp_pool;


/*
 * A dispatcher pool. 
 *
 * Selector Keys with event handlers/callbacks are registered 
 * with the Selectors for the dispatchers in the pool.
 * The dispatcher provides a set of 1 or more event waiters, with
 * a strategy for selection provided by the selector.
 */

/*
 * create a dispatch pool
 *
 * n_dispatchers [in]:
 *     number of dispatchers to create
 * select_func [in]:
 *     function to use for event waiting in dispatcher
 * p_pool [out]:
 *     output value for new pool that's created
 *
 * RETURNS:
 * --------
 * 0 on success
 * <0 failed
 */
int pag_dispatch_pool_create(int n_dispatchers, 
                             void *(*select_func)(void *),
                             struct pag_disp_pool **p_pool);

/*
 * Register a selector key on a dispatcher.
 *
 * dpool [in]:
 *     dispatch pool to register with
 * key [in]
 *     selector key to register
 *
 * RETURNS:
 * --------
 * 0 on success
 * <0 failed
 */
int pag_dispatch_register(struct pag_disp_pool *dpool, 
                          struct pag_selector_key *key);

/*
 * Unregister a selector key on a dispatcher.
 *
 * dpool [in]:
 *     dispatch pool key was registered with
 * key [in]
 *     selector key to unregister
 *
 * RETURNS:
 * --------
 * 0 on success
 * <0 failed
 */
int pag_dispatch_unregister(struct pag_disp_pool *dpool,
                            struct pag_selector_key *key);

/*
 * Destroy a dispatch pool
 *
 * dpool [in]:
 *     dispatch pool to destroy
 *
 * RETURNS:
 * --------
 * None.
 */
void pag_dispatch_pool_destroy(struct pag_disp_pool *pool);

#endif /* PAG_DISPATCHER_H */
