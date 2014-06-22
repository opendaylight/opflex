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
#ifndef MISC_UTIL_H
#define MISC_UTIL_H 1

#define PAG_ERROR_MSG_LEN 80

#include <stdio.h>
#include <stdbool.h>

typedef char fstring[128];

/* protos */
FILE *pipe_write(const char *);
FILE *pipe_read(const char *);
int pipe_close(FILE *);
void strerr_wrapper(int);
bool file_exist(char *);
char *timestring(void);
size_t get_file_size(char *);

#endif //MISC_UTIL_H
