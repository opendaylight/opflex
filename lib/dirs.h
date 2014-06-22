/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef DIRS_H
#define DIRS_H 1

const char *ovs_sysconfdir(void); /* /usr/local/etc */
const char *ovs_pkgdatadir(void); /* /usr/local/share/openvswitch */
const char *ovs_rundir(void);     /* /usr/local/var/run/openvswitch */
const char *ovs_logdir(void);     /* /usr/local/var/log/openvswitch */
const char *ovs_dbdir(void);      /* /usr/local/etc/openvswitch */
const char *ovs_bindir(void);     /* /usr/local/bin */

#endif /* dirs.h */
