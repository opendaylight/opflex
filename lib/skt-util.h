/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  SKT_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  SKT_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif

extern int skt_is_readable(const char *name, int fd);
extern int skt_is_up(const char *name, int fd);
extern int skt_is_writeable(const char *name, int fd);
extern char *skt_peer(const char *name, int fd);
extern int skt_port(const char *name, int fd);
extern  int  skt_set_buf(const char *name, int fd, int receive_size,
                         int send_size);
        
#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
