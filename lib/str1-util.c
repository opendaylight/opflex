/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *	str_util.c
 *    These are a collection of the string manipulation functions.
 *    Also, see the GET_UTIL functions.
 *
 *	Notes:
 *    The following changes have been made:
 *
 *      - Functions that take a length argument used to follow a convention
 *        that, if the length were zero, the function would determine the
 *        length by scanning the string for a null terminator.  This turned
 *        out to be a real nuisance if you had a need to handle zero-length
 *        strings ("").  The new convention is that,if the length argument
 *        is less than zero, the function will determine the length itself.
 *
 *    These functions are reentrant under VxWorks (except for the global
 *    debug flag).
 *
 *	Procedures:
 *    strCat() - a "logical" version of Standard C Library function strcat(3).
 *    strConvert() - scans a text string, converting "\<num>" sequences to
 *        the appropriate binary bytes.
 *    strCopy() - a "logical" version of Standard C Library function strcpy(3).
 *    strDestring() - resolves quote-delimited elements in a string.
 *    strDetab() - converts tabs in a string to blanks.
 *    strEnv() - translates environment variable references in a string.

 *    strEtoA() - converts a string of EBCDIC characters to ASCII.
 *    strInsert() - inserts a substring in a string.
 *    strMatch() - a string compare function that handles abbreviations.
 *    strRemove() - removes a substring from a string.
 *    strToLower() - converts a string to all lower-case.
 *    strToUpper() - converts a string to all upper-case.
 *    strTrim() - trims trailing tabs and spaces from a string.
 *    strdup() - duplicates a null-terminated string.
 *    strndup() - duplicates a string of a specified length.
 */


#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !__STDC__ && defined(sun)
#    define  memmove(dest,src,length)  bcopy(src,dest,length)
#endif

#include <gendcls.h> 
#include "meo-util.h"
#include "str1-util.h"
#include "debug.h"
#include "util.h"
#include "get-util.h"

/*
 *  Protos 
 */
char *strCopy(const char *source, int length, char destination[],
              size_t maxLength);

size_t strInsert (const char *substring, int subLength, size_t offset,
                  char *string, size_t maxLength);
size_t strRemove(size_t numChars,size_t offset,char *string);
size_t strTrim(char *string, int length);
int strMatch(const char *target, const char *model);

int  str_util_debug = 0 ;		/* Global debug switch (1/0 = yes/no). */

/*
 *    strCat () - catenates strings by length.
 *
 *    Invocation
 *        string = strCat (source, length, destination, maxLength) ;
 *
 *    where
 *        <source>	- I
 *            is the string to be added onto the end of the destination string.
 *        <length>	- I
 *            specifies the length of the source string.  If LENGTH is
 *            less than zero, strCat() determines the length by scanning
 *            the string for a null terminator.
 *        <destination>	- I/O
 *            is the string onto which the source string is appended.  The
 *            destination string must be null-terminated.
 *        <maxLength>	- I
 *            specifies the maximum size of the destination string that is
 *            to be extended by the source string.
 *        <string>	- O
 *            returns a pointer to the catenated string, i.e., the destination
 *            string.  The catenated string is always null-terminated, even if
 *            the source string has to be truncated (see strCopy()).
 *
 */

char *strCat (const char *source, int length, char destination[], 
              size_t maxLength)
{
    if (source == NULL)
        return (destination) ;
    if (length < 0) 
        length = strlen (source) ;

    strCopy(source, length,
            &destination[strlen (destination)],
            maxLength - strlen (destination)) ;
    return (destination) ;
}

/*
 *    strCopy () - 
 *    Function strCopy() copies strings by length.  The source string length
 *    specifies the length of the source string; the string need not be
 *    null-terminated.  The destination string length specifies the size of
 *    the character array that is to receive the copied string; even if the
 *    source string must be truncated, the destination string is always
 *    null-terminated.
 *
 *    Invocation:
 *        string = strCopy (source, length, destination, maxLength) ;
 *
 *    where
 *        <source>	- I
 *            is the string to be copied to the destination string.
 *        <length>	- I
 *            specifies the length of the source string.  If LENGTH is less
 *            than zero, strCopy() determines the length by scanning the
 *            string for a null terminator.
 *        <destination>	- O
 *            receives the copy of the source string.
 *        <maxLength>	- I
 *            specifies the maximum size of the destination string.
 *        <string>	- O
 *            returns a pointer to the copied string, i.e., the destination
 *            string.  The copied string is always null-terminated, even if
 *            the source string has to be truncated.
 *
 */
