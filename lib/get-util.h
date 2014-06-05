/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef  GET_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  GET_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif


extern char *getarg (const char *arg, int *length);
extern char *getfield(const char *s, int *length);
extern char *getstring(const char *last_argument, const char *quotes,
                       int *length);
extern char *getword(const char *string, const char *delimiters,
                     int *length);

#ifdef __cplusplus
    }
#endif

#endif	/* If this file was not INCLUDE'd previously. */
