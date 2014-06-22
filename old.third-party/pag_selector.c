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

#include <poll.h>
#include <errno.h>

#include "pag_selector.h"

/*
 * Implements the Selector Key and Selector types.
 * 
 * Selector keys are created for pollable constructs. The
 * keys are registered against a selector, which invokes a
 * common strategy for the poll across all selector keys.
 */

struct pag_selector_key {
    struct list list_node;
    int select_ops;                         /* selection events of interest */
    struct pag_key_handlers funcs;
    void *key_arg;
};

struct pag_selector {
    struct list keys;                     /* registered selector keys */

    int max_fd;                           /* currently unused */
    struct pollfd *fdset;                 /* currently unused */
    struct pag_selector_funcs {           /* NB: could remove as struct */
        void *(*select)(void *);          /* waits on key set */
    } funcs;
};


static void init_keyset(struct pag_selector *selector, size_t size);

static void
init_keyset(struct pag_selector *selector, size_t size)
{
    selector->fdset = realloc(selector->fdset, size);    

    /* 
     * TODO: NO-OP for now.  We'll want to put the
     * default FD handling here, only overridden
     * if provided.
     */
}


int
pag_selector_create(void *(*select_func)(void *), struct pag_selector **p_sel)
{
    struct pag_selector *selector;

    if (!select_func || !p_sel)
        return -EINVAL;

    /* need to allocate memory for the pointers as well */
    selector = xzalloc(sizeof(*selector));
    selector->funcs.select = select_func;
    selector->fdset = 0;
    list_init(&selector->keys);

    *p_sel = selector;

    return 0;
}

void
pag_selector_destroy(struct pag_selector *selector)
{
    struct pag_selector_key *key, *next_key;
    /*
     * remove and free all the selector keys
     */
    LIST_FOR_EACH_SAFE (key, next_key, list_node, &selector->keys) {
        list_remove(&key->list_node);
        pag_selector_key_destroy(key);
    }
}

int
pag_selector_key_create(struct pag_key_handlers *funcs,
                        int ops, struct pag_selector_key **p_key)
{
    struct pag_selector_key *key;

    key = xzalloc(sizeof(*key));
    key->select_ops = ops;
    key->funcs = *funcs;

    *p_key = key;
    return 0;
    
}

void
pag_selector_key_destroy(struct pag_selector_key *key)
{
    free(key);
}

int
pag_selector_register(struct pag_selector *selector, struct pag_selector_key *key)
{
    list_push_back(&selector->keys, &key->list_node);
    return 0;
}

void
pag_selector_unregister(struct pag_selector *selector, struct pag_selector_key *key)
{
    list_remove(&key->list_node);
}

int
pag_selector_get_keyset(struct pag_selector *selector)
{
    struct pag_selector_key *key, *next_key;
    int index = 0;
    int n_keys = list_size(&selector->keys);

    init_keyset(selector, n_keys);
    LIST_FOR_EACH_SAFE (key, next_key, list_node, &selector->keys) {
        /* build up the FDs to select on */
        if (key->funcs.add_fd) {
            key->funcs.add_fd(&selector->fdset[index++], key->funcs.key_arg);
        }
    }
    return 0;
}

int
pag_selector_first_key(struct pag_selector *selector,
                       struct pag_selector_key **p_key)
{
    struct pag_selector_key *key = 0;

    if (!selector || !p_key) {
        if (p_key) {
            *p_key = 0;
        }
        return -EINVAL;
    }

    if (!list_is_empty(&selector->keys)) {
        key = OBJECT_CONTAINING(list_front(&selector->keys),key,list_node);
    }
    
    *p_key = key;
    if (!key) {
        return -ENOMEM;
    }
    return 0;
}

bool
pag_selector_next_key(struct pag_selector *selector,
                      struct pag_selector_key **iterator)
{
    struct pag_selector_key *key;

    if (!selector || !iterator)
        return false;

    key = *iterator;
    LIST_FOR_EACH_CONTINUE(key, list_node, &selector->keys) {
        *iterator = key;
        return true;
    }
    *iterator = 0;
    return false;
}

void
pag_selector_select(struct pag_selector *selector)
{
    if(selector->funcs.select) {
        selector->funcs.select(selector->fdset);
    }
}

bool
pag_selector_key_is_ready(struct pag_selector_key *key)
{
    if (key->funcs.is_ready) {
        return key->funcs.is_ready(key);
    }
    return false;
}

void
pag_selector_key_do_work(struct pag_selector_key *key)
{
    if (key->funcs.do_work) {
        key->funcs.do_work(key->funcs.work_arg);
    }
}