char *strCopy (const char *source, int length, char destination[],
               size_t maxLength)
{
    if (destination == NULL) {
        errno = EINVAL ;
        DEBUG_OUT(1, ("(strCopy) Attempted copy to a NULL destination.\n")) ;
        return (NULL) ;
    }
    if (source == NULL)
        return (strcpy (destination, "")) ;

    if (length < 0)
        length = strlen (source) ;

    length = (length < (int) maxLength) ? length : maxLength - 1 ;
    strncpy (destination, source, length) ;
    destination[length] = '\0' ;

    return (destination) ;
}

/*
 *    strDestring ()Resolve Quote-Delimited Elements in a String.
 *
 *    Function strDestring() scans a string, replacing quote-delimited
 *    substrings by the text within the quotes.  For example, assuming
 *    that the allowed quote characters were single quotes, double quotes,
 *    and curly braces, the following conversions would be produced by
 *    strDestring():
 *
 *                ab		==>	ab
 *		"ab cd"		==>	ab cd
 *		ab"cd"		==>	abcd
 *		"ab"'cd'	==>	abcd
 *		"ab'cd"		==>	ab'cd
 *		{ab"Hello!"cd}	==>	ab"Hello!"cd
 *
 *
 *    Invocation:
 *        result = strDestring (string, length, quotes) ;
 *
 *    where:
 *        <string>	- I
 *            is the string to be "destring"ed.
 *        <length>	- I
 *            is the length of the string to be "destring"ed.  If LENGTH
 *            is less than 0, the input string (STRING) is assumed to be
 *            null-terminated and the processing of the string will be done
 *            in place (i.e., the input string will be modified).  If LENGTH
 *            is greater than or equal to 0, then it specifies the number of
 *            characters of STRING that are to be processed.  In the latter
 *            case, strDestring() will dynamically allocate new storage to
 *            hold the processed string; the input string will not be touched.
 *        <quotes>	- I
 *            is a pointer to a character string that contains the allowable
 *            quote characters.  For example, single and double quotes (the UNIX
 *            shell quote characters) would be specified as "\"'".  If a left
 *            brace, bracket, or parenthesis is specified, strDestring() is
 *            smart enough to look for the corresponding right brace, bracket,
 *            or parenthesis.
 *        <result>	- O
 *            returns a pointer to the processed string.  If the LENGTH
 *            argument was less than zero, the "destring"ing was performed
 *            directly on the input string and RESULT simply returns the
 *            input STRING argument.  If the LENGTH argument was greater
 *            than or equal to zero, then the "destring"ing was performed
 *            on a copy of the input string and RESULT returns a pointer
 *            to this dynamically-allocated string.  In the latter case,
 *            the calling routine is responsible for FREE(3)ing the result
 *            string.  A static empty string ("") is returned in the event
 *            of an error.
 *
 */
char *strDestring (char *string, int length, const char *quotes)
{
    char  *eos, rh_quote, *s ;

    if (string == NULL)  return ("") ;
    if (quotes == NULL)  quotes = "" ;

    if (length >= 0) {				/* Make copy of input string. */
        s = strndup (string, length) ;
        if (s == NULL) {
            DEBUG_OUT(0, ("(strDestring) Error duplicating: \"%*s\"\nstrndup: ",
                     length, string)) ;
            return (NULL) ;
        }
        string = s ;
    }

	/* Scan the new argument and determine its length. */
    for (s = string ;  *s != '\0' ;  s++) {
        if (strchr (quotes, *s) == NULL)	/* Non-quote character? */
            continue ;

        switch (*s) {				/* Determine right-hand quote. */
        case '{':  rh_quote = '}' ;  break ;
        case '[':  rh_quote = ']' ;  break ;
        case '(':  rh_quote = ')' ;  break ;
        default:
            rh_quote = *s ;  break ;
            break ;
        }

        eos = strchr (s+1, rh_quote) ;		/* Locate right-hand quote. */
        if (eos == NULL)			/* Assume quote at null terminator. */
            eos = s + strlen (s) ;
        else					/* Pull down one character. */
            memmove (eos, eos+1, strlen (eos+1) + 1) ;

        memmove (s, s+1, strlen (s+1) + 1) ;	/* Pull down one character. */
        s = eos - 2 ;				/* 2 quotes gone! */
    }

	/* Return the processed string to the calling routine. */
    return (string) ;
}

