/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *    ANSI/Non-ANSI C Configuration.
 */

#ifndef  ANSI_SETUP_H		/* Has the file been INCLUDE'd already? */
#define  ANSI_SETUP_H  yes

/* Configure the handling of function prototypes. */

#ifndef P_
#    if (__STDC__ == 1) || defined(__cplusplus) || defined(vaxc)
#        define  P_(s)  s
#    else
#        define  P_(s)  ()
#        define  const
#        define  volatile
#    endif
#endif

/* Supply the ANSI strerror(3) function on systems that don't support it. */

#if __STDC__
    /* Okay! */
#elif defined(VMS)
#    define  strerror(code)  strerror (code, vaxc$errno)
#else
    extern  int  sys_nerr ;		/* Number of system error messages. */
    extern  char  *sys_errlist[] ;	/* Text of system error messages. */
#    define  strerror(code) \
        (((code < 0) || (code >= sys_nerr)) ? "unknown error code" \
                                            : sys_errlist[code])
#endif
#ifndef BOOL
typedef	int	BOOL;
#endif

#ifndef	False
	#define	False (0)
#endif
#ifndef	True
	#define	True (1)
#endif

#ifndef TRUE
#   define TRUE 1
#endif
#ifndef FALSE
#   define FALSE 0
#endif


#endif				/* If this file was not INCLUDE'd previously. */
