/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef	SIG_H
#define SIG_H

/*
 *        signal.h - SIG routines
 *
 * Considerations:
 *
 * To Do:
 *
 * Revision History:  (latest changes at top)
 *    06252001 - dek - created.
 *
 *
 */

extern BOOL reload_after_sighup;
extern pid_t parent_pid;

extern void block_signals( int, int ) ;
extern void catch_signal(int ,void (*handler)(int )) ;
extern void catch_child(void);
extern void catch_child_leave_status(void) ;
extern void set_signals(void) ;
extern void zombie_killer( int ) ;
extern void sig_usr1( int );
extern void sig_hup (int );

#endif
