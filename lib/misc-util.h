/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

/* This header file consists of wrapper routines for popen() and pclose()
 * See pipe-util.c for more details.
 */

#define PAG_ERROR_MSG_LEN 80

#include <stdio.h>

/* protos */
FILE *pipe_write(const char *);
FILE *pipe_read(const char *);
int pipe_close(FILE *);
void strerr_wrapper(int);
