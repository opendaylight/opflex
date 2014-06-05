/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef  HASH_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  HASH_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif


#include  <stdio.h>			/* Standard I/O definitions. */

/*
 *    Hash Table Structures (Client View) and Definitions.
 */
typedef struct _hash_table *hash_table ;

/*
 *    Miscellaneous declarations.
 */
extern int hash_util_debug ;		/* Global debug switch (1/0 = yes/no). */


/*
 *    Public functions.
 */
extern int hash_add (hash_table table, const char *key, const void *data);
extern int hash_create (int max_entries, int debug, hash_table *table);
extern int hash_delete (hash_table table, const char *key);
extern int hash_destroy (hash_table table);
extern int hash_dump (FILE *outfile, const char *header, hash_table table);
extern int hash_search (hash_table table, const char *key, void **data);
extern int hash_statistics (FILE *outfile, hash_table table);

#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
