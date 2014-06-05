/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


/*
 *
 * @File str_util.c
 *
 * Description:
 *         String utils main for parsing the config file.
 *
 * Considerations:
 *
 * Module Test Plan:
 *
 * Functions:
 *			next_token() - Get the next token from a string
 *			toktocliplist() - Convert list of tokens to array
 *
 * To Do:
 *
 * Revision History:  (latest changes at top)
 *    DEK - Thur May 31 MST 2001 - created
 *
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ndmp includes */
#include <gendcls.h> 
#include <str-util.h>
#include <debug.h>

static char *last_ptr=NULL;

/*
static void set_first_token(char *ptr)
{
	last_ptr = ptr;
}
*/
int case_default = CASE_LOWER;

/*
 *  Get the next token from a string, return False if none found
 * handles double-quotes. 
 */
bool next_token(char **ptr,char *buff,char *sep, size_t bufsize)
{
  char *s;
  bool quoted;
  size_t len=1;

  if (!ptr) ptr = &last_ptr;
  if (!ptr) return(False);

  s = *ptr;

  /* default to simple separators */
  if (!sep) sep = " \t\n\r";

  /* find the first non sep char */
  while(*s && strchr(sep,*s)) s++;

  /* nothing left? */
  if (! *s) return(False);

  /* copy over the token */
  for (quoted = False; len < bufsize && *s && (quoted || !strchr(sep,*s)); s++)
    {
	    if (*s == '\"') {
		    quoted = !quoted;
	    } else {
		    len++;
		    *buff++ = *s;
	    }
    }

  *ptr = (*s) ? s+1 : s;  
  *buff = 0;
  last_ptr = *ptr;

  return(True);
}

/****************************************************************************
Convert list of tokens to array; dependent on above routine.
Uses last_ptr from above - bit of a hack.
****************************************************************************/
char **toktocliplist(int *ctok, char *sep)
{
  char *s=last_ptr;
  int ictok=0;
  char **ret, **iret;

  if (!sep) sep = " \t\n\r";

  while(*s && strchr(sep,*s)) s++;

  /* nothing left? */
  if (!*s) return(NULL);

  do {
    ictok++;
    while(*s && (!strchr(sep,*s))) s++;
    while(*s && strchr(sep,*s)) *s++=0;
  } while(*s);

  *ctok=ictok;
  s=last_ptr;

  if (!(ret=iret=malloc(ictok*sizeof(char *)))) return NULL;
  
  while(ictok--) {    
    *iret++=s;
    while(*s++);
    while(!*s) s++;
  }

  return ret;
}


/*******************************************************************
  compare 2 strings 
********************************************************************/
bool strequal(const char *s1, const char *s2)
{
  if (s1 == s2) return(True);
  if (!s1 || !s2) return(False);
  
  return(strcasecmp(s1,s2)==0);
}

/*******************************************************************
  compare 2 strings up to and including the nth char.
  ******************************************************************/
bool strnequal(const char *s1,const char *s2,size_t n)
{
  if (s1 == s2) return(True);
  if (!s1 || !s2 || !n) return(False);
  
  return(strncasecmp(s1,s2,n)==0);
}

/*******************************************************************
  compare 2 strings (case sensitive)
********************************************************************/
bool strcsequal(const char *s1,const char *s2)
{
  if (s1 == s2) return(True);
  if (!s1 || !s2) return(False);
  
  return(strcmp(s1,s2)==0);
}


/*******************************************************************
  convert a string to lower case
********************************************************************/
void strlower(char *string)
{
    char  *s ;
    int length = strlen (string) ;

    for (s = string ;  length-- > 0 ;  s++)
        if (isupper (*s))  *s = tolower (*s) ;

    //return (string) ;
}

/*******************************************************************
  convert a string to upper case
********************************************************************/
void strupper(char *string)
{
    char  *s ;
    int length = strlen (string) ;

    for (s = string ;  length-- > 0 ;  s++)
        if (islower (*s))  *s = toupper (*s) ;

    //return (string) ;
}

/*******************************************************************
  convert a string to "normal" form
********************************************************************/
void strnorm(char *s)
{
  extern int case_default;
  if (case_default == CASE_UPPER)
    strupper(s);
  else
    strlower(s);
}

/*******************************************************************
check if a string is in "normal" case
********************************************************************/
bool strisnormal(char *s)
{
  extern int case_default;
  if (case_default == CASE_UPPER)
    return(!strhaslower(s));

  return(!strhasupper(s));
}


