/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/******************************************************************************
 *									      *
 *				   N O T I C E				      *
 *									      *
 *		      Copyright Abandoned, 1987, Fred Fish		      *
 *									      *
 *									      *
 *	This previously copyrighted work has been placed into the  public     *
 *	domain	by  the  author  and  may be freely used for any purpose,     *
 *	private or commercial.						      *
 *									      *
 *	Because of the number of inquiries I was receiving about the  use     *
 *	of this product in commercially developed works I have decided to     *
 *	simply make it public domain to further its unrestricted use.	I     *
 *	specifically  would  be  most happy to see this material become a     *
 *	part of the standard Unix distributions by AT&T and the  Berkeley     *
 *	Computer  Science  Research Group, and a standard part of the GNU     *
 *	system from the Free Software Foundation.			      *
 *									      *
 *	I would appreciate it, as a courtesy, if this notice is  left  in     *
 *	all copies and derivative works.  Thank you.			      *
 *									      *
 *	The author makes no warranty of any kind  with	respect  to  this     *
 *	product  and  explicitly disclaims any implied warranties of mer-     *
 *	chantability or fitness for any particular purpose.		      *
 *									      *
 ******************************************************************************
 */


/*
 *  FILE
 *
 *	dbug.c	 runtime support routines for dbug package
 *
 *  SCCS
 *
 *	@(#)dbug.c	1.25	7/25/89
 *
 *  DESCRIPTION
 *
 *	These are the runtime support routines for the dbug package.
 *	The dbug package has two main components; the user include
 *	file containing various macro definitions, and the runtime
 *	support routines which are called from the macro expansions.
 *
 *	Externally visible functions in the runtime support module
 *	use the naming convention pattern "_db_xx...xx_", thus
 *	they are unlikely to collide with user defined function names.
 *
 *  AUTHOR(S)
 *
 *	Fred Fish		(base code)
 *	Enhanced Software Technologies, Tempe, AZ
 *	asuvax!mcdphx!estinc!fnf
 *
 *	Binayak Banerjee	(profiling enhancements)
 *	seismo!bpa!sjuvax!bbanerje
 *
 *	Michael Widenius:
 *	DBUG_DUMP	- To dump a pice of memory.
 *	PUSH_FLAG "O"	- To be used insted of "o" if we don't
 *			  want flushing (for slow systems)
 *	PUSH_FLAG "A"	- as 'O', but we will append to the out file instead
 *			  of creating a new one.
 *	Check of malloc on entry/exit (option "S")
 */

#ifndef _dbug_h
#define _dbug_h

#include <stdio.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C"
{
#endif

    extern char _dig_vec[];

#if !defined(DBUG_OFF) && !defined(_lint)
    extern int _db_on_, _no_db_;
    extern FILE *_db_fp_;
    extern char *_db_process_;
    extern int _db_keyword_(const char *keyword);
    extern void _db_setjmp_(void);
    extern void _db_longjmp_(void);
    extern void _db_push_(const char *control);
    extern void _db_pop_(void);
    extern void _db_enter_(const char *_func_, const char *_file_,
	uint _line_, const char **_sfunc_, const char **_sfile_,
	uint * _slevel_, char ***);
    extern void _db_return_(uint _line_, const char **_sfunc_,
	const char **_sfile_, uint * _slevel_);
    extern void _db_pargs_(uint _line_, const char *keyword);
    extern void _db_doprnt_(const char *format, ...);
    extern void _db_dump_(uint _line_, const char *keyword,
	const char *memory, uint length);
    extern void _db_lock_file(void);
    extern void _db_unlock_file(void);


#define DBUG_ENTER(a) const char *_db_func_, *_db_file_; uint _db_level_; \
	char **_db_framep_; \
	_db_enter_ (a,__FILE__,__LINE__,&_db_func_,&_db_file_,&_db_level_, \
		    &_db_framep_)
#define DBUG_LEAVE \
	(_db_return_ (__LINE__, &_db_func_, &_db_file_, &_db_level_))
#define DBUG_RETURN(a1) {DBUG_LEAVE; return(a1);}
#define DBUG_VOID_RETURN {DBUG_LEAVE; return;}
#define DBUG_EXECUTE(keyword,a1) \
	{if (_db_on_) {if (_db_keyword_ (keyword)) { a1 }}}
#define DBUG_PRINT(keyword,arglist) \
	{if (_db_on_) {_db_pargs_(__LINE__,keyword); _db_doprnt_ arglist;}}
#define DBUG_PUSH(a1) _db_push_ (a1)
#define DBUG_POP() _db_pop_ ()
#define DBUG_PROCESS(a1) (_db_process_ = a1)
#define DBUG_FILE (_db_fp_)
#define DBUG_SETJMP(a1) (_db_setjmp_ (), setjmp (a1))
#define DBUG_LONGJMP(a1,a2) (_db_longjmp_ (), longjmp (a1, a2))
#define DBUG_DUMP(keyword,a1,a2)\
	{if (_db_on_) {_db_dump_(__LINE__,keyword,a1,a2);}}
#define DBUG_IN_USE (_db_fp_ && _db_fp_ != stderr)
#define DEBUGGER_OFF _no_db_=1;_db_on_=0;
#define DEBUGGER_ON  _no_db_=0
#define DBUG_my_pthread_mutex_lock_FILE { _db_lock_file(); }
#define DBUG_my_pthread_mutex_unlock_FILE { _db_unlock_file(); }
#define DBUG_ASSERT(A) assert(A)
#else							   /* No debugger */

#define DBUG_ENTER(a1)
#define DBUG_RETURN(a1) return(a1)
#define DBUG_VOID_RETURN return
#define DBUG_LEAVE
#define DBUG_EXECUTE(keyword,a1) {}
#define DBUG_PRINT(keyword,arglist) {}
#define DBUG_PUSH(a1) {}
#define DBUG_POP() {}
#define DBUG_PROCESS(a1) {}
#define DBUG_FILE (stderr)
#define DBUG_SETJMP setjmp
#define DBUG_LONGJMP longjmp
#define DBUG_DUMP(keyword,a1,a2) {}
#define DBUG_IN_USE 0
#define DEBUGGER_OFF
#define DEBUGGER_ON
#define DBUG_my_pthread_mutex_lock_FILE
#define DBUG_my_pthread_mutex_unlock_FILE
#define DBUG_ASSERT(A) {}
#endif

#ifndef USE_VLOG
#define ENTER DBUG_ENTER
#define LEAVE(a) DBUG_LEAVE
#endif

#ifdef	__cplusplus
}
#endif
#endif
