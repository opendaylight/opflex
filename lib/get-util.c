/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


/*
 *    GETARG - gets the next argument from a string.
 *    GETFIELD - gets the next field from a database dump file record.
 *    GETSTRING - gets the next (possibly quote-delimited) argument from a
 *        string.
 *    GETWORD - gets the next "word" in a string.
 *
 */

#include  <stdio.h>
#include  <string.h>

#include  "get-util.h"
#include  "str1-util.h"


/*
 *    getarg- scans a string and returns the location and length of the next
 *    argument in the string.  An "argument" is text bounded by the white space
 *    (blanks or tabs) or commas.  GETARG uses the input value of the LENGTH
 *    argument to advance past the previous argument; on the first call to
 *    GETARG for a given string, set LENGTH equal to -1:
 *
 *            extern  char  *getarg () ;
 *            char  *arg ;
 *            int  length ;
 *            ...
 *            length = -1 ;
 *            arg = string_to_scan ;
 *            while ((arg = getarg (arg, &length)) != NULL) {
 *                ... Process ARG[0:length-1] ...
 *            }
 *
 *    GETARG detects empty arguments (consecutive commas, possibly with
 *    intervening white space) and returns a length of zero for the empty
 *    argument; a comma at the end of the line indicates a final empty
 *    argument.
 *
 *    NOTE that when the string has been completely scanned, GETARG will
 *    continually return a NULL pointer.
 *
 *    Invocation:
 *        argument = getarg (string, &length);
 *
 *    where
 *
 *        <string>
 *            is a pointer to the character string to be scanned.  On the
 *            first call to GETARG for a given string, STRING should point
 *            to the beginning of the string and LENGTH should equal -1.  On
 *            subsequent calls to GETARG for the same string, STRING should
 *            be updated to point to the last argument processed (see the
 *            example above).
 *        <length>
 *            is the address of a LENGTH variable that is, on input, the
 *            length of the last argument scanned.  Set LENGTH to -1 on the
 *            first call to GETARG for a given string; GETARG will then update
 *            LENGTH on subsequent calls.  On returning from GETARG, LENGTH
 *            returns the length of the new argument; zero is returned for
 *            empty arguments (indicated by consecutive commas).
 *        <argument>
 *            returns a pointer to the new argument.  Since ARGUMENT points
 *            to the location of the argument in the full string buffer, the
 *            argument is probably not null-terminated; the Standard C Library
 *            functions STRNCMP and STRNCPY can be used to compare and copy
 *            the argument.  ARGUMENT should be passed in as STRING on the
 *            next call to GETARG (see the example).
 *
 */

char  *getarg (const char *arg, int *length)
{    
    int  comma = 0;
  
    if (arg == NULL)  
        return (NULL);
    /* Advance past the previous argument. */
    if (*length >= 0) {
        arg = arg + *length;
        arg = arg + strspn (arg, " \t");	/* Skip white space that trails previous argument. */
        if (*arg == ',') {			/* Skip comma delimiter from previous argument. */
            arg++;  comma = 1;
        }
    }

    /* Locate the next argument.  A comma at the end of the string indicates a
     *	final empty argument. 
     */

    arg = arg + strspn (arg, " \t");	/* Skip leading white space from next argument. */
    *length = strcspn (arg, " \t,");	/* Locate the next argument delimiter. */
    *length = strTrim ((char *) arg,	/* Trim trailing white space. */
                       *length);
  
    if ((*arg == '\0') && !comma)
        return (NULL);
    else
        return ((char *) arg);
}