/*
 *    strDetab () converts tabs in a string to blanks.
 *
 *    Invocation:
 *        detabbedLength = strDetab (stringWithTabs, length, tabStops,
 *                                   stringWithoutTabs, maxLength) ;
 *
 *    where
 *        <stringWithTabs>	- I/O
 *            is a pointer to the string containing tabs.
 *        <length>		- I
 *            specifies the length of the string containing tabs.  If LENGTH
 *            is less than zero, strDetab determines the length by scanning
 *            STRING_WITH_TABS for a terminating null character.
 *        <tabStops>		- I
 *            specifies the number of characters between tab stops.  The
 *            default is 8 characters.
 *        <stringWithoutTabs>	- I/O
 *            is a pointer to a string buffer that will receive the expanded
 *            string.  The string will always be null-terminated (and truncated
 *            to a length of MAX_LENGTH-1 if necessary).  If this argument is
 *            NULL, strDetab() performs the conversion in place on
 *            STRING_WITH_TABS, subject to the MAX_LENGTH restriction.
 *        <maxLength>		- I
 *            is the size of the STRING_WITHOUT_TABS buffer that will receive
 *            the expanded string.  If the STRING_WITHOUT_TABS pointer is NULL,
 *            then MAX_LENGTH specifies the maximum size of the STRING_WITH_TABS
 *            buffer.
 *        <detabbedLength>	- O
 *            returns the length of the expanded string.
 *
 */
size_t strDetab (char *stringWithTabs, int length, int tabStops,
                 char *stringWithoutTabs, size_t maxLength)
{
    char  *s ;
    int  i, numSpaces ;

    if (stringWithTabs == NULL) {
        if (stringWithoutTabs != NULL)
            *stringWithoutTabs = '\0' ;
        return (0) ;
    }

    if (length < 0)
        length = strlen (stringWithTabs) ;
    if (tabStops <= 0)
        tabStops = 8 ;

    if (stringWithoutTabs == NULL)
        stringWithoutTabs = stringWithTabs ;
    else
        strCopy (stringWithTabs, length, stringWithoutTabs, maxLength) ;

	/* For each tab character in the string, delete the tab character and insert
   		the number of spaces necessary to shift the following text to the next
   		tab stop. */

    for (i = 0, s = stringWithoutTabs ;  *s != '\0' ;  i++, s++) {
        if (*s != '\t')  continue ;
        numSpaces = tabStops - (i % tabStops) - 1 ;  *s = ' ' ;
        if (numSpaces > 0) {
            numSpaces = strInsert (NULL, numSpaces, 0, s, maxLength - i) ;
            s = s + numSpaces ;  i = i + numSpaces ;
        }
    }
    return (strTrim (stringWithoutTabs, -1)) ;
}

/*
 *    strEnv () 
 *    Function strEnv() translates environment variables ("$<name>") and
 *    home directory references ("~") embedded in a string.  For example,
 *    if variable DG has been defined as "/usr/dekehn/dispgen", strEnv()
 *    will translate
 *                    "tpocc:$DG/page.tdl"
 *
 *    as
 *                    "tpocc:/usr/dekehn/dispgen/page.tdl".
 *
 *    Remember that C-Shell variables (defined using "set name = value") are
 *    NOT environment variables (defined using "setenv name value") and are
 *    NOT available to programs.  Define any variables you might need as
 *    environment variables.
 *
 *    Environment variables can be nested, i.e., defined in terms of each other.
 *    Undefined environment variables are not an error and are assumed to have
 *    a value of "" (a zero-length string).
 *
 *    Invocation:
 *        strEnv (string, length, &translation, maxLength) ;
 *
 *    where
 *        <string>	- I
 *            is the string which contains environment variable references.
 *        <length>	- I
 *            is the length of the string.  If LENGTH is less than zero,
 *            strEnv() determines the length by scanning STRING for a
 *            terminating null character.
 *        <translation>	- O
 *            is the address of a buffer which will receive the translated
 *            string.
 *        <maxLength>	- I
 *            is the maximum length of the translation; i.e., the size of
 *            the translation buffer.
 *
 */
