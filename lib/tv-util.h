/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  TV_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  TV_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif

#include <sys/time.h>		/* System time definitions. */

extern struct timeval tv_add (struct timeval time1, struct timeval time2);
extern int tv_compare (struct timeval time1, struct timeval time2);
extern struct timeval tv_create(long seconds, long microseconds);
extern struct timeval tv_createf (double fseconds);
extern double tv_float (struct timeval time);
extern char *tv_show(struct timeval binaryTime, int in_local, const char *format);
extern struct timeval tv_subtract(struct timeval time1, struct timeval time2);
extern struct timeval tv_tod(void);

#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
