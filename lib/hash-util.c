/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


/*
*    These utilities provide a means of building hash tables and performing
*    hash searches.
*
*    The classic representation of hash tables is used for these hash tables.
*    An array of buckets is created by hash_create(), sized to the first prime
*    number M that is larger than the expected maximum number of elements in
*    the table.  Key-value pairs are then added to the table by hashAdd().
*    A character string key is "folded" into an integer and divided by the
*    prime number M to produce an index into the array of buckets; the key-value
*    pair is then stored in the indicated bucket.  If multiple key-value pairs
*    map into the same bucket (AKA a collision), they are chained together by a
*    linked list attached to the bucket.
*
*    Building a hash table using these functions is very simple.  First, create
*    an empty hash table:
*
*        #include  "hash_util.h"		-- Hash table definitions.
*        #define  NUM_ITEMS  500
*        hash_table  table;
*        ...
*        hash_create (NUM_ITEMS, 0, &table);
*
*    The first argument to hash_create() is the expected number of items in
*    the table; the table will handle more, albeit with slower lookup times.
*
*    Key-value pairs are added to the table with hashAdd():
*
*        hashAdd (table, "<key>", (void *) value);
*
*    Keys are null-terminated characters strings and values must be cast as
*    (VOID *) pointers.  If the key is already in the table, its old value
*    is replaced with the new value.
*
*    Looking up the value of a key is done with hash_search():
*
*        void  *value;
*        ...
*        if (hash_search (table, "<key>", &value))
*            ... found ...
*        else
*            ... not found ...
*
*    The value is returned as a (VOID *) pointer, which the caller must then
*    cast back to the original type.
*
*    Key-value pairs can be individually deleted from a hash table:
*
*        hash_delete (table, "<key>");
*
*    or the entire table can be destroyed:
*
*        hash_destroy (table);
*
*    The HASH_UTIL group of hash table functions offer several advantages
*    over the Standard C Library hashing functions (HCREATE(3), HDESTROY(3),
*    and HSEARCH(3)).  First, the HASH_UTIL functions are easier to use: the
*    multi-purpose functionality of HSEARCH(3) is broken up into hashAdd()
*    and hash_search(), etc.  Second, the HASH_UTIL functions allow for more
*    than one hash table in a program.
*
*Procedures:
*    hash_add() - adds a key-data pair to a hash table.
*    hash_create() - creates an empty hash table.
*    hash_delete() - deletes a key-data pair from a hash table.
*    hash_destroy() - deletes a hash table.
*    hash_dump() - dumps a hash table.
*    hash_search() - locates a key in a hash table and returns the data value
*        associated with the key.
*    hash_statistics() - displays various statistics for a hash table.
*
*/

#include  <errno.h>
#include  <math.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  "str-util.h"
#include "vlog.h"
#include  "vlog-opflex.h"
#include  "hash-util.h"

VLOG_DEFINE_THIS_MODULE(hash_util);

/*
 *    Hash Table Data Structures.
 */
typedef  struct  hash_item {
    char  *key;			/* Item key. */
    const  void  *value;		/* Item value. */
    struct  hash_item  *next;		/* Pointer to next item in list. */
}  hash_item;

typedef  struct  _hash_table {
    int  debug;		            /* Debug switch (1/0 = yes/no). */
    int  max_chains;			/* Maximum number of entries N in table. */
    int  num_chains;			/* Actual number of non-empty entries. */
    int  longest_chain;			/* Records length of longest chain. */
    hash_item  *chain[1];		/* Array of N pointers to item chains. */
}  _hash_table;


/*
 *   Protos
 */

static int hash_key (const char *key, int table_sz);
static int hash_prime (int number);

/*
 *    hash_add () 
 *    Function hash_add() adds a key-value pair to a hash table.  If the key is
 *    already present in the table, its old value is replaced by the new value.
 *    The table must have already been created by hash_create().
 *
 *    Invocation:
 *        status = hash_add (table, key, data);
 *
 *    where
 *        <table>
 *            is the hash table handle returned by hash_create().
 *        <key>
 *            is the key for the item being entered in the table.
 *        <data>
 *            is the data to be associated with the key.  This argument is a
 *            (VOID *) pointer; whatever data or pointer to data being passed
 *            in for this argument should be cast to (VOID *).
 *        <status>
 *            returns the status of adding the key to the hash table, zero if
 *            no errors occurred and ERRNO otherwise.
 */