/*
 *    getfield - Get Next Field from Database Dump Record.
 *
 *    Function GETFIELD is used to scan a database dump file record.  The
 *    calling program must read the record from the file into a string buffer.
 *    GETFIELD is then called to retrieve each field in turn from the record.
 *    The following example reads a single record from the database dump file
 *    and displays each field in the record:
 *
 *            char  buffer[128], *s;
 *            int  field_length, length;
 *
 *            ... read a record into BUFFER ...
 *
 *            field_length = 0;
 *            s = getfield (buffer, &field_length);
 *            while (field_length > 0) {
 *                length = strTrim (s, field_length);	-- Trim trailing blanks.
 *                printf ("Field = %.*s\n", length, s);
 *                s = getfield (s, &field_length);
 *            }
 *
 *    The function value returned by GETFIELD is a pointer to the start of the
 *    field in the calling program's string buffer; LENGTH returns the length
 *    of the field.  NOTE that GETFIELD doesn't null-terminate the field - you
 *    need to manipulate the field yourself; e.g., use strCopy(), strncmp(3),
 *    strncpy(3), etc.  Makes you miss FORTRAN-77!
 *
 *    GETFIELD is intended as a common routine to be used by any programs
 *    that need to input and process a database dump file.  If you change
 *    database programs and the dump file format changes, you'll only need
 *    to change GETFIELD - the applications program won't need to be changed
 *    (I hope!).
 *
 *    Invocation:
 *        char  *last_field, *next_field, record[N];  int  length;
 *        last_field = record;  length = 0;	-- Initial values.
 *        ...
 *        next_field = getfield (last_field, &length);
 *
 *    where
 *        <last_field>
 *            points to the start of the last field returned by GETFIELD;
 *            when beginning the scan of a record, set LAST_FIELD to point
 *            to the beginning of the buffer containing the record.
 *        <length>
 *            on input, is the length of the last field returned by GETFIELD;
 *            set it to 0 when beginning the scan of a record.  On output,
 *            LENGTH returns the length of the next field, excluding the
 *            beginning and ending field delimiters (if any).
 *        <next_field>
 *            returns a pointer to the start of the next field.  The pointer
 *            points into the same buffer that LAST_FIELD points into; the
 *            new field is NOT null-terminated.
 *
 **/
#define  DIVIDER  '|'			/* Divides fields in database dump file. */

char  *getfield (const char *s, int *length)
{
    char  *bofield, *eofield;


    bofield = (char *) s + *length;		/* Beginning of field. */
    if (*bofield == DIVIDER)
        bofield++;

    *length = strlen (bofield);		/* No delimiter at end of string? */

    eofield = strchr (bofield, DIVIDER);	/* End of field. */
    if (eofield != NULL)  
        *length = (int)(eofield - bofield);

    return (bofield);

}

/*
 *    getstring - Get the Next (Possibly Quote-Delimited) Argument from a
 *         String.
 *
 *    Function GETSTRING scans the next argument in a string and returns the
 *    length of the raw argument.  GETSTRING() is intended for stepping through
 *    the arguments in a command line string, taking into account the quoting
 *    conventions of the command line.  For example, GETSTRING() will treat the
 *    following as single arguments:
 *
 *                            ab
 *                            "ab cd"
 *                            'ab cd'
 *                            ab"cd"
 *                            "ab"'cd'
 *
 *    The LENGTH value on input is added to the LAST_ARGUMENT pointer to
 *    determine the start of the scan.  Prior to the first call to GETSTRING()
 *    for a given command line string, LENGTH must be set to zero, as shown in
 *    the following example:
 *
 *            char  *quotes = "\"'";	-- Single and double quotes.
 *            char  *arg, *command_line;
 *            int  length;
 *            ...
 *            length = 0;
 *            arg = getstring (command_line, quotes, &length);
 *            while (length > 0) {
 *                ... Process COMMAND_LINE(arg:arg+length-1) ...
 *                arg = getstring (command_line, quotes, &length);
 *            }
 *
 *    When the command line string has been completely scanned, GETSTRING()
 *    will continually return a pointer to the null terminator at the end of
 *    the string and a length of zero.
 *
 *    Invocation:
 *        argument = getstring (last_argument, quotes, &length);
 *
 *    where:
 *        <last_argument>	- I
 *            is a pointer to the last argument scanned.  Initially, this should
 *            point to the beginning of the command line string and the LENGTH
 *            argument (see below) should be zero.
 *        <quotes>	- I
 *            is a pointer to a character string that contains the allowable
 *            quote characters.  For example, single and double quotes (the UNIX
 *            shell quote characters) would be specified as "\"'".  If a left
 *            brace, bracket, or parenthesis is specified, GETSTRING() is smart
 *            enough to look for the corresponding right brace, bracket, or
 *            parenthesis.  The quote string can be changed in between calls
 *            to GETSTRING().
 *        <length>	- I/O
 *            is the address of a LENGTH variable that is, on input, the
 *            length of the last argument scanned.  Initially zero, LENGTH is
 *            added to LAST_ARGUMENT to determine the start of the scan.  On
 *            output, LENGTH returns the length of the next argument.
 *        <argument>	- O
 *            returns a pointer to the next argument.  Since ARGUMENT points
 *            to the location of the argument in the full string buffer, the
 *            argument is probably not null-terminated; the Standard C Library
 *            functions STRNCMP and STRNCPY can be used to compare and copy the
 *            argument.  ARGUMENT should be passed in as LAST_ARGUMENT on the
 *            next call to GETSTRING().
 *
 */

