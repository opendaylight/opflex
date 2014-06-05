/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef  OPT_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  OPT_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif


/*
 *   Option Scan (Client View) and Definitions.
 */

typedef  struct  _OptContext  *OptContext;	/* Option scan context. */

/* Values returned by OPT_GET(). */

#define  OPTEND  0			/* End of command line. */
#define  OPTERR  -1			/* Invalid option or missing argument. */
#define  NONOPT  -2			/* Non-option argument. */


/*
 *  Miscellaneous declarations.
 */
extern  int  opt_util_debug;		/* Global debug switch (1/0 = yes/no). */

/*
 *   Function prototypes and external definitions for OPT_UTIL functions.
 */
extern int opt_create_argv (const char *program,
                            const char *command,
                            int *argc,
                            char *(*argv[]));
extern int opt_delete_argv(int argc, char *argv[]);
extern int opt_errors(OptContext scan, int display_flag);
extern int opt_get (OptContext scan, char **argument);
extern int opt_index (OptContext scan);
extern int opt_init (int argc, char *argv[], int is_list, ...);
extern char *opt_name (OptContext scan, int index);
extern int opt_reset (OptContext scan, int argc, char *argv[]);
extern int opt_set (OptContext scan, int new_index);
extern int opt_term (OptContext scan);

#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
