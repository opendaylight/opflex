/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef DIRS_H
#define DIRS_H 1

const char *pag_sysconfdir(void); /* /usr/local/etc */
const char *pag_pkgdatadir(void); /* /usr/local/share/openvswitch */
const char *pag_rundir(void);     /* /usr/local/var/run/openvswitch */
const char *pag_logdir(void);     /* /usr/local/var/log/openvswitch */
const char *pag_dbdir(void);      /* /usr/local/etc/openvswitch */
const char *pag_bindir(void);     /* /usr/local/bin */

#endif /* dirs.h */