void strEnv (const char *string, int length, char *translation,
             size_t maxLength)
{
    char  follow, *name, *s ;
    int  i ;

    if (translation == NULL)  return ;
    if (string == NULL) {
        strcpy (translation, "") ;
        return ;
    }
    strCopy (string, length, translation, maxLength) ;

	/* Scan through the string, replacing "~"s by the user's home directory and
   		environment variables ("$<name>") by their values. */
    for (i = 0 ;  translation[i] != '\0' ;  ) {
        if (translation[i] == '~') {			/* "~" */
            s = getenv ("HOME") ;			/* Get home directory. */
            if (s == NULL) {
                i++ ;					/* Insert "~". */
            } 
            else {
                strRemove (1, i, translation) ;		/* Insert home directory. */
                strInsert (s, -1, i, translation, maxLength) ;
            }

        } 
        else if (translation[i] == '$') {		/* "$<name>" */
            length = 0 ;				/* Extract "<name>". */
            name = (char *) getword (&translation[i], "$./", &length) ;
            follow = name[length] ;  name[length] = '\0' ;
            s = getenv (name) ;				/* Lookup "<name>". */
            name[length] = follow ;
							/* Replace "$<name>" ... */
            strRemove (name - &translation[i] + length, i, translation) ;
            if (s != NULL)				/* ... by "<value>". */
                strInsert (s, -1, i, translation, maxLength) ;

        }
        else {					/* Normal character. */
            i++ ;
        }
    }
    return ;
}

/*
 *
 *    strEtoA ()
 *    Function strEtoA() converts an EBCDIC string to an ASCII string.  The
 *    conversion table for this program was created using the "dd conv=ascii"
 *    Unix program, so I hope it's right!
 *
 *
 *    Invocation:
 *        asciiString = strEtoA (ebcdicString, length) ;
 *
 *    where
 *        <ebcdicString>	- I/O
 *            is a pointer to the EBCDIC string to be converted.  The
 *            EBCDIC-to-ASCII conversion is done in-place, so EBCDIC_STRING
 *            should be in writeable memory.
 *        <length>	- I
 *            specifies the length of the EBCDIC string.  If LENGTH is
 *            less than zero, strEtoA() determines the length by scanning
 *            EBCDIC_STRING for a terminating null character.
 *        <asciiString>	- O
 *            returns a pointer to the converted string.  Since the EBCDIC-to-
 *            ASCII conversion is done in-place, this pointer simply points to
 *            the input string, EBCDIC_STRING.
 *
 */
char *strEtoA(char *string, int length)
{
    static  char  ebcdic_to_ascii[256] = {
        0, 1, 2, 3, -100, 9, -122, 127, -105, -115, -114, 11, 12, 13, 14, 15,
        16, 17, 18, 19, -99, -123, 8, -121, 24, 25, -110, -113, 28, 29, 30, 31,
        -128, -127, -126, -125, -124, 10, 23, 27, -120, -119, -118, -117, -116, 5, 6, 7,
        -112, -111, 22, -109, -108, -107, -106, 4, -104, -103, -102, -101, 20, 21, -98, 26,
        32, -96, -95, -94, -93, -92, -91, -90, -89, -88, 91, 46, 60, 40, 43, 33,
        38, -87, -86, -85, -84, -83, -82, -81, -80, -79, 93, 36, 42, 41, 59, 94,
        45, 47, -78, -77, -76, -75, -74, -73, -72, -71, 124, 44, 37, 95, 62, 63,
        -70, -69, -68, -67, -66, -65, -64, -63, -62, 96, 58, 35, 64, 39, 61, 34,
        -61, 97, 98, 99, 100, 101, 102, 103, 104, 105, -60, -59, -58, -57, -56, -55,
        -54, 106, 107, 108, 109, 110, 111, 112, 113, 114, -53, -52, -51, -50, -49, -48,
        -47, 126, 115, 116, 117, 118, 119, 120, 121, 122, -46, -45, -44, -43, -42, -41,
        -40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25,
        123, 65, 66, 67, 68, 69, 70, 71, 72, 73, -24, -23, -22, -21, -20, -19,
        125, 74, 75, 76, 77, 78, 79, 80, 81, 82, -18, -17, -16, -15, -14, -13,
        92, -97, 83, 84, 85, 86, 87, 88, 89, 90, -12, -11, -10, -9, -8, -7,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -6, -5, -4, -3, -2, -1
    } ;
    char  *s ;
    if (length < 0)
        length = strlen (string) ;

    for (s = string ;  length-- ;  s++)
        *s = ebcdic_to_ascii[0x00FF&*s] ;
    return (string) ;
}

