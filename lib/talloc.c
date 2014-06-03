/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * @File talloc.c
 *
 * @brief This is a very simple temporary memory allocator. To use it do the following:
 *   1) when you first want to allocate a pool of meomry use
 *   talloc_init() and save the resulting context pointer somewhere
 *   2) to allocate memory use talloc()
 *   3) when _all_ of the memory allocated using this context is no longer needed
 *   use talloc_destroy()
 *   talloc does not zero the memory. It guarantees memory of a
 *   TALLOC_ALIGN alignment
 *
 * @author don.kehn@copansys.com
 *
 * @remark Revision History:  (latest changes at top)
 *    DEK - Thur May 31 MST 2001 - created
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <gendcls.h>

#include "talloc.h" 
#include "str-util.h"
#include "debug.h"
#include "util.h"

#define TALLOC_ALIGN 32
#define TALLOC_CHUNK_SIZE (0x2000)

/*
 * @brief talloc_init 
 *       initializae the context
 *
 * @return TALLOC_CTX * talloc context
 *
 */
TALLOC_CTX *talloc_init(void)
{
    TALLOC_CTX *t;

    t = (TALLOC_CTX *)malloc(sizeof(*t));
    if (!t) return NULL;

    t->list = NULL;

    return t;
}

/*
 * @brief talloc
 *       allocate memory block within the context
 *
 * @param0 TALLOC_CTX * the context in whcih the allocation will take place
 * @param1 size_t size to alloc
 *
 * @return void * the block allocated
 *
 */
void *talloc(TALLOC_CTX *t, size_t size) {
    void *p;
    /* printf("********************tmalloc size=%d called %ld\n", 
       size, pthread_self()); */
    size = (size + TALLOC_ALIGN) & (~TALLOC_ALIGN-1);

    if (!t->list || (t->list->total_size - t->list->alloc_size) < size) {
        struct talloc_chunk *c;
        size_t asize = (size + TALLOC_CHUNK_SIZE) & ~(TALLOC_CHUNK_SIZE-1);

		c = (struct talloc_chunk *)malloc(sizeof(*c));
		/*printf("+++++++++++++++++++++++tmalloc size=%d called %ld\n", 
          sizeof(*c), pthread_self()); */
		       
		if (!c) return NULL;
		c->next = t->list;
		c->ptr = (void *)malloc(asize);
		/* DEBUG_OUT(0, ("++++++++++++++++++talloc - malloc %p size = %d\n", c->ptr, asize));
         */
		if (!c->ptr) {
			free(c);
			return NULL;
		}
		c->alloc_size = 0;
		c->total_size = asize;
		t->list = c;
	}

	p = ((char *)t->list->ptr) + t->list->alloc_size;
	t->list->alloc_size += size;
	return p;
}

/*
 * @brief talloc_destroy
 *       free the context
 * 
 * @param0 TALLOC_CTX * to be freed
 *
 * @return NONE
 *
 */
void talloc_destroy(TALLOC_CTX *t)
{
	struct talloc_chunk *c;
	
	while (t->list) {
		c = t->list->next;
		free(t->list->ptr);
		free(t->list);
		t->list = c;
	}

	free(t);
	/* printf("********************tmalloc_destroy called %p pid=%ld\n",t,pthread_self()); 
     */
}
