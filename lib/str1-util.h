/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  STR1_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  STR1_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif

#include  <stddef.h>			/* Standard C definitions. */
#ifndef NO_STRDUP
#    include  <string.h>		/* Standard C string functions. */
#endif

extern int  str_util_debug ;		/* Global debug switch (1/0 = yes/no). */


extern char *strCat(const char *source, int length, char destination[],
                    size_t maxLength);
extern char *strConvert(char *string);
extern char *strCopy(const char *source, int length, char destination[],
                     size_t maxLength);
extern char *strDestring(char *string, int length, const char *quotes);
extern size_t  strDetab(char *stringWithTabs, int length, int tabStops,
                        char *stringWithoutTabs, size_t maxLength);
extern void  strEnv(const char *string, int length, char *translation,
                    size_t maxLength);
extern char *strEtoA(char *string, int length);

extern size_t strInsert(const char *substring, int subLength, size_t offset,
                        char *string, size_t maxLength);
extern int  strMatch(const char *target, const char *model);
extern size_t strRemove(size_t numChars, size_t offset, char *string);
extern char *strToLower(char *string, int length);
extern char *strToUpper(char *string, int length);
extern size_t  strTrim(char *string, int length);

#ifdef NO_STRDUP
        extern char *strdup(const char *string);
#endif

extern char *strndup(const char *string, size_t length);


#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
