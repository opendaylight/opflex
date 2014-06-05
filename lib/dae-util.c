/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


/*
 *    dae_util.c - Daemon Utilities.
 *
 * Public Procedures:
 *    dae_monize() - makes a process a daemon.
 *
 **/

#include  <errno.h>			/* System error definitions. */
#include  <signal.h>			/* System signal definitions. */
#include  <stdio.h>			/* Standard I/O definitions. */
#include  <stdlib.h>			/* Standard C Library definitions. */
#include  <string.h>			/* C Library string functions. */
#include  <unistd.h>			/* UNIX-specific definitions. */
#include  <sys/stat.h>			/* File status definitions. */

#include "ansi-setup.h"         /* env setup */
#include "debug.h"              /* debug.c stuff */
#include "dae-util.h"

/*
 *    dae_monize () - Make a Process a Daemon.
 *    Function dae_monize() makes the current process a daemon.
 *
 *    Invocation:
 *        status = dae_monize (num_fds);
 *
 *    where
 *        <num_fds>	- I
 *            is the number of file descriptors, beginning with 0, to close.
 *        <status>	- O
 *            returns the status of making this process a daemon, zero if there
 *            were no errors and ERRNO otherwise.
 *
 */
int dae_monize(int num_fds)
{ 
    pid_t child_pid;
  
    /* Don't fall to pieces if the daemon attempts to read from or write to
    **	its controlling terminal. 
    */
#ifdef SIGTTIN
    signal (SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
    signal (SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTSTP
    signal (SIGTSTP, SIG_IGN);
#endif
  
    /* Put the daemon in the background by forking a child process and exiting
    **	the parent (i.e., the current process). 
    */
  
    child_pid = fork ();
    if (child_pid < 0) {
        DEBUG_OUT(0, ( "(dae_monize) Error forking background process.\nfork: "));
        return (errno);
    }
    if (child_pid > 0)  exit (0);	/* Terminate the parent process. */
  
    /* Disassociate the daemon from its controlling terminal. */
  
    if (setsid () == -1) {
        DEBUG_OUT( 0, 
                   ("(dae_monize) Error disassociating from the terminal.\nsetsid: "));
        return (errno);
    }
  
    /* Close any open files. */
  
    if ((num_fds < 0) || (num_fds > _NFILE))  num_fds = _NFILE;
    while (num_fds > 2) 
        close (--num_fds);
 
  
    /* Change the current directory to root to avoid preventing a mounted file
    **	system from being unmounted. 
    */
  
    if (chdir ("/")) {
        DEBUG_OUT(0, ("(dae_monize) Error changing directory to the root file system.\nchdir: "));
        return (errno);
    }
  
    /* Clear the inherited access mask for new files. */
  
    umask (0);
  
    /* Ignore exited children (zombies). */
  
    signal (SIGCLD, SIG_IGN);
  
    return (0);
  
}

#ifdef  TEST

/*
 *
 *    Program to test dae_monize().  Compile as follows:
 *        % cc -g -DTEST dae_monize.c -I<... includes ...>
 *
 *    Invocation:
 *        % a.out
 *
 */

int  main (int argc, char *argv[])
{
    vperror_print = 1;

    printf ("Parent PID: %lu\n", getpid ());
    dae_monize (-1);
    printf ("Daemon PID: %lu\n", getpid ());
    for (;;) {
        sleep (5);
    }

}
#endif  /* TEST */
