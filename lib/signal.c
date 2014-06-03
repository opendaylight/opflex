/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * Module Test Plan:
 *
 * Usage:
 *
 * Functions:  
 *
 * To Do:
 *
 * Revision History:  (latest changes at top)
 *    06232001 - dek - Created
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <pthread.h>


#include <gendcls.h> 
#include <debug.h>
#include <sig.h>

static int pgrp = 0;

pid_t parent_pid= 0;


/*
 * Catch child exits and reap the child zombie status.
 */
static void sig_cld(int signum)
{
    while (waitpid((pid_t)-1,(int *)NULL, WNOHANG) > 0)
        ;
  
    /*
     * Turns out it's *really* important not to
     * restore the signal handler here if we have real POSIX
     * signal handling. If we do, then we get the signal re-delivered
     * immediately - hey presto - instant loop ! DEK.
     */
  
    catch_signal(SIGCHLD, sig_cld);
}

/*
 * catch child exits - leave status;
 */
static void sig_cld_leave_status(int signum)
{
    /*
     * Turns out it's *really* important not to
     * restore the signal handler here if we have real POSIX
     * signal handling. If we do, then we get the signal re-delivered
     * immediately - hey presto - instant loop ! DEK.
     */  
    catch_signal(SIGCHLD, sig_cld_leave_status);
}

/*
 *  Block sigs.
 */
void block_signals(BOOL block,int signum)
{
    int block_mask;
    static int oldmask = 0;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,signum);
    sigprocmask(block?SIG_BLOCK:SIG_UNBLOCK,&set,NULL);

    block_mask = sigmask(signum);

    if (block) {
        oldmask = sigblock(block_mask);
    } else {
        sigsetmask(oldmask);
    }
}

/*
 * Catch a signal. This should implement the following semantics:
 *
 * 1) The handler remains installed after being called.
 * 2) The signal should be blocked during handler execution.
 */
void catch_signal(int signum,void (*handler)(int ))
{
    struct sigaction act;
  
    memset((char *)&act, 0, sizeof(act));
  
    act.sa_handler = handler;

    /*
     * We *want* SIGALRM to interrupt a system call.
     */
    if(signum != SIGALRM)
        act.sa_flags = SA_RESTART;
  
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask,signum);
    sigaction(signum,&act,NULL);
}

/*
 *  Ignore SIGCLD via whatever means is necessary for this OS.
 */
void catch_child(void) 
{
    catch_signal(SIGCHLD, sig_cld);
}

/*
 *  Catch SIGCLD but leave the child around so it's status can be reaped.
 */
void catch_child_leave_status(void)
{
    catch_signal(SIGCHLD, sig_cld_leave_status);
}

/*
 *  sig_term: Termination signal
 */
/* static int Restart = FALSE; */
/* static int Exit = FALSE; */
void sig_term(int sig) 
{

    DEBUG_OUT(0, ("caught SIGTERM, shutting down"));

#ifndef NO_KILLPG
    killpg(pgrp,SIGKILL);
#else
    kill(-pgrp,SIGKILL);
#endif /* NO_KILLPG */

    exit(-1);
}
/*
 *  bus_error: Termination signal
 */
void bus_error(void) 
{
    DEBUG_OUT(0, ("(bus_error): caught SIGBUS, dumping core\n"));
    exit(-1);
}

/*
 *  memory segmetation fault - 
 */
void seg_fault(void) 
{
    //stackinfo i;

    //DEBUG_OUT(0,("(seg_fault): caught SIGSEGV, dumping core:\n"));
    DEBUG_OUT(0,("(seg_fault): caught SIGSEGV, dumping core:thread=%ld\n", pthread_self())); 
    /* Obtain the call stack so that we can tell where the illegal memory
     * access came from.  This relies on the assumption that the stack area
     * for the signal handler is the same as the stack area for the main
     * process.
     */
    
    exit(-1);
}

/*
 * printstack - 
 * TODO - dkehn finish this !!
 */
/**void printstack (stackinfo *p)
{
  stackinfo s = *p;

  while (__mp_getframe(p) && (p->addr != NULL)) {
    DEBUG_OUT(0, ("\t %p ->", p->addr));
    
*/


/*
* zombie_killer()
* 
* zombie_killer() is the function call specified in 
* signal() to handle the death of the child process.
*
* This call is needed to receive the exit status of the
* child so that a 'zombie' process is not left behind.
*
* You would see this as a 'defunct' when 'ps -e' is
* used.
*
*/
void zombie_killer( int s )
{
    int	statusp;
    struct rusage rusage;
    int	child_pid;
      
    while((child_pid = wait3(&statusp,WNOHANG,&rusage)) > 0); 

    signal(SIGCHLD,zombie_killer);

} 

/*
 * @brief - sig_hup() - SIGHUP handler. Once issued the system need to 
 *          re-read the configuration information.
 *
 * @param0 <sig>                   - I
 *         signal number for the SIGHUP.
 *
 */
//volatile sig_atomic_t reload_after_sighup;
BOOL reload_after_sighup = False;

void sig_hup (int sig) 
{
    block_signals(True,SIGHUP);
    DEBUG_OUT(0,("Got SIGHUP.....\n"));
    
    /*
     * Fix from <don.kehn@lefthandnetworks.com> here.
     * We used to reload in the signal handler - this
     * is a *BIG* no-no.
     */
    reload_after_sighup = True;	
    
    block_signals(False,SIGHUP);
}
#ifndef DEBUG_SIGNALS
/*
 * @brief - sig_usr1() - SIGUSR1 handler. 
 *          This handler is indened for the child process to close down 
 *          their operations and issued DISCONNECTS to the attached clients
 *          and die. A re-reading of the configuration file is going to happen.
 *
 * @param0 <sig>                   - I
 *         signal number for the SIGHUP.
 *
 */

void sig_usr1 (int sig) 
{
    block_signals(True,SIGUSR1);
    DEBUG_OUT(0,("ndmpd Got SIGUSR1\n"));
    block_signals(False,SIGUSR1);
}
#endif

/*
 * setSignals - 
 */
void set_signals(void) 
{
    pgrp = getgid();
    signal(SIGSEGV,(void (*)(int))seg_fault);
    signal(SIGBUS, (void (*)(int))bus_error);
    signal(SIGTERM,(void (*)(int))sig_term);
    signal(SIGHUP, (void (*)(int))sig_hup);
#ifndef DEBUG_SIGNALS
    signal(SIGUSR1, (void (*)(int))sig_usr1);
#endif
}