/*
*    strInsert () - inserts N characters of text at any position in
*    a string.
*
*    Invocation:
*        numInserted = strInsert (substring, length, offset, string, maxLength) ;
*
*    where
*        <substring>	- I
*            points to the substring that will be inserted in STRING.  If
*            this argument is NULL, then LENGTH blanks will be inserted in
*            STRING.
*        <length>	- I
*            is the length of SUBSTRING.  If LENGTH is less than zero, the
*            length is determined by searching for the null terminator in
*            SUBSTRING.
*        <offset>	- I
*            is the character offset (0..N-1) in STRING at which SUBSTRING
*            will be inserted.
*        <string>	- I/O
*            points to the string into which text will be inserted.
*        <maxLength>	- I
*            is the size of the STRING buffer.  Text that would be shifted
*            beyond the end of STRING is truncated.
*        <numInserted>	- O
*            returns the number of characters actually inserted.  Normally,
*            this will just be the length of SUBSTRING.  If, however, the
*            size of the STRING buffer is insufficient to accomodate the
*            full shift required for the insertion, NUM_INSERTED will be
*            less than the length of SUBSTRING.
*
*/
size_t strInsert(const char *substring, int subLength, size_t offset,
                 char *string, size_t maxLength)
{
    char  *s ;
    int  length ;

	/* Make sure arguments are all valid. */
    if (string == NULL) 
        return (0) ;
    if ((substring != NULL) && (subLength < 0))
        subLength = strlen (substring) ;
    if (subLength == 0)
        return (0) ;

	/* Compute the number of characters following STRING[OFFSET] that can be
   		shifted right to make room for SUBSTRING.  Stored in variable LENGTH,
   		the number computed includes the null terminator at the end of the
   		string (or an extraneous character if truncation will occur). */
    length = offset + subLength + strlen (&string[offset]) + 1 ;
    if (length > (int) maxLength)
        length = maxLength ;
    length = length - subLength - offset ;

	/* If there is room enough in the string buffer for the substring to
   		be inserted, then insert it.  Text following STRING[OFFSET] may be
   		truncated, if necessary. */
    if (length > 0) {		/* Shift text N columns to the right. */
        for (s = &string[offset+length-1] ;  length-- > 0 ;  s--)
            s[subLength] = *s ;
        s = s + subLength ;
    }

	/* If there is insufficient room in the string buffer to insert the full
   		text of the substring, then insert whatever will fit from the substring.
   		The original text following STRING[OFFSET] will be lost. */
    else {
        subLength = subLength + length - 1 ;
        s = &string[offset+subLength-1] ;
    }

	/* Copy the substring into the string.  Variable S points to the end of the
   		room made for inserting the substring.  For example, if the substring will
   		be copied into character positions 2-7 of the target string, then S points
   		to character position 7.  Variable SUB_LENGTH specifies the number of
   		characters to copy from SUBSTRING. */
    length = subLength ;
    if (substring == NULL) {	/* Insert N blanks? */
        while (length-- > 0)
            *s-- = ' ' ;
    } 
    else {				/* Insert N characters of text? */
        while (length-- > 0)
            *s-- = substring[length] ;
    }
    string[maxLength-1] = '\0' ;	/* Ensure null-termination in case of truncation. */

	/* This function was extremely tedious to write.  The next time you extol the
   		virtues of C, remember that this function would have been a one-liner in
   		FORTRAN 77 or BASIC. */
    return (subLength) ;
}