int hash_add (hash_table table, const char *key, const void *data)
{
    static char mod[] = "hash_add";
    hash_item  *item, *prev;
    int  comparison, index;

    if (table == NULL) {
        errno = EINVAL;
        VLOG_ERR("%s: Hash table not created yet.\n", mod);
        return (errno);
    }

	/* If the key is already in the hash table, then replace its data value. */
    index = hash_key (key, table->max_chains);
    comparison = -1;  prev = (hash_item *) NULL;
    for (item = table->chain[index];  item != NULL;  item = item->next) {
        comparison = strcmp (item->key, key);
        if (comparison >= 0)  break;
        prev = item;
    }

    if (comparison == 0) {
        item->value = data;
        if (table->debug)
            printf ("%s: Replaced \"%s\":%p (%p) in table %p[%d].\n",
                    mod, key, data, item, table, index);
        return (0);
    }

	/* 
	*	Add a brand new item to the hash table: allocate an ITEM node for the item,
   	*	fill in the fields, and link the new node into the chain of items. 
   	*/
    item = (hash_item *) xzalloc(sizeof (hash_item));	/* Allocate item node. */
    item->key = strdup (key);			/* Fill in the item node. */
    if (item->key == NULL) {
        VLOG_ERR("%s: (add) Error duplicating key \"%s\".\nstrdup: ",
                 mod, key);
        free ((char *) item);
        return (errno);
    }
    item->value = data;

    if (prev == NULL) {				/* Link in at head of list. */
        item->next = table->chain[index];
        if (item->next == NULL)
            table->num_chains++;
        table->chain[index] = item;
    }
    else {					/* Link in further down the list. */
        item->next = prev->next;
        prev->next = item;
    }

    if (table->debug)
        printf ("%s: Added \"%s\":%p (%p) to table %p[%d].\n",
                mod, key, data, item, table, index);


	/* 
	**	For statistical purposes, measure the length of the chain and, if necessary,
	**   update the LONGEST_CHAIN value for the hash table. 
	*/

    comparison = 0;
    for (item = table->chain[index];  item != NULL;  item = item->next) {
        comparison++;
    }
    if (table->longest_chain < comparison)
        table->longest_chain = comparison;

    return (0);
}

/*
 *    hash_create ()*
 *    Function hash_create() creates an empty hash table.  hash_add() can then be
 *    called to add entries to the table.  The number of buckets in the table
 *    is equal to the first prime number larger than the expected maximum number
 *    of elements in the table.
 *
 *    Invocation:
 *        status = hash_create (max_entries, debug, &table);
 *
 *    where
 *        <max_entries>
 *            is the maximum number of entries expected in the table.
 *        <debug>		- I
 *            enables debug output (to STDOUT) on any HASH_UTIL calls
 *            for the new hash table.
 *        <table>
 *            returns a handle for the new hash table.  This handle is
 *            used for accessing the table in subsequent HASH_UTIL calls.
 *        <status>
 *            returns the status of creating the hash table, zero if no errors
 *            occurred and ERRNO otherwise.
 *
 */
int hash_create (int max_entries, int debug, hash_table *table)
{
    static char mod[] ="hash_create";
    int  i, prime, size;

	/* 
	** Find the first prime number larger than the expected number of entries
  	** in the table. */

    prime = (max_entries % 2) ? max_entries : max_entries + 1;
    for (;; ) {			/* Check odd numbers only. */
        if (hash_prime (prime))  break;
        prime += 2;
    }

	/* Create and initialize the hash table. */
    size = sizeof (_hash_table)  +  ((prime - 1) * sizeof (hash_item *));
    *table = (void *) xzalloc (size);
    (*table)->debug = debug;
    (*table)->max_chains = prime;
    (*table)->num_chains = 0;		/* Number of non-empty chains. */
    (*table)->longest_chain = 0;	/* Length of longest chain. */
    for (i = 0;  i < prime;  i++)
        (*table)->chain[i] = (hash_item *) NULL;

    if ((*table)->debug)
        printf ("%s: Created hash table %p of %d elements.\n",
                mod, *table, prime);

    return (0);
}

