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

#ifndef PAG_TPOOL_H
#define PAG_TPOOL_H 1

/* Structure is opaque to user */
struct pag_tpool;

/*
 * Create a thread pool, specifying the number of threads
 * ('n_workers') in the pool, the maximum work queue depth of each
 * thread, and the number of work requests that each thread
 * will batch up for processing
 *
 * PARAMETERS:
 * -----------
 * n_workers [in]:
 *     The number of threads in the pool
 * max_qlen [in]:
 *     The maximum depth of each thread's work queue
 * n_batch [in]:
 *     The maximum number of work requests that a thread
 *     will execute at a time
 *
 * RETURNS:
 *     0: failure in creating the pool
 *     >0: Handle for the thread pool
 */
struct pag_tpool * pag_tpool_create(int n_workers, int max_qlen, int n_batch);

/*
 * Add work to the thread pool
 *
 *  PARAMETERS:
 *  -----------
 *  pool [in]:
 *      pool handle returned from pag_tpool_create.
 *  id [in]:
 *      opaque identifier, used to ensure that all requests
 *      from the same identifier are scheduled to the same
 *      thread, guaranteeing serialization. If NULL, then the
 *      scheduler can assign to any thread.
 *  work_func [in]:
 *      Function for the thread pool to execut
 *  work_arg [in]:
 *      Variable to pass to the function, when executed
 *      by the thread pool
 *
 *
 *  RETURNS:
 *  --------
 *  0 on success
 * < 0 (errno)
 */
int pag_tpool_add_work(struct pag_tpool *pool, void *id,
                       void * (*work_func)(void *arg), void *work_arg);


/*
 * Destroy the thread pool
 *
 *  PARAMETERS:
 *  -----------
 *  pool [in]:
 *      pool handle returned from pag_tpool_create.
 *
 *  RETURNS:
 *  --------
 *  None
 */
void pag_tpool_destroy(struct pag_tpool *pool);

#endif /* PAG_TPOOL_H */