/*
 *    strMatch ()
 *    Function strMatch() matches a possibly-abbreviated target string against
 *    a model string.  For example, "C", "CO", "COO", and "COOK" are partial
 *    matches of "COOK"; "COOKS" is NOT a partial match of "COOK".
 *
 *    Invocation:
 *        found = strMatch (target, model) ;
 *
 *    where
 *        <target>
 *            is the string to be checked for matching against the model string.
 *            In the example above, "C", "CO", "COO", etc. are target strings.
 *        <model>
 *            is the model string against which the match is tested.  In the
 *            example above, "COOK" is the model string.
 *        <found>
 *            returns true (a non-zero value) if the target string is a partial
 *            or full match of the model string; false (zero) is returned if the
 *            target string bears no relation to the model string.
 *
 */

int strMatch (const char *target, const char *model)
{
    int  length ;

    length = strlen (target) ;
    if (length > (int) strlen (model))
        return (0) ;				/* Target string is too long. */
    else if (strncmp (target, model, length))
        return (0) ;				/* No match. */
    else
        return (1) ;				/* Matched! */
}

/*
 *    strRemove () removes N characters of text from any position in
 *    a string.
 *
 *    Invocation:
 *        length = strRemove (numToRemove, offset, string) ;
 *
 *    where
 *        <numToRemove>	- I
 *            is the number of characters to delete from the string.
 *        <offset>	- I
 *            is the character offset (0..N-1) in STRING at which the
 *            deletion will take place.
 *        <string>	- I/O
 *            points to the string from which text will be deleted.
 *        <length>	- O
 *            returns the length of the string after the deletion.
 *
 */

size_t strRemove (size_t numToRemove, size_t offset, char *string)
{
    int  length ;

	/* Validate the arguments. */
    if (string == NULL)
        return (0) ;
    length = strlen (string) ;
    if ((int) offset >= length)
        return (0) ;
    if (numToRemove > (length - offset))
        numToRemove = length - offset ;

	/* Remove the substring. */
    memmove (&string[offset], &string[offset+numToRemove],
             length - (offset + numToRemove) + 1) ;
    return (strlen (string)) ;
}

/*
 *    strToLower ()
 *    Function strToLower() converts the characters in a string to lower
 *    case.  If the length argument is zero, the string is assumed to be
 *    null-terminated; otherwise, only LENGTH characters are converted.
 *
 *    Invocation:
 *        result = strToLower (string, length) ;
 *
 *    where
 *        <string>
 *            points to the string to be converted; the conversion is
 *            done in-place.
 *        <length>
 *            is the number of characters to be converted.  If LENGTH is less
 *            than zero, the entire string is converted up to the null terminator.
 *        <result>
 *            returns a pointer to the converted string; i.e., STRING.
 *
 */
char *strToLower (char *string, int  length)
{
    char  *s ;

    if (length < 0)
        length = strlen (string) ;

    for (s = string ;  length-- > 0 ;  s++)
        if (isupper (*s))  *s = tolower (*s) ;

    return (string) ;
}

/*
 *    strToUpper ()
 *    Function strToUpper() converts the characters in a string to upper
 *    case.  If the length argument is zero, the string is assumed to be
 *    null-terminated; otherwise, only LENGTH characters are converted.
 *
 *    Invocation:
 *        result = strToUpper (string, length) ;
 *
 *    where
 *        <string>
 *            points to the string to be converted; the conversion is
 *            done in-place.
 *        <length>
 *            is the number of characters to be converted.  If LENGTH is less
 *            than zero, the entire string is converted up to the null terminator.
 *        <result>
 *            returns a pointer to the converted string; i.e., STRING.
 *
 */
char *strToUpper (char *string, int length)
{
    char  *s ;

    if (length < 0)
        length = strlen (string) ;

    for (s = string ;  length-- > 0 ;  s++)
        if (islower (*s))  *s = toupper (*s) ;

    return (string) ;
}