/*
 *    hash_delete ()  *
 *    Function hash_delete() deletes a key-data entry from a hash table.  The
 *    table must have already been created by hash_create() and the key-data
 *    entry added to the table by hash_add().
 *
 *    Invocation:
 *        status = hash_delete (table, key);
 *
 *    where
 *        <table>
 *            is the hash table handle returned by hash_create().
 *        <key>
 *            is the key for the item being deleted from the table.
 *        <status>
 *            returns the status of deleting the key from the hash table, zero
 *            if no errors occurred and ERRNO otherwise.
 *
 */
int hash_delete (hash_table table, const char *key)
{
    static char mod[] = "hash_delete";
    hash_item  *item, *prev;
    int  index;

    if (table == NULL) {
        errno = EINVAL;
        VLOG_ERR("%s: Hash table not created yet.\n", mod);
        return (errno);
    }

	/* Locate the key's entry in the hash table. */
    index = hash_key (key, table->max_chains);
    prev = (hash_item *) NULL;
    for (item = table->chain[index];  item != NULL;  item = item->next) {
        if (strcmp (item->key, key) == 0)  break;
        prev = item;
    }

	/* Unlink the entry from the hash table and free it. */
    if (item == NULL) {
        if (table->debug)
            printf ("%s: Key \"%s\" not found in table %p.\n",
                    mod, key, table);
        return (-2);
    }
    else {
        if (prev == NULL)
            table->chain[index] = item->next;
        else
            prev->next = item->next;
    }

    if (table->debug)
        printf ("%s: Deleted \"%s\":%p from table %p.\n",
                mod, item->key, item->value, table);

    free (item->key);			/* Free item key. */
    free ((char *) item);		/* Free the item. */

    return (0);
}

/*
 *    hash_destroy () deletes a hash table.
 *
 *    Invocation:
 *        status = hash_destroy (table);
 *
 *    where
 *        <table>
 *            is the hash table handle returned by hash_create().
 *        <status>
 *            returns the status of deleting the hash table, zero if no errors
 *            occurred and ERRNO otherwise.
 *
 */
int hash_destroy (hash_table table)
{
    int  i;
    hash_item  *item, *next;

    if (table->debug)
        printf ("(hash_destroy) Deleting hash table %p.\n", table);

    if (table == NULL)
        return (0);

    for (i = 0;  i < table->max_chains;  i++) {
        for (item = table->chain[i];  item != NULL;  item = next) {
            next = item->next;
            free (item->key);			/* Free item key. */
            free ((char *) item);		/* Free the item. */
        }
    }

    free ((char *) table);			/* Free the hash table. */
    return (0);

}

/*
 *    hash_dump () dumps a hash table to the specified output file.
 *
 *    Invocation:
 *        status = hash_dump (outfile, header, table);
 *
 *    where
 *        <outfile>
 *            is the UNIX file descriptor (FILE *) for the output file.
 *        <header>
 *            is a text string to be output as a header.  The string is
 *            actually used as the format string in an FPRINTF statement,
 *            so you need to include any newlines ("\n"), etc. that you
 *            need.  This argument can be NULL.
 *        <table>
 *            is the hash table handle returned by hash_create().
 *        <status>
 *            returns zero.
 *
 */
int hash_dump (FILE *outfile, const char *header, hash_table table)
{
    int  i;
    hash_item  *item;

    if (header != NULL)
        fprintf (outfile, header);

    if (table == NULL) {
        fprintf (outfile, "-- Null Hash Table --\n");
        return (0);
    }

    hash_statistics (outfile, table);
    fprintf (outfile, "\n");

    for (i = 0;  i < table->max_chains;  i++) {
        item = table->chain[i];
        if (item != NULL) {
            fprintf (outfile, "Bucket %d:\n", i);
            while (item != NULL) {
                fprintf (outfile, "    Value: %p    Key: \"%s\"\n",
                         item->value, item->key);
                item = item->next;
            }
        }
    }

    return (0);
}