/*******************************************************************
skip past some strings in a buffer
********************************************************************/
char *skip_string(char *buf,size_t n)
{
  while (n--)
    buf += strlen(buf) + 1;
  return(buf);
}

/*******************************************************************
 Count the number of characters in a string. Normally this will
 be the same as the number of bytes in a string for single byte strings,
 but will be different for multibyte.
 16.oct.98, jdblair@cobaltnet.com.
********************************************************************/

size_t str_charnum(const char *s)
{
  return strlen(s);
}

/*******************************************************************
trim the specified elements off the front and back of a string
********************************************************************/

bool trim_string(char *s,const char *front,const char *back)
{
  bool ret = False;
  size_t front_len = (front && *front) ? strlen(front) : 0;
  size_t back_len = (back && *back) ? strlen(back) : 0;
  size_t s_len;

  while (front_len && strncmp(s, front, front_len) == 0)
  {
    char *p = s;
    ret = True;
    while (1)
    {
      if (!(*p = p[front_len]))
        break;
      p++;
    }
  }

  if(back_len)
  {
    s_len = strlen(s);
    while ((s_len >= back_len) && 
	   (strncmp(s + s_len - back_len, back, back_len)==0))  
      {
        ret = True;
        s[s_len - back_len] = '\0';
        s_len = strlen(s);
      }
  }
  
  return(ret);
}


/****************************************************************************
does a string have any uppercase chars in it?
****************************************************************************/
bool strhasupper(const char *s)
{
  while (*s) {
    if (isupper(*s))
      return(True);
    s++;
  }  
  return(False);
}

/****************************************************************************
does a string have any lowercase chars in it?
****************************************************************************/
bool strhaslower(const char *s)
{
  while (*s) {
    if (islower(*s))
      return(True);
    s++;
  }
  return(False);
}

/****************************************************************************
find the number of chars in a string
****************************************************************************/
size_t count_chars(const char *s,char c)
{
  size_t count=0;

  while (*s)  {
    if (*s == c)
      count++;
    s++;
  }
  return(count);
}

/*******************************************************************
Return True if a string consists only of one particular character.
********************************************************************/

bool str_is_all(const char *s,char c)
{
  if(s == NULL)
    return False;

  if(!*s)
    return False;

  while (*s)    {
    if (*s != c)
      return False;
    s++;
  }
  return True;
}

/*******************************************************************
safe string copy into a known length string. maxlength does not
include the terminating zero.
********************************************************************/

char *safe_strcpy(char *dest,const char *src, size_t maxlength)
{
    size_t len;

    if (!dest) {
        DEBUG_OUT(0,("ERROR: NULL dest in safe_strcpy\n"));
        return NULL;
    }

    if (!src) {
        *dest = 0;
        return dest;
    }  

    len = strlen(src);

    if (len > maxlength) {
	    DEBUG_OUT(0,("ERROR: string overflow by %d in safe_strcpy [%.50s]\n",
		     (int)(len-maxlength), src));
	    len = maxlength;
    }
      
    memcpy(dest, src, len);
    dest[len] = 0;
    return dest;
}  

/*******************************************************************
safe string cat into a string. maxlength does not
include the terminating zero.
********************************************************************/

char *safe_strcat(char *dest, const char *src, size_t maxlength)
{
    size_t src_len, dest_len;

    if (!dest) {
        DEBUG_OUT(0,("ERROR: NULL dest in safe_strcat\n"));
        return NULL;
    }

    if (!src) {
        return dest;
    }  

    src_len = strlen(src);
    dest_len = strlen(dest);

    if (src_len + dest_len > maxlength) {
	    DEBUG_OUT(0,("ERROR: string overflow by %d in safe_strcat [%.50s]\n",
		     (int)(src_len + dest_len - maxlength), src));
	    src_len = maxlength - dest_len;
    }
      
    memcpy(&dest[dest_len], src, src_len);
    dest[dest_len + src_len] = 0;
    return dest;
}

/*******************************************************************
 Paranoid strcpy into a buffer of given length (includes terminating
 zero. Strips out all but 'a-Z0-9' and replaces with '_'. Deliberately
 does *NOT* check for multibyte characters. Don't change it !
********************************************************************/

char *alpha_strcpy(char *dest, const char *src, size_t maxlength)
{
	size_t len, i;

	if (!dest) {
		DEBUG_OUT(0,("ERROR: NULL dest in alpha_strcpy\n"));
		return NULL;
	}

	if (!src) {
		*dest = 0;
		return dest;
	}  

	len = strlen(src);
	if (len >= maxlength)
		len = maxlength - 1;

	for(i = 0; i < len; i++) {
		int val = (src[i] & 0xff);
		if(isupper(val) ||islower(val) || isdigit(val))
			dest[i] = src[i];
		else
			dest[i] = '_';
	}

	dest[i] = '\0';

	return dest;
}

