/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  NET_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  NET_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif


extern unsigned long net_addr_of(const char *host_name);
extern char *net_host_of(unsigned long ip_address);
extern int net_port_of(const char *serverName, const char *protocol);
extern char *net_server_of(int port, const char *protocol);

#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