/*
 *    hash_key () converts a character string key to an integer index
 *    into a hash table.  The conversion takes place in two steps: (i) "fold"
 *    the character string key into an integer, and (ii) divide that integer
 *    by the number of buckets in the table (a prime number computed by
 *    hash_create()).
 *
 *    The folding algorithm is best illustrated by an example.  Suppose you
 *    have a key "ABCDEFGHIJK" and integers are 4-byte entities on your
 *    computer.  The key could then folded by picking up 4-byte groups of
 *    characters from the key and adding them together:
 *
 *                                "ABCD"
 *                                "EFGH"
 *                              +  "IJK"
 *                                ------
 *                              = result
 *
 *    One way of doing this is to:
 *        (1) pick up "A", then
 *        (2) shift "A" left 8 bits and add "B", then
 *        (3) shift "AB" left 8 bits and add "C", and then
 *        (4) shift "ABC" left 8 bits and add "D".
 *
 *    Pick up "EFGH" in the same way and add it to "ABCD"; then pick up "IJK"
 *    and add it on.
 *
 *    Invocation:
 *        index = hash_key (key, table_sz);
 *
 *    where
 *        <key>
 *            is a character string key.
 *        <table_sz>
 *            is the size M of the table being hashed.
 *        <index>
 *            returns the index, [0..M-1], in the table of where the key
 *            can be found.
 *
*/
static int hash_key (const char *key, int table_sz)
{
    const char *s;
    unsigned int i, value, sum;

    if (table_sz == 0)
        return (0);	/* Empty table? */

	/* Fold the character string key into an integer number. */

#define  BITS_TO_SHIFT  8

    s = key;
    for (sum = 0;  *s != '\0'; ) {
        for (i = value = 0;  (i < sizeof (int)) && (*s != '\0');  i++, s++)
            value = (value << BITS_TO_SHIFT) + *((unsigned char *) s);
        sum = sum + value;
    }

    return (sum % table_sz);		/* Return index [0..M-1] into table. */
}

/*
 *    hash_prime () determines if a number is a prime number.
 *
 *    Invocation:
 *        isPrime = hash_prime (number);
 *
 *    where
 *        <number>
 *            is the number to be checked for "prime"-ness.
 *        <isPrime>
 *            return TRUE (non-zero) if NUMBER is prime and FALSE (zero) if
 *            NUMBER is not prime.
 *
 */
static int hash_prime (int number)
{
    int  divisor;

    if (number < 0)
        number = -number;
    if (number < 4)
        return (1);	/* 0, 1, 2, and 3 are prime. */

	/* 	Check for possible divisors.  The "divisor > dividend" test is similar
   	**	to checking 2 .. sqrt(N) as possible divisors, but avoids the need for
	**  linking to the math library. 
	*/
    for (divisor = 2; ; divisor++) {
        if ((number % divisor) == 0)
            return (0);		/* Not prime - divisor found. */
        if (divisor > (number / divisor))
            return (1);		/* Prime - no divisors found. */
    }
}

/*
 *    hash_search () looks up a key in a hash table and returns the
 *    data associated with that key.  The hash table must be created using
 *    hash_create() and entries must be added to the table using hash_add().
 *
 *    Invocation:
 *        found = hash_search (table, key, &data);
 *
 *    where
 *        <table>
 *            is the hash table handle returned by hash_create().
 *        <key>
 *            is the key for the item being searched in the table.
 *        <data>
 *            returns the data associated with the key.  The value returned is
 *            a (VOID *) pointer; this pointer can be cast back to whatever data
 *            or pointer to data was stored by hash_add().
 *        <found>
 *            returns TRUE (non-zero) if the key was found in the hash table;
 *            FALSE (zero) is returned if the key was not found.
 *
 */
int hash_search (hash_table table, const char *key, void **data)
{   
    static char mod[] = "hash_search";
    hash_item  *item;
    int  comparison, index;

	/* Lookup the item in the hash table. */
    index = hash_key (key, table->max_chains);
    comparison = -1;
    for (item = table->chain[index];  item != NULL;  item = item->next) {
        comparison = strcmp (item->key, key);
        if (comparison >= 0)  break;
    }
	/* If found, return the item's data value to the calling routine. */
    if (comparison == 0) {
        if (data != NULL)
            *data = (void *) item->value;
        if (table->debug)
            printf ("%s:  \"%s\":%p found in table %p.\n", 
                    mod, key, item->value, table);
        return (-1);
    } else {
        if (data != NULL)
            *data = NULL;
        if (table->debug)
            printf ("%s: Key \"%s\" not found in table %p.\n", 
                    mod, key, table);
        return (0);
    }

}

