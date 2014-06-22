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

#ifndef PAG_SELECTOR_H
#define PAG_SELECTOR_H 1

#include <stdint.h>
#include <poll.h>

#include "list.h"

/*
 * Implements the Selector Key and Selector types.
 * 
 * Selector Keys are an abstraction for a selectable construct. 
 * Keys are registered against a Selector, which implements a
 * strategy for the poll across all Selector Keys that it contains.
 *
 * These APIs are not thread safe, so thread-safety must be built
 * on top of them, if needed.
 */

struct pag_selector_key;
struct pag_selector;


/*
 * Create a selector
 *
 * select_func [in]:
 *     function to implement the select
 * p_sel [out]:
 *      provides the new selector
 *
 * RETURNS:
 * --------
 * Always 0
 */
int pag_selector_create(void *(*select_func)(void *), 
                        struct pag_selector **p_sel);

struct pag_key_handlers {
    int (*add_fd)(struct pollfd *, void *);
    void *key_arg;
    int (*is_ready)(void *);
    void *(*do_work)(void *);
    void *work_arg;
};

/*
 * Create a selector key, which can be registered against
 * a selector
 *
 * funcs [in]:
 *     function handlers and args that the selector can call to 
 *     add they key's pollable FDs to the selectors list,
 *     and indicate availability of event
 * key_arg [in]:
 *     argument to pass to the key's 'add_fd' function
 * ops [in]:
 *     selection events of interest (e.g. read, write)
 * p_key [out]:
 *     provides the  new selector key
 *
 * RETURNS:
 * --------
 * Always 0
 */
int pag_selector_key_create(struct pag_key_handlers *funcs, int ops, 
                            struct pag_selector_key **p_key);

/*
 * Delete a selector key. Must first be unregistered from 
 * a selector.
 */
void pag_selector_key_destroy(struct pag_selector_key *key);

/*
 * Destroys a selector
 *
 * selector [in]:
 *    The selector to destroy
 *
 * RETURNS:
 * --------
 * None
 */
void pag_selector_destroy(struct pag_selector *selector);

/*
 * Register a selection key against a selector
 *
 * selector [in]:
 *     the selector to register against
 * key [in]:
 *     the key to register on the selector
 *
 * RETURNS:
 * --------
 * Always 0
 */
int pag_selector_register(struct pag_selector *selector, 
                          struct pag_selector_key *key);

/*
 * Unregisters a selection key from a selector
 *
 * key [in]:
 *     the key to register on the selector
 *
 * RETURNS:
 * --------
 * None
 */
void pag_selector_unregister(struct pag_selector *selector,
                             struct pag_selector_key *key);

/*
 * Build up the keyset for a given selector.
 *
 * selector [in]:
 *     selector to build keyset for
 *
 * RETURNS:
 * --------
 * Always 0
 */
int pag_selector_get_keyset(struct pag_selector *selector);

/*
 * Get the first key in the list
 *
 * selector [in]:
 *     selector to iterate over for keys
 * iterator [in/out]:
 *     as input, provides iterator to use for iterating
 *     over list; as output, returns pointer to current key
 *
 * RETURNS:
 * --------
 * true: valid key returned
 * false: no more keys
 */
int pag_selector_first_key(struct pag_selector *selector,
                           struct pag_selector_key **p_key);
/*
 * Get the next key in the list
 *
 * selector [in]:
 *     selector to iterate over for keys
 * iterator [in/out]:
 *     as input, provides iterator to use for iterating
 *     over list (should be populated by pag_selector_first_key);
 *     as output, returns pointer to current key
 *
 * RETURNS:
 * --------
 * true: valid key returned
 * false: no more keys
 */
bool pag_selector_next_key(struct pag_selector *selector,
                           struct pag_selector_key **iterator);
/*
 * Invoke the select method
 *
 * selector [in]:
 *     selector to invoke select on
 *
 * RETURNS:
 * -------
 * Always 0
 */
void pag_selector_select(struct pag_selector *selector);

/*
 * Invoke is_ready() for the selector key
 *
 * selector [in]:
 *     selector to invoke select on
 *
 * RETURNS:
 * -------
 * true if ready
 * false if not ready
 */
bool pag_selector_key_is_ready(struct pag_selector_key *key);

/*
 * Invoke the do_work() for the selector key
 *
 * selector [in]:
 *     selector to invoke select on
 *
 * RETURNS:
 * -------
 * Always 0
 */
void pag_selector_key_do_work(struct pag_selector_key *key);

#endif /* PAG_SELECTOR_H */