char  *getstring (const char *last_argument, const char *quotes, int *length)
{   
    char  *arg, *s;
    char  rh_quote;

    if (last_argument == NULL) {
        *length = 0;  
        return (NULL);
    }
    if (quotes == NULL)
        quotes = "";

    /* Advance past the previous argument to the start of the next argument. */
    arg = (char *) last_argument;
    if (*length > 0)
        arg = arg + *length;
    arg = arg + strspn (arg, " \t");		/* Skip white space that
                                               trails previous argument. */
    /* Scan the new argument and determine its length. */
    for (s = arg;  *s != '\0';  s++) {
        if ((*s == ' ') || (*s == '\t'))	/* Argument-terminating white space? */
            break;
        if (strchr (quotes, *s) == NULL)	/* Non-quote character? */
            continue;

        switch (*s) {				/* Determine right-hand quote. */
        case '{':  rh_quote = '}';  break;
        case '[':  rh_quote = ']';  break;
        case '(':  rh_quote = ')';  break;
        default:
            rh_quote = *s;  break;
            break;
        }

        s = strchr (++s, rh_quote);		/* Locate right-hand quote. */
        if (s == NULL)
            s = arg + strlen(arg) - 1;

    }

    /* Return the new argument and its length to the calling routine. */
    *length = s - arg;
    return (arg);
}

/*
 *    getword - scans a string and returns the location and length of the next word
 *    in the string.  A "word" is text bounded by the specified delimiters.  The
 *    LENGTH value on input is added to the string pointer to determine the start
 *    of the scan;  prior to the first call to GETWORD for a given string, set
 *    LENGTH equal to zero:
 *
 *            extern  char  *getword ();
 *            char  *delimiters = " \t,";	-- Blanks and tabs.
 *            char  *word;
 *            int  length;
 *            ...
 *            length = 0;
 *            word = string_to_scan;
 *            while (strlen (word) > 0) {
 *                word = getword (word, delimiters, &length);
 *                ... Process STRING(word:word+length-1) ...
 *            }
 *
 *    NOTE that when the string has been completely scanned, GETWORD will
 *    continually return a pointer to the null terminator in the string and
 *    a length of zero.
 *
 *    Invocation:
 *        word = getword (string, delimiters, &length);
 *
 *    where
 *        <string>
 *            is a pointer to the character string to be scanned.  The
 *            value of LENGTH on input is added to this pointer to determine
 *            the first character to be scanned.  NOTE that the pointer
 *            passed in for STRING should initially point to the full string
 *            being scanned and should be updated on each call to GETWORD (see
 *            the example above).
 *        <delimiters>
 *            is a pointer to a character string that contains the characters
 *            that are possible delimiters.  For example, if commas and blanks
 *            are your delimiters, pass in " ," as the delimiter string.  The
 *            delimiter string can be changed between calls to GETWORD.
 *        <length>
 *            is the address of a LENGTH variable that is, on input, the
 *            length of the last word scanned.  Initially zero, LENGTH is
 *            added to STRING to determine the start of the scan.  On output,
 *            LENGTH returns the length of the new "word".
 *        <word>
 *            is a pointer to the new "word".  Since WORD points to the
 *            location of the "word" in the full string buffer, the "word"
 *            is probably not null-terminated; the Standard C Library
 *            functions STRNCMP and STRNCPY can be used to compare and
 *            copy the "word".  WORD should be passed in as STRING on the
 *            next call to GETWORD (see the example).
 *
 */
char  *getword (const char *string, const char *delimiters, int *length)
{
  char  *s;

  s = (char *) string + *length;
  s = s  +  strspn(s, delimiters);
  *length = strcspn(s, delimiters);
  return (s);
}
