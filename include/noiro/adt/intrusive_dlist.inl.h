/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _NOIRO__ADT_INTRUSIVE_DLIST_INL_H
#define _NOIRO__ADT_INTRUSIVE_DLIST_INL_H

#include <assert.h>

struct d_intr_hook_ {
    struct d_intr_hook_ *link[2];
};

static inline void
d_intr_list_init(d_intr_head_t *head) {
    head->link[0] = head->link[1] = head;
}

static inline void
d_intr_list_unlink(d_intr_hook_t *hook) {
    /* skip over */
    hook->link[0]->link[1]=hook->link[1];
    hook->link[1]->link[0]=hook->link[0];
    /* clean up, not strictly needed! */
    hook->link[0] = hook->link[1] = hook;
}

static inline void d_intr_list_insert(
        d_intr_hook_t * new_elem,
        d_intr_hook_t * previous,
        unsigned char dir
        )
{
    assert(dir == (dir & 0x1));

    dir &= 0x1;
    unsigned char rid = dir ^ 0x1;

    new_elem->link[rid] = previous;
    new_elem->link[dir] = previous->link[dir];
    previous->link[dir]->link[rid] = new_elem;
    previous->link[dir] = new_elem;
}

#define D_INTR_LIST_FOR_EACH_04args(elem, head, hook_name, dir)       \
    for (                                                             \
            d_intr_hook_t                                             \
                * ____si_iter = (head)->link[dir],                    \
                * ____si_next                                         \
          ;                                                           \
            elem = (__typeof__(elem))((char *)____si_iter             \
                - offsetof(__typeof__(*elem), hook_name)),            \
            ____si_next = ____si_iter->link[dir],                     \
            ____si_iter != head                                       \
          ;                                                           \
            (void) (head == &elem->hook_name), /* for type check */   \
            ____si_iter = ____si_next                                 \
        )

#define D_INTR_LIST_FOR_EACH_03args(elem, head, hook_name) \
    D_INTR_LIST_FOR_EACH_04args(elem, head, hook_name, 0)

#include <noiro/tricks/overload.h>
#define D_INTR_LIST_FOR_EACH(...) \
    VARIADIC_FN(D_INTR_LIST_FOR_EACH, ##__VA_ARGS__)
#define d_intr_list_insert_03args d_intr_list_insert
#define d_intr_list_insert_02args(_1, _2) d_intr_list_insert(_1, _2, 0)
#define d_intr_list_insert(...) VARIADIC_FN(d_intr_list_insert, ##__VA_ARGS__)

static inline int d_intr_list_is_empty(d_intr_head_t *head) {
    return head->link[0] == head;
}

#endif/*_NOIRO__ADT_INTRUSIVE_DLIST_INL_H*/
