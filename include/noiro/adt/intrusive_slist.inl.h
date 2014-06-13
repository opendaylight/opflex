/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _NOIRO__ADT_INTRUSIVE_SLIST_INL_H
#define _NOIRO__ADT_INTRUSIVE_SLIST_INL_H

struct s_intr_hook_ {
    struct s_intr_hook_ *link;
};

static inline void
s_intr_list_init(s_intr_head_t *head) {
    head->link = NULL;
}

static inline void
s_intr_list_unlink_next(s_intr_hook_t *prev_hook) {
    s_intr_hook_t * to_unlink = prev_hook->link;
    if(!to_unlink) {
        return;
    }
    /* skip over */
    prev_hook->link=to_unlink->link;
    to_unlink->link=NULL; /* clean up, not strictly needed! */
}

static inline void s_intr_list_insert_after(
        s_intr_hook_t * new_elem,
        s_intr_hook_t * head
        )
{
    new_elem->link = head->link;
    head->link = new_elem;
}

#define S_INTR_LIST_FOR_EACH(elem, head, hook_name)                   \
    for (                                                             \
            s_intr_hook_t                                             \
                * ____si_iter = (head)->link,                         \
                * ____si_next                                         \
          ;                                                           \
            elem = (__typeof__(elem))((char *)____si_iter             \
                - offsetof(__typeof__(*elem), hook_name)),            \
            ____si_next = ____si_iter ? ____si_iter->link : NULL,     \
            ____si_iter /* we underflew, but exit loop here... ;-) */ \
          ;                                                           \
            (void) (head == &elem->hook_name), /* for type check */   \
            ____si_iter = ____si_next                                 \
        )

static inline int s_intr_list_is_empty(s_intr_head_t *head) {
    return !head->link;
}

#endif/*_NOIRO__ADT_INTRUSIVE_SLIST_INL_H*/