/****************************************************************************
 Like strncpy but always null terminates. Make sure there is room!
 The variable n should always be one less than the available size.
****************************************************************************/

char *StrnCpy(char *dest,const char *src,size_t n)
{
  char *d = dest;
  if (!dest) return(NULL);
  if (!src) {
    *dest = 0;
    return(dest);
  }
  while (n-- && (*d++ = *src++)) ;
  *d = 0;
  return(dest);
}

/****************************************************************************
like strncpy but copies up to the character marker.  always null terminates.
returns a pointer to the character marker in the source string (src).
****************************************************************************/
char *strncpyn(char *dest, const char *src,size_t n, char c)
{
	char *p;
	size_t str_len;

	p = strchr(src, c);
	if (p == NULL)
	{
	  DEBUG_OUT(5, ("strncpyn: separator character (%c) not found\n", c));
	  return NULL;
	}

	str_len = PTR_DIFF(p, src);
	strncpy(dest, src, MIN(n, str_len));
	dest[str_len] = '\0';

	return p;
}


/*************************************************************
 Routine to get hex characters and turn them into a 16 byte array.
 the array can be variable length, and any non-hex-numeric
 characters are skipped.  "0xnn" or "0Xnn" is specially catered
 for.

 valid examples: "0A5D15"; "0x15, 0x49, 0xa2"; "59\ta9\te3\n"

**************************************************************/
size_t strhex_to_str(char *p, size_t len, const char *strhex)
{
	size_t i;
	size_t num_chars = 0;
	unsigned char   lonybble, hinybble;
	char           *hexchars = "0123456789ABCDEF";
	char           *p1 = NULL, *p2 = NULL;

	for (i = 0; i < len && strhex[i] != 0; i++)
	{
		if (strnequal(hexchars, "0x", 2))
		{
			i++; /* skip two chars */
			continue;
		}

		if (!(p1 = strchr(hexchars, toupper(strhex[i]))))
		{
			break;
		}

		i++; /* next hex digit */

		if (!(p2 = strchr(hexchars, toupper(strhex[i]))))
		{
			break;
		}

		/* get the two nybbles */
		hinybble = PTR_DIFF(p1, hexchars);
		lonybble = PTR_DIFF(p2, hexchars);

		p[num_chars] = (hinybble << 4) | lonybble;
		num_chars++;

		p1 = NULL;
		p2 = NULL;
	}
	return num_chars;
}

/****************************************************************************
check if a string is part of a list
****************************************************************************/
bool in_list(char *s,char *list,bool casesensitive)
{
  pstring tok;
  char *p=list;

  if (!list) return(False);

  while (next_token(&p,tok,LIST_SEP,sizeof(tok))) {
    if (casesensitive) {
      if (strcmp(tok,s) == 0)
        return(True);
    } else {
      if (strcasecmp(tok,s) == 0)
        return(True);
    }
  }
  return(False);
}

/* this is used to prevent lots of mallocs of size 1 */
static char *null_string = NULL;

/****************************************************************************
set a string value, allocing the space for the string
****************************************************************************/
static bool string_init(char **dest,const char *src)
{
  size_t l;
  if (!src)     
    src = "";

  l = strlen(src);

  if (l == 0)
    {
      if (!null_string) {
        if((null_string = (char *)malloc(1)) == NULL) {
          DEBUG_OUT(0,("string_init: malloc fail for null_string.\n"));
          return False;
        }
        *null_string = 0;
      }
      *dest = null_string;
    }
  else
    {
      (*dest) = (char *)malloc(l+1);
      if ((*dest) == NULL) {
	      DEBUG_OUT(0,("Out of memory in string_init\n"));
	      return False;
      }

      pstrcpy(*dest,src);
    }
  return(True);
}

/****************************************************************************
free a string value
****************************************************************************/
void string_free(char **s)
{
  if (!s || !(*s)) return;
  if (*s == null_string)
    *s = NULL;
  if (*s) free(*s);
  *s = NULL;
}

/****************************************************************************
set a string value, allocing the space for the string, and deallocating any 
existing space
****************************************************************************/
bool string_set(char **dest,const char *src)
{
  string_free(dest);

  return(string_init(dest,src));
}


