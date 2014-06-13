/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _NOIRO__ADT_INTRUSIVE_SLIST_H
#define _NOIRO__ADT_INTRUSIVE_SLIST_H

#if 0
   _____ _                                _         _                 _      
  |_   _| |__   ___   _ __ ___   ___  ___| |_   ___(_)_ __ ___  _ __ | | ___ 
    | | | '_ \ / _ \ | '_ ` _ \ / _ \/ __| __| / __| | '_ ` _ \| '_ \| |/ _ \
    | | | | | |  __/ | | | | | | (_) \__ \ |_  \__ \ | | | | | | |_) | |  __/
    |_| |_| |_|\___| |_| |_| |_|\___/|___/\__| |___/_|_| |_| |_| .__/|_|\___|
                                                               |_|           
                     _       _                  _           
                    (_)_ __ | |_ _ __ _   _ ___(_)_   _____ 
                    | | '_ \| __| '__| | | / __| \ \ / / _ \
                    | | | | | |_| |  | |_| \__ \ |\ V /  __/
                    |_|_| |_|\__|_|   \__,_|___/_| \_/ \___|
                                                            
           _             _         _ _       _            _   _ _     _   
       ___(_)_ __   __ _| |_   _  | (_)_ __ | | _____  __| | | (_)___| |_ 
      / __| | '_ \ / _` | | | | | | | | '_ \| |/ / _ \/ _` | | | / __| __|
      \__ \ | | | | (_| | | |_| | | | | | | |   <  __/ (_| | | | \__ \ |_ 
      |___/_|_| |_|\__, |_|\__, | |_|_|_| |_|_|\_\___|\__,_| |_|_|___/\__|
                   |___/   |___/                                          
#endif

#include <stddef.h>

typedef struct s_intr_hook_ s_intr_hook_t;
typedef s_intr_hook_t s_intr_head_t;

static inline void
s_intr_list_init(s_intr_head_t * head);

static inline void
s_intr_list_unlink_next(s_intr_hook_t *prev_hook);

static inline void
s_intr_list_insert_after(
        s_intr_hook_t * new_elem,
        s_intr_head_t * head
        );

static inline int
s_intr_list_is_empty(s_intr_head_t *head);

/* Also exposes an iterator macro:
 *                                S_INTR_LIST_FOR_EACH(elem, head, hook_name) */

#include <noiro/adt/intrusive_slist.inl.h>

#endif/*_NOIRO__ADT_INTRUSIVE_SLIST_H*/

