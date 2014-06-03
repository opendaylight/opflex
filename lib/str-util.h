/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef	STR_UTIL_H
#define STR_UTIL_H

#include <stdbool.h>

#define pstrcpy(d,s) safe_strcpy((d),(s),sizeof(pstring)-1)
#define pstrcat(d,s) safe_strcat((d),(s),sizeof(pstring)-1)
#define fstrcpy(d,s) safe_strcpy((d),(s),sizeof(fstring)-1)
#define fstrcat(d,s) safe_strcat((d),(s),sizeof(fstring)-1)

extern bool next_token(char **ptr,char *,char *, size_t) ;
extern char **toktocliplist(int *, char *);
extern bool strequal(const char *, const char *);
extern bool strnequal(const char *,const char *,size_t);
extern bool strcsequal(const char *,const char *);
extern void strlower(char *);
extern void strupper(char *) ;
extern void strnorm(char *) ;
extern bool strisnormal(char *);
extern char *skip_string(char *,size_t);
extern size_t str_charnum(const char *);
extern bool trim_string(char *,const char *,const char *);
extern bool strhasupper(const char *);
extern bool strhaslower(const char *);
extern size_t count_chars(const char *,char);
extern bool str_is_all(const char *s,char c);
extern char *safe_strcpy(char *,const char *, size_t);
extern char *safe_strcat(char *dest, const char *, size_t);
extern char *alpha_strcpy(char *dest, const char *src, size_t maxlength);
extern char *StrnCpy(char *,const char *,size_t );
extern char *strncpyn(char *, const char *,size_t, char );
extern size_t strhex_to_str(char *, size_t, const char *);
extern bool in_list(char *,char *,bool);
extern void string_free(char **);
extern bool string_set(char **,const char *);
extern bool string_sub(char *,const char *,const char *, size_t);
extern bool fstring_sub(char *,const char *,const char *);
extern bool pstring_sub(char *,const char *,const char *);
extern bool all_string_sub(char *,const char *,const char *, size_t );
extern void split_at_last_component(char *, char *, char , char *);
extern char *octal_string(int);
extern char *string_truncate(char *, int);
extern int StrCaseCmp(const char *, const char *) ;
extern size_t strTrim (char *string, int length) ;
extern char *strDestring (char *string, int length, const char *quotes);
extern int strwicmp( char *, char * ) ;

#endif