/****************************************************************************
substitute a string for a pattern in another string. Make sure there is 
enough room!

This routine looks for pattern in s and replaces it with 
insert. It may do multiple replacements.

any of " ; ' $ or ` in the insert string are replaced with _
if len==0 then no length check is performed

Return True if a change was made, False otherwise.
****************************************************************************/
bool string_sub(char *s,const char *pattern,const char *insert, size_t len)
{
	bool changed = False;
	char *p;
	size_t ls,lp,li, i;

	if (!insert || !pattern || !s) return False;

	ls = (size_t)strlen(s);
	lp = (size_t)strlen(pattern);
	li = (size_t)strlen(insert);

	if (!*pattern) return False;
	
	while (lp <= ls && (p = strstr(s,pattern))) {
		changed = True;
		if (len && (ls + (li-lp) >= len)) {
			DEBUG_OUT(0,("ERROR: string overflow by %d in string_sub(%.50s, %d)\n", 
				 (int)(ls + (li-lp) - len),
				 pattern, (int)len));
			break;
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		for (i=0;i<li;i++) {
			switch (insert[i]) {
			case '`':
			case '"':
			case '\'':
			case ';':
			case '$':
			case '%':
			case '\r':
			case '\n':
				p[i] = '_';
				break;
			default:
				p[i] = insert[i];
			}
		}
		s = p + li;
		ls += (li-lp);
	}

	return changed;
}

bool fstringg_sub(char *s,const char *pattern,const char *insert)
{
	return string_sub(s, pattern, insert, sizeof(fstring));
}

bool pstring_sub(char *s,const char *pattern,const char *insert)
{
	return string_sub(s, pattern, insert, sizeof(pstring));
}

/****************************************************************************
similar to string_sub() but allows for any character to be substituted. 
Use with caution!
if len==0 then no length check is performed
****************************************************************************/
bool all_string_sub(char *s,const char *pattern,const char *insert, size_t len)
{
	bool changed = False;
	char *p;
	size_t ls,lp,li;

	if (!insert || !pattern || !s) return False;

	ls = (size_t)strlen(s);
	lp = (size_t)strlen(pattern);
	li = (size_t)strlen(insert);

	if (!*pattern) return False;
	
	while (lp <= ls && (p = strstr(s,pattern))) {
		changed = True;
		if (len && (ls + (li-lp) >= len)) {
			DEBUG_OUT(0,("ERROR: string overflow by %d in all_string_sub(%.50s, %d)\n", 
				 (int)(ls + (li-lp) - len),
				 pattern, (int)len));
			break;
		}
		if (li != lp) {
			memmove(p+li,p+lp,strlen(p+lp)+1);
		}
		memcpy(p, insert, li);
		s = p + li;
		ls += (li-lp);
	}

	return changed;
}

/****************************************************************************
 splits out the front and back at a separator.
****************************************************************************/
void split_at_last_component(char *path, char *front, char sep, char *back)
{
	char *p = strrchr(path, sep);

	if (p != NULL)
	{
		*p = 0;
	}
	if (front != NULL)
	{
		pstrcpy(front, path);
	}
	if (p != NULL)
	{
		if (back != NULL)
		{
			pstrcpy(back, p+1);
		}
		*p = '\\';
	}
	else
	{
		if (back != NULL)
		{
			back[0] = 0;
		}
	}
}


/****************************************************************************
write an octal as a string
****************************************************************************/
char *octal_string(int i)
{
	static char ret[64];
	if (i == -1) {
		return "-1";
	}
	snprintf(ret, sizeof(ret), "0%o", i);
	return ret;
}


/****************************************************************************
truncate a string at a specified length
****************************************************************************/
char *string_truncate(char *s, int length)
{
	if (s && strlen(s) > length) {
		s[length] = 0;
	}
	return s;
}

/***************************************************************************
Do a case-insensitive, whitespace-ignoring string compare.
***************************************************************************/
int strwicmp(char *psz1, char *psz2)
{
   /* if BOTH strings are NULL, return TRUE, if ONE is NULL return */
   /* appropriate value. */
   if (psz1 == psz2)
      return (0);
   else
      if (psz1 == NULL)
         return (-1);
      else
          if (psz2 == NULL)
              return (1);

   /* sync the strings on first non-whitespace */
   while (1)
   {
      while (isspace(*psz1))
         psz1++;
      while (isspace(*psz2))
         psz2++;
      if (toupper(*psz1) != toupper(*psz2) || *psz1 == '\0' || *psz2 == '\0')
         break;
      psz1++;
      psz2++;
   }
   return (*psz1 - *psz2);
}