/*
 *    strTrim ()
 *    Function strTrim() trims trailing white space (blanks, tabs, and new-line
 *    characters) from a string.  If the length argument is less than zero, the
 *    string is assumed to be null-terminated; after trimming trailing white
 *    space, the null terminator is relocated to the new end of the string.  If
 *    the length argument is greater than or equal to zero, the string does NOT
 *    need to be null-terminated; after trimming trailing white space, the null
 *    terminator is NOT relocated.  In either case, strTrim() returns the length
 *    of the new string.
 *
 *    Invocation:
 *            trimmedLength = strTrim (string, length) ;
 *
 *    where
 *        <string>	- I/O
 *            is the string to be trimmed.  If the length argument is less
 *            than zero, STRING is assumed to be null-terminated and strTrim()
 *            will *** relocate the null terminator ***.  If LENGTH is
 *            greater than or equal to zero, strTrim() will not relocate the
 *            null terminator; it simply computes the trimmed length.
 *        <length>	- I
 *            is the length, before trimming, of STRING.  If LENGTH is less
 *            than zero, STRING is assumed to be null-terminated.
 *        <trimmedLength>	- O
 *            returns the length of STRING with trailing blanks, tabs, and
 *            new-line characters removed.
 *
 */
size_t strTrim (char *string, int length)
{
    char  *s ;
    int  newLength ;

    newLength = (length < 0) ? strlen (string) : length ;
    s = string + newLength ;

    while ((s-- != string) &&
           ((*s == ' ') || (*s == '\t') || (*s == '\n')))
        newLength-- ;

    if (length < 0)
        *++s = '\0' ;

    return (newLength) ;
}

#ifdef NO_STRDUP
/*
 *    strdup () - Duplicate a Null-Terminated String.
 *    Function strdup() duplicates a null-terminated string.  strdup() is
 *    supported by many C libraries, but it is not part of the ANSI C library.
 *
 *
 *    Invocation:
 *
 *        duplicate = strdup (string) ;
 *
 *    where:
 *
 *        <string>	- I
 *            is the string to be duplicated.
 *        <duplicate>	- O
 *            returns a MALLOC(3)ed copy of the input string.  The caller
 *            is responsible for FREE(3)ing the duplicate string.  NULL is
 *            returned in the event of an error.
 *
 */
char *strdup (const char *string)

{
    char  *duplicate ;

    if (string == NULL) {
        errno = EINVAL ;
        DEBUG_OUT(1, ("(strdup) NULL string: ")) ;
        return (NULL) ;
    }

    duplicate = malloc (strlen (string) + 1) ;
    if (duplicate == NULL) {
        DEBUG_OUT(0, ("(strdup) Error duplicating %d-byte string.\n\"%s\"\nmalloc: ",
                 strlen (string) + 1, string)) ;
        return (NULL) ;
    }

    strcpy (duplicate, string) ;
    return (duplicate) ;
}
#endif

/*
 *
 *Procedure:
 *
 *    strndup ()
 *
 *    Duplicate a String of a Specified Length.
 *
 *
 *Purpose:
 *
 *    Function strndup() duplicates a string of a specified length.  strndup()
 *    is the "n" counterpart of strdup(), as in strncmp(3) and strcmp(3).
 *
 *
 *    Invocation:
 *
 *        duplicate = strndup (string, length) ;
 *
 *    where:
 *
 *        <string>	- I
 *            is the string to be duplicated.
 *        <length>	- I
 *            is the number of characters to be duplicated.
 *        <duplicate>	- O
 *            returns a MALLOC(3)ed copy of the input string; the duplicate
 *            is null terminated.  The caller is responsible for FREE(3)ing
 *            the duplicate string.  NULL is returned in the event of an error.
 *
 */
char *strndup (const char *string, size_t length)
{
    char  *duplicate ;

    duplicate = malloc (length + 1) ;
    if (duplicate == NULL) {
        DEBUG_OUT(0, ("(strndup) Error duplicating %d-byte string.\n\"%*s\"\nmalloc: ",
                      (int)length, (int)length, string)) ;
        return (NULL) ;
    }

    if (string == NULL) {
        *duplicate = '\0' ;
    }
    else {
        strncpy (duplicate, string, length) ;
        duplicate[length] = '\0' ;
    }

    return (duplicate) ;
}
