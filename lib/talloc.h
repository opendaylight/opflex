/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef TALLOC_H
#define TALLOC_H

 /*
 * @File talloc.h
 */

struct talloc_chunk {
	struct talloc_chunk *next;
	void *ptr;
	size_t alloc_size;
	size_t total_size;
};

typedef struct {
	struct talloc_chunk *list;
} TALLOC_CTX;

extern TALLOC_CTX *talloc_init(void) ;
extern void *talloc(TALLOC_CTX *, size_t) ;
extern void talloc_destroy(TALLOC_CTX *)  ;

#endif
