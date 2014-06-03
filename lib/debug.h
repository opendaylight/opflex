/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */



#ifndef DEBUG_H
#define DEBUG_H


/*****************************************************************************
* @File:
*       debug.h
*
* Description:
*        Guess what its all the debug stuff for the system 
*
* Considerations:
*
* Module Test Plan:
*
* Usage:
*
* To Do:
*
* Revision History:  (latest changes at top)
*    07-MAR-2014 - dkehn@noironetworks.com - Created
*
*****************************************************************************/

#include  <stdio.h>
#include  <stdlib.h>        /* Standard C Library defitions */
#include  <ctype.h>         /* Standard character functions. */
#include  <errno.h>         /* System error definitions. */
#include  <limits.h>            /* Maximum/minimum value definitions. */
#include  <string.h>
#include  <unistd.h>        /* UNIX I/O definitions */

#include "ansi-setup.h"
#include "gendcls.h"


/* -------------------------------------------------------------------------- **
 * Debugging code.  See also debug.c
 */

/* mkproto.awk has trouble with ifdef'd function definitions (it ignores
 * the #ifdef directive and will read both definitions, thus creating two
 * diffferent prototype declarations), so we must do these by hand.
 */
/* I know the __attribute__ stuff is ugly, but it does ensure we get the 
   arguemnts to DEBUG() right. We have got them wrong too often in the 
   past */
int  Debug1( char *, ... )
#ifdef __GNUC__
     __attribute__ ((format (printf, 1, 2)))
#endif
;
BOOL dbgtext( char *, ... )
#ifdef __GNUC__
     __attribute__ ((format (printf, 1, 2)))
#endif
;


/* If we have these macros, we can add additional info to the header. */
#ifdef HAVE_FILE_MACRO
#define FILE_MACRO (__FILE__)
#else
#define FILE_MACRO ("")
#endif

#ifdef HAVE_FUNCTION_MACRO
#define FUNCTION_MACRO  (__FUNCTION__)
#else
#define FUNCTION_MACRO  ("")
#endif

/* Debugging macros. 
 *  DEBUGLVL() - If level is <= the system-wide DEBUGLEVEL then generate a
 *               header using the default macros for file, line, and
 *               function name.
 *               Returns True if the debug level was <= DEBUGLEVEL.
 *               Example usage:
 *                 if( DEBUGLVL( 2 ) )
 *                   dbgtext( "Some text.\n" );
 *  DEGUG()    - Good old DEBUG().  Each call to DEBUG() will generate a new
 *               header *unless* the previous debug output was unterminated
 *               (i.e., no '\n').  See debug.c:dbghdr() for more info.
 *               Example usage:
 *                 DEBUG( 2, ("Some text.\n") );
 *  DEBUGADD() - If level <= DEBUGLEVEL, then the text is appended to the
 *               current message (i.e., no header).
 *               Usage:
 *                 DEBUGADD( 2, ("Some additional text.\n") );
 */
#define DEBUGLVL( level ) \
  ( (DEBUGLEVEL >= (level)) \
   && dbghdr( level, FILE_MACRO, FUNCTION_MACRO, (__LINE__) ) )

#if 0

#define DEBUG_OUT( level, body ) \
  ( ( DEBUGLEVEL >= (level) \
   && dbghdr( level, FUNCTION_MACRO ) ) \
      ? (void)(dbgtext body) : (void)0 )

#define DEBUGADD( level, body ) \
     ( (DEBUGLEVEL >= (level)) ? (void)(dbgtext body) : (void)0 )

#else

#define DEBUG_OUT( level, body ) \
  (void)( (DEBUGLEVEL >= (level)) \
          && (dbghdr( level, FILE_MACRO, FUNCTION_MACRO, (__LINE__) )) \
       && (dbgtext body) )

#define DEBUGADD( level, body ) \
  (void)( (DEBUGLEVEL >= (level)) && (dbgtext body) )

#endif

#undef DBUG_ENTER
#undef DBUG_PRINT
#undef DBUG_RETURN
#undef DBUG_VOID_RETURN

#define DBUG_ENTER(a) {DEBUG_OUT(0, ("%s:%d-->%s\n", __FILE__, __LINE__, a));}
#define DBUG_RETURN(a) { DEBUG_OUT(0, ("%s:%d<--return=%ld\n", __FILE__, __LINE__, (long)a)); return(a); }
#define DBUG_VOID_RETURN { DEBUG_OUT(0, ("%s:%d <\n", __FILE__, __LINE__)); return; }
#define DBUG_PRINT(a) {DEBUG_OUT(0, ("%s:%d : %s\n", __FILE__, __LINE__, a));}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * externs references 
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
extern FILE   *dbf;
extern pstring debugf;
extern BOOL    append_log;
extern int     DEBUGLEVEL;

extern void setup_logging( char *, BOOL );
extern void reopen_logs( void ) ;
extern void force_check_log_size( void );
extern BOOL need_to_check_log_size( void ) ;
extern void check_log_size( void ) ;
extern BOOL dbghdr( int level, char *file, char *func, int line );
extern void dbgflush( void );

extern void vperror (int level, char *format, ...) ;


#endif /* the end DEBUG_Hd */