/*
 *    hash_statistics () outputs various statistical measurements for
 *    a hash table.
 *
 *    Invocation:
 *        status = hash_statistics (outfile, table);
 *
 *    where
 *        <outfile>
 *            is the UNIX file descriptor (FILE *) for the output file.
 *        <table>
 *            is the hash table handle returned by hash_create().
 *        <status>
 *            returns zero.
 *
 */
int hash_statistics (FILE *outfile, hash_table table)
{
    static char mod[] = "hash_statistics";
    int  count, *histogram, i, longest_chain, max_chains, num_chains;
    long  sum, sum_of_squares;
    hash_item *item;

    if (table == NULL) {
        fprintf (outfile, "-- Null Hash Table --\n");
        return (0);
    }

    max_chains = table->max_chains;
    num_chains = table->num_chains;
    longest_chain = table->longest_chain;

    fprintf (outfile, 
             "There are %d empty buckets, %d non-empty buckets,\nand %d items in the longest chain.\n\n",
             max_chains - num_chains, num_chains, longest_chain);

    histogram = (int *) xzalloc((longest_chain + 1) * sizeof (int));

    for (count = 0;  count <= longest_chain;  count++)
        histogram[count] = 0;

    for (i = 0;  i < max_chains;  i++) {
        item = table->chain[i];
        for (count = 0;  item != NULL;  count++)
            item = item->next;
        histogram[count]++;
    }

    for (count = 1, sum = sum_of_squares = 0;
         count <= longest_chain;  count++) {
        fprintf (outfile, "Buckets of length %d: %d\n", count, histogram[count]);
        sum = sum  +  histogram[count] * count;
        sum_of_squares = sum_of_squares  +  histogram[count] * count * count;
    }

    fprintf (outfile, "\nMean bucket length = %G\n",
             (double) sum / (double) num_chains);
    fprintf (outfile, "\nStandard deviation = %G\n",
             sqrt ((double) ((num_chains * sum_of_squares) - (sum * sum)) /
                   (double) (num_chains * (num_chains - 1))));

    free ((char *) histogram);
    return (0);
}

#ifdef  TEST

/*******************************************************************************
**
**    Program to test the HASH_UTIL routines.  Compile as follows:
**
**        % cc -g -DTEST hash_util.c -I<... includes ...>
**
**    Invocation:
**
**        % a.out [ <num_entries> ]
**
*********************************************************************************/

//extern  int  vperror_print;		/* 0 = no print, !0 = print */

main (int argc, char *argv[])
{ 
    char  *text[16];
    int  i, maxNumEntries;
    void  *data, *table;

    //vperror_print = 1;

    maxNumEntries = (argc > 1) ? atoi (argv[1]) : 100;

	/* Create an empty hash table. */
    if (hash_create (maxNumEntries, 1, &table)) {
        VLOG_ERR("Error creating table.\nhash_create: ");
        exit (errno);
    }

	/* Add "SYM_n" symbols to the table. */
    for (i = 0;  i < maxNumEntries;  i++) {
        sprintf (text, "SYM_%d", i);
        if (hash_add (table, text, (void *) i)) {
            VLOG_ERR("Error adding entry %d to the table.\nhash_add: ", i);
            exit (errno);
        }
    }

	/* Verify that the symbols were entered correctly and with the correct value. */
    for (i = 0;  i < maxNumEntries;  i++) {
        sprintf (text, "SYM_%d", i);
        if (!hash_search (table, text, &data) || ((int) data != i)) {
            VLOG_ERR("Error looking up entry %d in the table.\nhash_search: ", i);
            exit (errno);
        }
    }

	/* Dump the hash table. */
    hash_dump (stdout, "\n", table);

	/* Delete the hash table. */
    if (hash_destroy (table)) {
        VLOG_ERR("Error deleting the hash table.\nhash_destroy: ");
        exit (errno);
    }
}
#endif /* TEST */
