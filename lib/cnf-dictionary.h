/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

/*
 *   @file    dictionary.h
 *   @brief   Implements a dictionary for string variables.
 *
 *  This module implements a simple dictionary object, i.e. a list
 *  of string/string associations. This object is useful to store e.g.
 *  informations retrieved from a configuration file (ini files).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 *  @brief    Dictionary object
 *
 *  This object contains a list of string/string associations. Each
 *  association is identified by a unique string key. Looking up values
 *  in the dictionary is speeded up by the use of a (hopefully collision-free)
 *  hash function.
 */
typedef struct _dictionary_ {
    int             n ;     /* Number of entries in dictionary */
    int             size ;  /* Storage size */
    char        **  val ;   /* List of string values */
    char        **  key ;   /* List of string keys */
    unsigned     *  hash ;  /* List of hash values for keys */
} dictionary ;


unsigned dictionary_hash(const char * key);
dictionary *dictionary_new(int size);
void dictionary_del(dictionary *vd);
char *dictionary_get(dictionary *d, const char *key, char *def);
int dictionary_set(dictionary * vd, const char *key, const char *val);
void dictionary_unset(dictionary *d, const char *key);
void dictionary_dump(dictionary *d, FILE *out);

#endif
