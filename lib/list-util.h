/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  LIST_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  LIST_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif

#include <stdbool.h>

/*
 *    List Structure (Client View) and Definitions.
 */
typedef struct _list_node list_node_t, *list_node_p;	/* List handle. */

/*
 * Public functions.
 */
extern int list_add (list_node_p *list, int position, void *item);
extern void *list_delete (list_node_p *list, int position);
extern int list_find (list_node_p list, void *item, bool (*findFunc)(void *, void *));
extern int list_find_with_dp (list_node_p list, void *item);
extern void *list_get (list_node_p list, int position);
extern int list_length (list_node_p list);

#ifdef __cplusplus
    }
#endif

#endif /* If this file was not INCLUDE'd previously. */
